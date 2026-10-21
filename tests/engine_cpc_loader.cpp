// Loaders: where their parts go when nothing is said, high under HIMEM,
// under the data, after them or over their end, in the screen if need be;
// what is wrong with one put
// where it cannot work, found before it is written; the BASIC listing; the
// three files with their headers; and the whole run on a Z80 of the test's
// own, as the loader runs it.

#include <algorithm>
#include <random>

#include "check.h"
#include "cpc/amsdos.h"
#include "cpc/basic.h"
#include "cpc/depacker.h"
#include "cpc/loader.h"
#include "golden.h"
#include "z80_mini.h"
#include "zx0/zx0.h"

namespace {

using namespace tuxcmp::cpc;

bool has(const std::vector<Finding>& findings, Problem problem, bool fatal)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.problem == problem && finding.fatal == fatal; });
}

std::vector<uint8_t> program(size_t size)
{
    std::vector<uint8_t> bytes;
    for (size_t i = 0; bytes.size() < size; ++i)
        bytes.push_back(static_cast<uint8_t>((i * 7) % 61 + (i / 300) % 5));
    return bytes;
}

// What the loader does once BASIC has loaded its two files: the program
// called, on the Z80.
bool loaded(const Loader& loader, const std::vector<uint8_t>& original, const std::vector<uint8_t>& packed)
{
    const auto files = makeLoaderFiles(loader, packed);
    if (!files)
        return false;
    MiniZ80 z80;
    // LOAD puts a file where its header says, without the header.
    for (const std::vector<uint8_t>* file : {&files->packed, &files->program}) {
        const auto header = readHeader(*file);
        if (!header)
            return false;
        std::copy(file->begin() + kHeaderSize, file->end(), z80.memory.begin() + header->loadAddress);
    }
    // Back to BASIC at &0000, or on to the data, where it stops here.
    if (!z80.run(loader.program, 0xC000, loader.entry.value_or(0x0000)))
        return false;
    return std::equal(original.begin(), original.end(), z80.memory.begin() + loader.destination);
}

}  // namespace

int main()
{
    const std::vector<uint8_t> shop = golden::read("SHOP.SCR");
    const tuxcmp::zx0::Packed shopPacked{golden::read("SHOP.SCR.ZX0"), 2};
    const int shopSize = static_cast<int>(shopPacked.data.size());

    // A picture: to the screen, its parts high under HIMEM, the program
    // under the compressed file; MODE 1 and a key waited for.
    {
        const Loader loader = suggestLoader("shop.zx0", 0xC000, std::nullopt, 16384, shopPacked);
        CHECK(loader.packedName == "SHOP.ZX0" && loader.basicName == "SHOP.BAS" && loader.programName == "SHOP.BIN");
        CHECK_EQ(loader.source, (kHimem - shopSize) & ~0xF);
        CHECK_EQ(loader.program, (loader.source - 80) & ~0xF);
        CHECK(loader.source + shopSize <= kHimem && loader.program + 80 <= loader.source);
        CHECK(loader.mode == 1 && loader.waitKey && !loader.entry);
        CHECK(checkLoader(loader, shop, shopPacked.data).empty());
        CHECK(memoryLimit(loader) == loader.program - 1);
        char text[300];
        std::snprintf(text, sizeof text,
                      "10 REM SHOP.ZX0 to &C000, by TuxCMP\n20 MODE 1\n30 MEMORY &%04X\n40 LOAD\"SHOP.ZX0\",&%04X\n"
                      "50 LOAD\"SHOP.BIN\",&%04X\n60 CALL &%04X\n70 CALL &BB18\n",
                      loader.program - 1, loader.source, loader.program, loader.program);
        CHECK(loaderListing(loader) == text);
        if (loaderListing(loader) != text)
            std::printf("%s", loaderListing(loader).c_str());

        const auto files = makeLoaderFiles(loader, shopPacked.data);
        CHECK(files.has_value());
        if (files) {
            const auto basic = readHeader(files->basic);
            CHECK(basic && basic->type == kBasic && basic->loadAddress == kBasicStart);
            CHECK(basic && tokenize(loaderListing(loader)) == std::vector<uint8_t>(files->basic.begin() + kHeaderSize, files->basic.end()));
            CHECK(std::string(files->basic.begin() + 1, files->basic.begin() + 12) == "SHOP    BAS");
            const auto code = readHeader(files->program);
            CHECK(code && code->type == kBinary && code->loadAddress == loader.program && code->entryAddress == loader.program);
            CHECK(code && code->length == 80);
            const auto data = readHeader(files->packed);
            CHECK(data && data->type == kBinary && data->loadAddress == loader.source && data->length == shopSize);
            CHECK(std::equal(shopPacked.data.begin(), shopPacked.data.end(), files->packed.begin() + kHeaderSize));
        }
        CHECK(loaded(loader, shop, shopPacked.data));
    }

    // A program of 30K at &1000, run at &1000: its parts go above it,
    // under HIMEM; BASIC is not gone back to.
    {
        const std::vector<uint8_t> code = program(30000);
        const auto packed = tuxcmp::zx0::compress(code);
        CHECK(packed.has_value());
        const Loader loader = suggestLoader("GAME.ZX0", 0x1000, uint16_t(0x1000), 30000, *packed);
        CHECK(loader.mode == -1 && !loader.waitKey && loader.entry == 0x1000);
        CHECK(loader.source >= 0x1000 + 30000 && loader.program >= 0x1000 + 30000);
        CHECK(checkLoader(loader, code, packed->data).empty());
        // Under &1300, the data cannot be kept from BASIC: they need not be.
        CHECK(memoryLimit(loader) == std::min(loader.source, loader.program) - 1);
        CHECK(loaderListing(loader).find("CALL &BB18") == std::string::npos);
        CHECK(loaded(loader, code, packed->data));
    }

    // 40K at &0400, to &A640: there is room for their parts neither above
    // nor below; the compressed data go over the end of the decompressed,
    // and the program, with no room left under HIMEM, in the screen.
    {
        std::vector<uint8_t> code = program(0xA640 - 0x0400);
        std::mt19937 random(3);
        for (size_t i = 0; i < code.size(); ++i)
            if (i % 50 < 35)
                code[i] = static_cast<uint8_t>(random());
        const auto packed = tuxcmp::zx0::compress(code);
        CHECK(packed.has_value());
        const Loader loader = suggestLoader("BIG.ZX0", 0x0400, uint16_t(0x0400), static_cast<int>(code.size()), *packed);
        const int end = 0xA640;
        CHECK(loader.source >= kBasicRoom && loader.source < end);
        CHECK(loader.source + packed->data.size() >= static_cast<size_t>(end + packed->delta));
        CHECK_EQ(loader.program, kScreen);
        CHECK(checkLoader(loader, code, packed->data).empty());
        CHECK(memoryLimit(loader) == loader.source - 1);
        CHECK(loaded(loader, code, packed->data));
    }

    // 40K that hardly compress: there is no room for them under HIMEM
    // above what BASIC needs. Their loader is still made, to be told why
    // it cannot work.
    {
        std::vector<uint8_t> code = program(0xA000 - 0x0400);
        std::mt19937 random(3);
        for (size_t i = 0; i < code.size(); ++i)
            if (i % 50 < 45)
                code[i] = static_cast<uint8_t>(random());
        const auto packed = tuxcmp::zx0::compress(code);
        CHECK(packed.has_value());
        const Loader loader = suggestLoader("NOISE.ZX0", 0x0400, uint16_t(0x0400), static_cast<int>(code.size()), *packed);
        CHECK(has(checkLoader(loader, code, packed->data), Problem::SourceOutside, true));
    }

    // 30K that compress well, at &2000: no room above, nor below; the
    // compressed data over the end of the decompressed, as near as they
    // can be, the program high.
    {
        std::vector<uint8_t> code = program(30000);
        std::mt19937 random(4);
        for (size_t i = 0; i < code.size(); ++i)
            if (i % 50 < 20)
                code[i] = static_cast<uint8_t>(random());
        const auto packed = tuxcmp::zx0::compress(code);
        CHECK(packed.has_value());
        const Loader loader = suggestLoader("MID.ZX0", 0x2000, uint16_t(0x2000), 30000, *packed);
        const int end = 0x2000 + 30000;
        const int least = end + packed->delta - static_cast<int>(packed->data.size());
        CHECK(loader.source >= least && loader.source < least + 16);
        CHECK_EQ(loader.program, (kHimem - 82) & ~0xF);
        CHECK(checkLoader(loader, code, packed->data).empty());
        CHECK(loaded(loader, code, packed->data));
        // Lower, it would be written over before it is read.
        Loader lower = loader;
        lower.source = static_cast<uint16_t>(least - 0x40);
        CHECK(has(checkLoader(lower, code, packed->data), Problem::SourceOverwritten, true));
    }

    // What is wrong, found.
    {
        Loader good = suggestLoader("SHOP.ZX0", 0xC000, std::nullopt, 16384, shopPacked);
        Loader bad = good;
        bad.basicName = "TOOLONGNAME.BAS";
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::BadName, true));
        bad = good;
        bad.programName = "SHOP.BAS";
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::SameNames, true));
        bad = good;
        bad.destination = 0xD000;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::DestinationPastEnd, true));
        bad = good;
        bad.destination = 0xA000;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::DestinationOnSystem, true));
        bad = good;
        bad.destination = 0x0010;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::DestinationTooLow, true));
        bad.destination = 0x0100;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::DestinationOnBasic, true));
        bad.entry = 0x0100;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::DestinationOnBasic, false));
        bad = good;
        bad.source = 0xB000;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::SourceOutside, true));
        bad = good;
        bad.program = 0x0100;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::ProgramOutside, true));
        // Under &1300 BASIC cannot load it.
        bad.program = 0x1200;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::ProgramOutside, true));
        bad = good;
        bad.destination = 0x0800;
        bad.mode = -1;
        const auto unkept = checkLoader(bad, shop, shopPacked.data);
        CHECK(has(unkept, Problem::DestinationUnkept, false) && works(unkept));
        CHECK(memoryLimit(bad) == good.program - 1);
        bad = good;
        bad.program = static_cast<uint16_t>(good.source + 10);
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::ProgramOverwritten, true));
        bad = good;
        bad.entry = 0x1000;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::EntryOutside, false));
        bad = good;
        bad.mode = -1;
        const auto shifted = checkLoader(bad, shop, shopPacked.data);
        CHECK(has(shifted, Problem::ScreenWithoutMode, false) && works(shifted));
        CHECK(loaderListing(bad).find("MODE") == std::string::npos);
        // The compressed file loaded over the picture's first bytes: it
        // is written over before it is read.
        bad = good;
        bad.source = 0xC000;
        CHECK(has(checkLoader(bad, shop, shopPacked.data), Problem::SourceOverwritten, true));
        // All loaded in the screen: no MEMORY.
        Loader screen = good;
        screen.destination = 0x4000;
        screen.program = 0xC000;
        screen.source = 0xC100;
        CHECK(checkLoader(screen, shop, shopPacked.data).empty());
        CHECK(memoryLimit(screen) == 0x3FFF);
        screen.destination = 0x0200;
        screen.entry = 0x0200;
        CHECK(!memoryLimit(screen).has_value());
        CHECK(loaderListing(screen).find("MEMORY") == std::string::npos);
    }

    // ---- Where the data go back, and are run ----
    {
        CHECK_EQ(defaultDestination(std::nullopt, 0x4000, "BIN"), 0xC000);
        CHECK_EQ(defaultDestination(std::nullopt, 0x3F00, "scr"), 0xC000);
        CHECK_EQ(defaultDestination(std::nullopt, 0x3F00, "BIN"), 0x4000);
        CHECK_EQ(defaultDestination(std::nullopt, 0x4001, "SCR"), 0x4000);
        const Header game{kBinary, 0x4000, 0x4010, 3000};
        CHECK_EQ(defaultDestination(game, 3000, "BIN"), 0x4000);
        CHECK(defaultEntry(game, 3000) == 0x4010);
        CHECK(!defaultEntry(std::nullopt, 3000));
        // Not among the data, a picture, a BASIC program: not run.
        CHECK(!defaultEntry(game, 0x10));
        CHECK(!defaultEntry(Header{kBinary, 0xC000, 0xC000, 0x4000}, 0x4000));
        CHECK(!defaultEntry(Header{kBasic, 0x170, 0x170, 100}, 100));
    }
    return checkSummary("engine_cpc_loader");
}
