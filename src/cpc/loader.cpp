#include "cpc/loader.h"

#include <algorithm>
#include <cstdio>
#include <memory>

#include "cpc/amsdos.h"
#include "cpc/basic.h"
#include "cpc/depacker.h"

namespace tuxcmp::cpc {

namespace {

// KM WAIT KEY.
constexpr int kWaitKey = 0xBB18;

bool overlap(int start, int end, int otherStart, int otherEnd)
{
    return start < otherEnd && otherStart < end;
}

// Where a file can be loaded: above the room BASIC needs, under HIMEM or in
// the screen's 16K.
bool loadable(int start, int length)
{
    return start >= kBasicRoom && start + length <= kMemoryEnd && !overlap(start, start + length, kHimem, kScreen);
}

int alignDown(int address)
{
    return address & ~0xF;
}

int alignUp(int address)
{
    return (address + 0xF) & ~0xF;
}

std::string hex(int value)
{
    char text[8];
    std::snprintf(text, sizeof text, "&%04X", value & 0xFFFF);
    return text;
}

std::string stemOf(const std::string& name)
{
    return name.substr(0, name.find('.'));
}

// All but what the decompression itself shows.
std::vector<Finding> placeFindings(const Loader& loader, int length, int packedLength)
{
    std::vector<Finding> found;
    const auto add = [&found](Problem problem, bool fatal) { found.push_back({problem, fatal}); };

    const auto basic = discName(loader.basicName), program = discName(loader.programName), packed = discName(loader.packedName);
    if (!basic || !program || !packed)
        add(Problem::BadName, true);
    else if (*basic == *program || *basic == *packed || *program == *packed)
        add(Problem::SameNames, true);

    const int start = loader.destination, end = start + length;
    if (end > kMemoryEnd)
        add(Problem::DestinationPastEnd, true);
    if (start < kLowest)
        add(Problem::DestinationTooLow, true);
    if (overlap(start, end, kHimem, kScreen))
        add(Problem::DestinationOnSystem, true);
    if (start < kBasicEnd)
        add(Problem::DestinationOnBasic, !loader.entry.has_value());
    else if (start < kBasicRoom && !loader.entry)
        add(Problem::DestinationUnkept, false);
    if (!loadable(loader.source, packedLength))
        add(Problem::SourceOutside, true);
    const int programLength = depackerLength(loader.entry.has_value());
    if (!loadable(loader.program, programLength))
        add(Problem::ProgramOutside, true);
    const int programEnd = loader.program + programLength;
    if (overlap(loader.program, programEnd, start, end) || overlap(loader.program, programEnd, loader.source, loader.source + packedLength))
        add(Problem::ProgramOverwritten, true);
    if (loader.entry && (*loader.entry < start || *loader.entry >= end))
        add(Problem::EntryOutside, false);
    if (start >= kScreen && loader.mode < 0 && !loader.entry)
        add(Problem::ScreenWithoutMode, false);
    return found;
}

}  // namespace

bool works(const std::vector<Finding>& findings)
{
    return std::none_of(findings.begin(), findings.end(), [](const Finding& finding) { return finding.fatal; });
}

void placeLoader(Loader& loader, int length, const zx0::Packed& packed)
{
    const int size = static_cast<int>(packed.data.size());
    const int programLength = depackerLength(loader.entry.has_value());
    const int start = loader.destination, end = start + length;
    const auto tryAt = [&](int source, int program) {
        if (source < 0 || program < 0 || source >= kMemoryEnd || program >= kMemoryEnd)
            return false;
        Loader trial = loader;
        trial.source = static_cast<uint16_t>(source);
        trial.program = static_cast<uint16_t>(program);
        if (!works(placeFindings(trial, length, size)))
            return false;
        loader = trial;
        return true;
    };
    // High under HIMEM; under the data; after them.
    const int high = alignDown(kHimem - size);
    if (tryAt(high, alignDown(high - programLength)))
        return;
    const int under = alignDown(start - size);
    if (tryAt(under, alignDown(under - programLength)))
        return;
    const int after = alignUp(end);
    if (tryAt(alignUp(after + programLength), after))
        return;
    // The compressed data over the end of the decompressed ones, as far
    // beyond them as ZX0 says.
    const int least = end + packed.delta - size;
    for (const int source : {alignUp(least), least, high})
        for (const int program : {alignDown(kHimem - programLength), alignDown(std::min(start, source) - programLength),
                                  alignUp(source + size), kScreen})
            if (tryAt(source, program))
                return;
    // Nowhere: the first place, for the user to see what is wrong with it.
    loader.source = static_cast<uint16_t>(std::max(high, 0));
    loader.program = static_cast<uint16_t>(std::max(alignDown(high - programLength), 0));
}

Loader suggestLoader(const std::string& packedName, uint16_t destination, std::optional<uint16_t> entry,
                     int length, const zx0::Packed& packed)
{
    Loader loader;
    loader.packedName = discName(packedName).value_or(packedName);
    const std::string stem = stemOf(loader.packedName);
    loader.basicName = stem + ".BAS";
    loader.programName = stem + ".BIN";
    loader.destination = destination;
    loader.entry = entry;
    // A picture: the screen in the mode most are drawn in, and a key
    // waited for, for it to be seen before BASIC writes over it.
    if (destination >= kScreen && !entry) {
        loader.mode = 1;
        loader.waitKey = true;
    }
    placeLoader(loader, length, packed);
    return loader;
}

std::vector<Finding> checkLoader(const Loader& loader, std::span<const uint8_t> original,
                                 std::span<const uint8_t> packed)
{
    const int length = static_cast<int>(original.size());
    std::vector<Finding> found = placeFindings(loader, length, static_cast<int>(packed.size()));
    const bool placed = std::none_of(found.begin(), found.end(), [](const Finding& finding) {
        return finding.problem == Problem::DestinationPastEnd || finding.problem == Problem::SourceOutside;
    });
    if (placed) {
        // The compressed file loaded, and decompressed by the Z80's steps.
        const auto memory = std::make_unique<zx0::Memory>();
        memory->fill(0);
        std::copy(packed.begin(), packed.end(), memory->begin() + loader.source);
        const auto run = zx0::depack(*memory, loader.source, loader.destination, length);
        if (!run || run->written != length
            || !std::equal(original.begin(), original.end(), memory->begin() + loader.destination))
            found.push_back({Problem::SourceOverwritten, true});
    }
    return found;
}

std::optional<uint16_t> memoryLimit(const Loader& loader)
{
    int lowest = kHimem;
    for (const int start : {static_cast<int>(loader.source), static_cast<int>(loader.program)})
        lowest = std::min(lowest, start);
    // The data's place is kept from BASIC too, if MEMORY can go that low.
    if (loader.destination >= kBasicRoom)
        lowest = std::min(lowest, static_cast<int>(loader.destination));
    if (lowest >= kHimem)
        return std::nullopt;
    return static_cast<uint16_t>(lowest - 1);
}

std::string loaderListing(const Loader& loader)
{
    std::vector<std::string> lines;
    lines.push_back("REM " + loader.packedName + " to " + hex(loader.destination) + ", by TuxCMP");
    if (loader.mode >= 0)
        lines.push_back("MODE " + std::to_string(loader.mode));
    if (const auto limit = memoryLimit(loader))
        lines.push_back("MEMORY " + hex(*limit));
    lines.push_back("LOAD\"" + loader.packedName + "\"," + hex(loader.source));
    lines.push_back("LOAD\"" + loader.programName + "\"," + hex(loader.program));
    lines.push_back("CALL " + hex(loader.program));
    if (loader.waitKey && !loader.entry)
        lines.push_back("CALL " + hex(kWaitKey));
    std::string listing;
    for (size_t i = 0; i < lines.size(); ++i)
        listing += std::to_string(10 * (i + 1)) + " " + lines[i] + "\n";
    return listing;
}

std::optional<LoaderFiles> makeLoaderFiles(const Loader& loader, std::span<const uint8_t> packed)
{
    const auto basic = tokenize(loaderListing(loader));
    if (!basic)
        return std::nullopt;
    LoaderFiles files;
    files.basic = withHeader(loader.basicName, {kBasic, kBasicStart, 0, 0}, *basic);
    const std::vector<uint8_t> program =
        depackerProgram({loader.program, loader.source, loader.destination, loader.entry});
    files.program = withHeader(loader.programName, {kBinary, loader.program, loader.program, 0}, program);
    files.packed = withHeader(loader.packedName, {kBinary, loader.source, 0, 0}, packed);
    return files;
}

}  // namespace tuxcmp::cpc
