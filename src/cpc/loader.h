#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "zx0/zx0.h"

// A loader for a compressed file: a BASIC program that loads the file and
// the Z80 program that decompresses it, then calls that program, which puts
// the data back where they belong and runs them or goes back to BASIC.
//
//   10 REM SHOP.ZX0 to &C000, by TuxCMP
//   20 MODE 1
//   30 MEMORY &8D3F
//   40 LOAD"SHOP.ZX0",&8D90
//   50 LOAD"SHOP.BIN",&8D40
//   60 CALL &8D40
//   70 CALL &BB18
//
// The three go on the same disc; RUN"SHOP" runs the first.

namespace tuxcmp::cpc {

// The CPC's memory, as a loader may use it.
constexpr int kLowest = 0x0040;     // below, the restarts' RAM
constexpr int kBasicEnd = 0x0400;   // below, the BASIC loader itself
// Below, what BASIC needs to load a file: the loader, and above it the 4K
// it keeps free for the file's buffer. A MEMORY lower than this, and LOAD
// says "Memory full".
constexpr int kBasicRoom = 0x1300;
constexpr int kHimem = 0xA67C;      // from there, AMSDOS's and the firmware's, then the stack (HIMEM is &A67B)
constexpr int kScreen = 0xC000;
constexpr int kMemoryEnd = 0x10000;

struct Loader {
    std::string basicName;    // "SHOP.BAS"
    std::string programName;  // "SHOP.BIN"
    std::string packedName;   // "SHOP.ZX0"
    uint16_t destination = kScreen;  // where the data go back
    uint16_t source = 0;             // where the compressed file is loaded
    uint16_t program = 0;            // where the program that decompresses is
    std::optional<uint16_t> entry;   // where the data are run, once back; else back to BASIC
    int mode = -1;                   // the screen mode set first, or -1 to leave it
    bool waitKey = false;            // a key waited for at the end, before BASIC says Ready
};

// What would go wrong with a loader, or might.
enum class Problem {
    BadName,              // a name a disc cannot have
    SameNames,            // two of its three files have one name
    DestinationPastEnd,   // the data run past &FFFF
    DestinationTooLow,    // below &0040
    DestinationOnSystem,  // over AMSDOS, the firmware or the stack: &A67C to &BFFF
    DestinationOnBasic,   // over the loader itself, below &0400: only with an entry, never coming back
    DestinationUnkept,    // under &1300, where MEMORY cannot keep the data from BASIC once the loader is over
    SourceOutside,        // the compressed file not where it can be loaded
    ProgramOutside,       // nor the program
    ProgramOverwritten,   // the program over the data or the compressed file
    SourceOverwritten,    // the compressed data written over before they are read
    EntryOutside,         // the entry not in the data decompressed
    ScreenWithoutMode,    // a picture shown without MODE: it may be shifted if the screen has scrolled
};

struct Finding {
    Problem problem;
    bool fatal;  // the loader would not work
};

// The loader's names from the compressed file's, and its addresses chosen
// where its parts do not lie over one another: high under HIMEM if they
// fit there, else under the data, else after them, else the compressed
// data over the end of the decompressed, `delta` beyond them at least, and
// the program where there is room, the screen's memory if need be.
Loader suggestLoader(const std::string& packedName, uint16_t destination, std::optional<uint16_t> entry,
                     int length, const zx0::Packed& packed);
// Where the compressed file and the program go, for the destination and
// entry the loader has.
void placeLoader(Loader& loader, int length, const zx0::Packed& packed);

// What is wrong with a loader for these data, compressed to `packed`: the
// decompression is run as the Z80 does it, in the 64K, to see the data come
// back whole.
std::vector<Finding> checkLoader(const Loader& loader, std::span<const uint8_t> original,
                                 std::span<const uint8_t> packed);
bool works(const std::vector<Finding>& findings);

// The BASIC program, as text, and the value of its MEMORY: under all that
// is loaded below HIMEM, and the data if MEMORY can keep them from BASIC;
// nothing if that is HIMEM itself.
std::string loaderListing(const Loader& loader);
std::optional<uint16_t> memoryLimit(const Loader& loader);

// The files to put on the disc, each with its AMSDOS header.
struct LoaderFiles {
    std::vector<uint8_t> basic;
    std::vector<uint8_t> program;
    std::vector<uint8_t> packed;  // the compressed data, loading where the loader loads them
};
std::optional<LoaderFiles> makeLoaderFiles(const Loader& loader, std::span<const uint8_t> packed);

}  // namespace tuxcmp::cpc
