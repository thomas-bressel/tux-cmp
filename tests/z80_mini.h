#pragma once

#include <array>
#include <cstdint>

// Just enough of a Z80 to run DepackZX0 and the program that calls it: the
// instructions they are made of, with the carry and zero flags they test,
// on 64K of memory. Anything else stops it.

struct MiniZ80 {
    std::array<uint8_t, 0x10000> memory{};
    uint16_t pc = 0, sp = 0;
    uint8_t a = 0, f = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    long steps = 0;
    bool unknown = false;

    static constexpr uint8_t kCarry = 0x01, kZero = 0x40;

    uint16_t bc() const { return static_cast<uint16_t>(b << 8 | c); }
    uint16_t de() const { return static_cast<uint16_t>(d << 8 | e); }
    uint16_t hl() const { return static_cast<uint16_t>(h << 8 | l); }
    void setBc(uint16_t v) { b = static_cast<uint8_t>(v >> 8), c = static_cast<uint8_t>(v); }
    void setDe(uint16_t v) { d = static_cast<uint8_t>(v >> 8), e = static_cast<uint8_t>(v); }
    void setHl(uint16_t v) { h = static_cast<uint8_t>(v >> 8), l = static_cast<uint8_t>(v); }
    bool carry() const { return f & kCarry; }
    bool zero() const { return f & kZero; }
    void flags(bool carryOut, bool zeroOut) { f = static_cast<uint8_t>((f & ~(kCarry | kZero)) | (carryOut ? kCarry : 0) | (zeroOut ? kZero : 0)); }

    uint8_t fetch() { return memory[pc++]; }
    uint16_t word() { const uint8_t low = fetch(); return static_cast<uint16_t>(fetch() << 8 | low); }
    void push(uint16_t v) { memory[--sp] = static_cast<uint8_t>(v >> 8); memory[--sp] = static_cast<uint8_t>(v); }
    uint16_t pop() { const uint8_t low = memory[sp++]; return static_cast<uint16_t>(memory[sp++] << 8 | low); }
    void jr(bool taken) { const auto d8 = static_cast<int8_t>(fetch()); if (taken) pc = static_cast<uint16_t>(pc + d8); }
    void call(bool taken) { const uint16_t to = word(); if (taken) { push(pc); pc = to; } }
    void ret(bool taken) { if (taken) pc = pop(); }
    uint8_t rl(uint8_t v) { const bool out = v & 0x80; v = static_cast<uint8_t>(v << 1 | (carry() ? 1 : 0)); flags(out, v == 0); return v; }
    uint8_t rr(uint8_t v) { const bool out = v & 1; v = static_cast<uint8_t>(v >> 1 | (carry() ? 0x80 : 0)); flags(out, v == 0); return v; }

    // Runs from `start` with a return address of `stop` on the stack, until
    // it returns there or has gone too far.
    bool run(uint16_t start, uint16_t stack, uint16_t stop, long limit = 50'000'000)
    {
        pc = start;
        sp = stack;
        push(stop);
        while (pc != stop && steps++ < limit && !unknown)
            step();
        return pc == stop && !unknown;
    }

    void step()
    {
        const uint8_t op = fetch();
        switch (op) {
        case 0x01: setBc(word()); break;                       // ld bc,nn
        case 0x11: setDe(word()); break;                       // ld de,nn
        case 0x21: setHl(word()); break;                       // ld hl,nn
        case 0x03: setBc(static_cast<uint16_t>(bc() + 1)); break;  // inc bc
        case 0x23: setHl(static_cast<uint16_t>(hl() + 1)); break;  // inc hl
        case 0x0C: c = static_cast<uint8_t>(c + 1); f = static_cast<uint8_t>((f & ~kZero) | (c == 0 ? kZero : 0)); break;  // inc c
        case 0x17: { const bool out = a & 0x80; a = static_cast<uint8_t>(a << 1 | (carry() ? 1 : 0)); f = static_cast<uint8_t>((f & ~kCarry) | (out ? kCarry : 0)); break; }  // rla
        case 0x18: jr(true); break;
        case 0x20: jr(!zero()); break;
        case 0x30: jr(!carry()); break;
        case 0x38: jr(carry()); break;
        case 0x19: { const int sum = hl() + de(); setHl(static_cast<uint16_t>(sum)); f = static_cast<uint8_t>((f & ~kCarry) | (sum > 0xFFFF ? kCarry : 0)); break; }  // add hl,de
        case 0x3E: a = fetch(); break;                         // ld a,n
        case 0x41: b = c; break;
        case 0x47: b = a; break;
        case 0x4E: c = memory[hl()]; break;
        case 0x4F: c = a; break;
        case 0x78: a = b; break;
        case 0x7E: a = memory[hl()]; break;
        case 0x87: { const bool out = a & 0x80; a = static_cast<uint8_t>(a << 1); flags(out, a == 0); break; }  // add a,a
        case 0x91: { const bool borrow = c > a; a = static_cast<uint8_t>(a - c); flags(borrow, a == 0); break; }  // sub c
        case 0xAF: a = 0; flags(false, true); break;          // xor a
        case 0xC1: setBc(pop()); break;
        case 0xC5: push(bc()); break;
        case 0xE1: setHl(pop()); break;
        case 0xE5: push(hl()); break;
        case 0xF1: { const uint16_t v = pop(); a = static_cast<uint8_t>(v >> 8); f = static_cast<uint8_t>(v); break; }  // pop af
        case 0xE3: { const uint16_t v = static_cast<uint16_t>(memory[sp] | memory[static_cast<uint16_t>(sp + 1)] << 8);  // ex (sp),hl
                     memory[sp] = l; memory[static_cast<uint16_t>(sp + 1)] = h; setHl(v); break; }
        case 0xC3: pc = word(); break;                        // jp nn
        case 0xC8: ret(zero()); break;
        case 0xC9: ret(true); break;
        case 0xD8: ret(carry()); break;
        case 0xCD: call(true); break;
        case 0xD4: call(!carry()); break;
        case 0xCB: {
            const uint8_t next = fetch();
            switch (next) {
            case 0x10: b = rl(b); break;
            case 0x11: c = rl(c); break;
            case 0x18: b = rr(b); break;
            case 0x19: c = rr(c); break;
            default: unknown = true;
            }
            break;
        }
        case 0xED:
            if (fetch() == 0xB0) {  // ldir
                do {
                    memory[de()] = memory[hl()];
                    setDe(static_cast<uint16_t>(de() + 1));
                    setHl(static_cast<uint16_t>(hl() + 1));
                    setBc(static_cast<uint16_t>(bc() - 1));
                } while (bc() != 0);
            } else {
                unknown = true;
            }
            break;
        default:
            unknown = true;
        }
    }
};
