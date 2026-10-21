// The compressor against PackZX0: each picture of tests/golden compressed
// to the very bytes PackZX0 made of it, and those bytes decompressed back
// to the picture, as data and in the CPC's memory.

#include <algorithm>
#include <memory>

#include "check.h"
#include "golden.h"
#include "zx0/zx0.h"

int main()
{
    for (const golden::Pair& pair : golden::kPairs) {
        const std::vector<uint8_t> original = golden::read(pair.original);
        const std::vector<uint8_t> made = golden::read(pair.packed);
        CHECK_EQ(original.size(), 16384);
        CHECK(!made.empty());

        const auto packed = tuxcmp::zx0::compress(original);
        CHECK(packed.has_value());
        if (!packed)
            continue;
        CHECK_EQ(packed->data.size(), made.size());
        CHECK(packed->data == made);
        if (packed->data != made)
            std::printf("  %s is not what PackZX0 made of it\n", pair.original);
        // The pictures need two bytes beyond their end to be decompressed
        // in place.
        CHECK_EQ(packed->delta, 2);

        CHECK(tuxcmp::zx0::decompress(made) == original);
        // As the CPC does: from &4000 to the screen.
        auto memory = std::make_unique<tuxcmp::zx0::Memory>();
        memory->fill(0);
        std::copy(made.begin(), made.end(), memory->begin() + 0x4000);
        const auto run = tuxcmp::zx0::depack(*memory, 0x4000, 0xC000);
        CHECK(run.has_value());
        CHECK(run && run->written == 16384 && run->destinationEnd == 0x0000);
        CHECK(run && run->sourceEnd == 0x4000 + made.size());
        CHECK(std::equal(original.begin(), original.end(), memory->begin() + 0xC000));
    }
    return checkSummary("zx0_golden");
}
