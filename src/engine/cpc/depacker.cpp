#include "cpc/depacker.h"

#include <cstdio>
#include <map>
#include <string_view>

namespace tuxcmp::cpc {

namespace {

// Enough of an assembler for a routine: bytes, and the absolute calls and
// relative jumps to its labels.
class Code {
public:
    explicit Code(uint16_t origin)
        : origin_(origin)
    {
    }

    void bytes(std::initializer_list<uint8_t> values) { out_.insert(out_.end(), values); }
    void word(uint16_t value) { bytes({static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)}); }
    void label(std::string_view name) { labels_[std::string(name)] = here(); }
    // call nn, call nc,nn, jp nn...: the opcode and a label's address.
    void absolute(uint8_t opcode, std::string_view target)
    {
        bytes({opcode});
        fixups_.push_back({out_.size(), std::string(target), false});
        word(0);
    }
    // jr e, jr nz,e...: the opcode and the distance to a label.
    void relative(uint8_t opcode, std::string_view target)
    {
        bytes({opcode});
        fixups_.push_back({out_.size(), std::string(target), true});
        bytes({0});
    }

    std::vector<uint8_t> finish()
    {
        for (const Fixup& fixup : fixups_) {
            const uint16_t target = labels_.at(fixup.label);
            if (fixup.relative) {
                const int distance = target - (origin_ + static_cast<int>(fixup.at) + 1);
                out_[fixup.at] = static_cast<uint8_t>(distance);
            } else {
                out_[fixup.at] = static_cast<uint8_t>(target);
                out_[fixup.at + 1] = static_cast<uint8_t>(target >> 8);
            }
        }
        return out_;
    }

private:
    struct Fixup {
        size_t at;
        std::string label;
        bool relative;
    };

    uint16_t origin_;
    std::vector<uint8_t> out_;
    std::map<std::string, uint16_t> labels_;
    std::vector<Fixup> fixups_;

    uint16_t here() const { return static_cast<uint16_t>(origin_ + out_.size()); }
};

constexpr uint8_t kCall = 0xCD, kCallNc = 0xD4, kJr = 0x18, kJrNz = 0x20, kJrNc = 0x30, kJrC = 0x38;

// DepackZX0, instruction by instruction, as the source has it.
void routine(Code& code)
{
    code.label("DepkZX0");
    code.bytes({0x01, 0xFF, 0xFF});  // ld bc,#ffff      preserve default offset 1
    code.bytes({0xC5});              // push bc
    code.bytes({0x03});              // inc bc
    code.bytes({0x3E, 0x80});        // ld a,#80
    code.label("dzx0s_literals");
    code.absolute(kCall, "dzx0s_elias");  // obtain length
    code.bytes({0xED, 0xB0});        // ldir             copy literals
    code.bytes({0x87});              // add a,a          copy from last offset or new offset?
    code.relative(kJrC, "dzx0s_new_offset");
    code.absolute(kCall, "dzx0s_elias");  // obtain length
    code.label("dzx0s_copy");
    code.bytes({0xE3});              // ex (sp),hl       preserve source, restore offset
    code.bytes({0xE5});              // push hl          preserve offset
    code.bytes({0x19});              // add hl,de        calculate destination - offset
    code.bytes({0xED, 0xB0});        // ldir             copy from offset
    code.bytes({0xE1});              // pop hl           restore offset
    code.bytes({0xE3});              // ex (sp),hl       preserve offset, restore source
    code.bytes({0x87});              // add a,a          copy from literals or new offset?
    code.relative(kJrNc, "dzx0s_literals");
    code.label("dzx0s_new_offset");
    code.absolute(kCall, "dzx0s_elias");  // obtain offset MSB
    code.bytes({0x47});              // ld b,a
    code.bytes({0xF1});              // pop af           discard last offset
    code.bytes({0xAF});              // xor a            adjust for negative offset
    code.bytes({0x91});              // sub c
    code.bytes({0xC8});              // ret z            no more bytes: done
    code.bytes({0x4F});              // ld c,a
    code.bytes({0x78});              // ld a,b
    code.bytes({0x41});              // ld b,c
    code.bytes({0x4E});              // ld c,(hl)        obtain offset LSB
    code.bytes({0x23});              // inc hl
    code.bytes({0xCB, 0x18});        // rr b             last offset bit becomes first length bit
    code.bytes({0xCB, 0x19});        // rr c
    code.bytes({0xC5});              // push bc          preserve new offset
    code.bytes({0x01, 0x01, 0x00});  // ld bc,1          obtain length
    code.absolute(kCallNc, "dzx0s_elias_backtrack");
    code.bytes({0x03});              // inc bc
    code.relative(kJr, "dzx0s_copy");
    code.label("dzx0s_elias");
    code.bytes({0x0C});              // inc c            interlaced Elias gamma coding
    code.label("dzx0s_elias_loop");
    code.bytes({0x87});              // add a,a
    code.relative(kJrNz, "dzx0s_elias_skip");
    code.bytes({0x7E});              // ld a,(hl)        load another group of 8 bits
    code.bytes({0x23});              // inc hl
    code.bytes({0x17});              // rla
    code.label("dzx0s_elias_skip");
    code.bytes({0xD8});              // ret c
    code.label("dzx0s_elias_backtrack");
    code.bytes({0x87});              // add a,a
    code.bytes({0xCB, 0x11});        // rl c
    code.bytes({0xCB, 0x10});        // rl b
    code.relative(kJr, "dzx0s_elias_loop");
}

std::string hex(unsigned value)
{
    char text[8];
    std::snprintf(text, sizeof text, "&%04X", value & 0xFFFF);
    return text;
}

}  // namespace

std::vector<uint8_t> depackRoutine(uint16_t origin)
{
    Code code(origin);
    routine(code);
    return code.finish();
}

std::vector<uint8_t> depackerProgram(const DepackerCall& call)
{
    Code code(call.origin);
    code.bytes({0x21});  // ld hl,source
    code.word(call.source);
    code.bytes({0x11});  // ld de,destination
    code.word(call.destination);
    code.absolute(kCall, "DepkZX0");
    if (call.entry) {
        code.bytes({0xC3});  // jp entry
        code.word(*call.entry);
    } else {
        code.bytes({0xC9});  // ret
    }
    routine(code);
    return code.finish();
}

int depackerLength(bool withEntry)
{
    DepackerCall call;
    if (withEntry)
        call.entry = 0;
    return static_cast<int>(depackerProgram(call).size());
}

std::string depackerSource(const DepackerCall& call, const std::string& packedName)
{
    std::string source;
    source += "; Decompresses " + packedName + ", loaded at " + hex(call.source) + ", to " + hex(call.destination) + ".\n";
    source += "; Made by TuxCMP. DepackZX0 is Demoniak's routine for PackZX0's files.\n\n";
    source += "\torg " + hex(call.origin) + "\n\n";
    source += "\tld hl," + hex(call.source) + "\t\t; the compressed data\n";
    source += "\tld de," + hex(call.destination) + "\t\t; where they go\n";
    source += "\tcall DepkZX0\n";
    source += call.entry ? "\tjp " + hex(*call.entry) + "\t\t; what they are, run\n" : std::string("\tret\n");
    source += R"(
; Decompression
; HL = source address (compressed)
; DE = destination address
DepkZX0:
	ld bc,#ffff		; preserve default offset 1
	push bc
	inc bc
	ld a,#80
dzx0s_literals:
	call dzx0s_elias	; obtain length
	ldir			; copy literals
	add a,a			; copy from last offset or new offset?
	jr c,dzx0s_new_offset
	call dzx0s_elias	; obtain length
dzx0s_copy:
	ex (sp),hl		; preserve source, restore offset
	push hl			; preserve offset
	add hl,de		; calculate destination - offset
	ldir			; copy from offset
	pop hl			; restore offset
	ex (sp),hl		; preserve offset, restore source
	add a,a			; copy from literals or new offset?
	jr nc,dzx0s_literals
dzx0s_new_offset:
	call dzx0s_elias	; obtain offset MSB
	ld b,a
	pop af			; discard last offset
	xor a			; adjust for negative offset
	sub c
	ret z			; no more bytes: done
	ld c,a
	ld a,b
	ld b,c
	ld c,(hl)		; obtain offset LSB
	inc hl
	rr b			; last offset bit becomes first length bit
	rr c
	push bc			; preserve new offset
	ld bc,1			; obtain length
	call nc,dzx0s_elias_backtrack
	inc bc
	jr dzx0s_copy
dzx0s_elias:
	inc c			; interlaced Elias gamma coding
dzx0s_elias_loop:
	add a,a
	jr nz,dzx0s_elias_skip
	ld a,(hl)		; load another group of 8 bits
	inc hl
	rla
dzx0s_elias_skip:
	ret c
dzx0s_elias_backtrack:
	add a,a
	rl c
	rl b
	jr dzx0s_elias_loop
)";
    return source;
}

}  // namespace tuxcmp::cpc
