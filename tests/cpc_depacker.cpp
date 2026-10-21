// The program a loader calls, run on a Z80 of the test's own: assembled
// anywhere, it decompresses PackZX0's files and others to the bytes they
// were, goes back or jumps where it is told, and is the routine of
// DepackZX0.asm, instruction by instruction, as TuxAPE's assembler makes it.

#include <algorithm>
#include <random>

#include "check.h"
#include "cpc/depacker.h"
#include "golden.h"
#include "z80_mini.h"
#include "zx0/zx0.h"

namespace {

using tuxcmp::cpc::DepackerCall;

// The data decompressed by the program on the Z80; true if they came back.
bool runs(const std::vector<uint8_t>& original, const std::vector<uint8_t>& packed, const DepackerCall& call)
{
    MiniZ80 z80;
    std::fill(z80.memory.begin(), z80.memory.end(), uint8_t{0x76});
    std::copy(packed.begin(), packed.end(), z80.memory.begin() + call.source);
    const std::vector<uint8_t> program = tuxcmp::cpc::depackerProgram(call);
    std::copy(program.begin(), program.end(), z80.memory.begin() + call.origin);
    // Called from BASIC, the stack under &C000; back at &0000.
    if (!z80.run(call.origin, 0xC000, 0x0000))
        return false;
    // Nothing of the stack left on it.
    if (z80.sp != 0xC000)
        return false;
    return std::equal(original.begin(), original.end(), z80.memory.begin() + call.destination);
}

}  // namespace

int main()
{
    // The routine, as TuxAPE's assembler makes DepackZX0.asm at &A000
    // (checked by tools/cpc-check).
    const std::vector<uint8_t> routine = tuxcmp::cpc::depackRoutine(0xA000);
    const std::vector<uint8_t> assembled = {
        0x01, 0xFF, 0xFF, 0xC5, 0x03, 0x3E, 0x80, 0xCD, 0x37, 0xA0, 0xED, 0xB0, 0x87, 0x38, 0x0D, 0xCD,
        0x37, 0xA0, 0xE3, 0xE5, 0x19, 0xED, 0xB0, 0xE1, 0xE3, 0x87, 0x30, 0xEB, 0xCD, 0x37, 0xA0, 0x47,
        0xF1, 0xAF, 0x91, 0xC8, 0x4F, 0x78, 0x41, 0x4E, 0x23, 0xCB, 0x18, 0xCB, 0x19, 0xC5, 0x01, 0x01,
        0x00, 0xD4, 0x3F, 0xA0, 0x03, 0x18, 0xDB, 0x0C, 0x87, 0x20, 0x03, 0x7E, 0x23, 0x17, 0xD8, 0x87,
        0xCB, 0x11, 0xCB, 0x10, 0x18, 0xF2};
    CHECK_EQ(routine.size(), 70);
    CHECK(routine == assembled);
    // Elsewhere, only its calls change.
    const std::vector<uint8_t> moved = tuxcmp::cpc::depackRoutine(0x4000);
    int differ = 0;
    for (size_t i = 0; i < moved.size(); ++i)
        differ += moved[i] != routine[i];
    CHECK_EQ(differ, 4);

    // The program: HL and DE set, the routine called, back or on.
    const std::vector<uint8_t> back = tuxcmp::cpc::depackerProgram({0x8000, 0x9000, 0xC000, std::nullopt});
    CHECK(std::vector<uint8_t>(back.begin(), back.begin() + 10)
          == std::vector<uint8_t>({0x21, 0x00, 0x90, 0x11, 0x00, 0xC0, 0xCD, 0x0A, 0x80, 0xC9}));
    const std::vector<uint8_t> on = tuxcmp::cpc::depackerProgram({0x8000, 0x9000, 0x4000, uint16_t(0x4123)});
    CHECK(std::vector<uint8_t>(on.begin(), on.begin() + 12)
          == std::vector<uint8_t>({0x21, 0x00, 0x90, 0x11, 0x00, 0x40, 0xCD, 0x0C, 0x80, 0xC3, 0x23, 0x41}));
    CHECK_EQ(tuxcmp::cpc::depackerLength(false), 80);
    CHECK_EQ(tuxcmp::cpc::depackerLength(true), 82);
    CHECK_EQ(back.size(), 80);
    CHECK_EQ(on.size(), 82);

    // PackZX0's pictures, to the screen.
    for (const golden::Pair& pair : golden::kPairs) {
        const std::vector<uint8_t> original = golden::read(pair.original);
        const std::vector<uint8_t> packed = golden::read(pair.packed);
        CHECK(runs(original, packed, {0xA000, 0x4000, 0xC000, std::nullopt}));
        CHECK(runs(original, packed, {0x0400, 0x8000, 0xC000, std::nullopt}));
    }

    // Other data, compressed here, decompressed anywhere.
    std::mt19937 random(12);
    for (int round = 0; round < 6; ++round) {
        std::vector<uint8_t> original;
        const size_t size = 500 + random() % 12000;
        while (original.size() < size) {
            if (original.size() > 8 && random() % 2) {
                const size_t from = original.size() - 1 - random() % std::min<size_t>(original.size(), 3000);
                for (size_t i = 0, length = 1 + random() % 40; i < length; ++i)
                    original.push_back(original[from + i % (original.size() - from)]);
            } else {
                original.push_back(static_cast<uint8_t>(random() % (round + 2) * 37));
            }
        }
        const auto packed = tuxcmp::zx0::compress(original);
        CHECK(packed.has_value());
        if (packed)
            CHECK(runs(original, packed->data, {0x9F00, 0x2000, 0x5000, std::nullopt}));
    }

    // Told to run the data, it jumps there with the stack as it was given.
    {
        const std::vector<uint8_t> original(100, 0xC9);  // ret, a hundred times
        const auto packed = tuxcmp::zx0::compress(original);
        MiniZ80 z80;
        std::copy(packed->data.begin(), packed->data.end(), z80.memory.begin() + 0x6000);
        const std::vector<uint8_t> program = tuxcmp::cpc::depackerProgram({0x7000, 0x6000, 0x4000, uint16_t(0x4000)});
        std::copy(program.begin(), program.end(), z80.memory.begin() + 0x7000);
        CHECK(z80.run(0x7000, 0xC000, 0x0000));
        CHECK(z80.sp == 0xC000 && std::equal(original.begin(), original.end(), z80.memory.begin() + 0x4000));
    }

    // Its source names the same addresses.
    const std::string source = tuxcmp::cpc::depackerSource({0xA000, 0x4000, 0xC000, std::nullopt}, "SHOP.ZX0");
    CHECK(source.find("org &A000") != std::string::npos && source.find("ld hl,&4000") != std::string::npos);
    CHECK(source.find("ld de,&C000") != std::string::npos && source.find("\tret\n") != std::string::npos);
    CHECK(source.find("dzx0s_elias_backtrack:") != std::string::npos);
    const std::string jumping = tuxcmp::cpc::depackerSource({0xA000, 0x4000, 0x1000, uint16_t(0x1234)}, "GAME.ZX0");
    CHECK(jumping.find("jp &1234") != std::string::npos);
    return checkSummary("cpc_depacker");
}
