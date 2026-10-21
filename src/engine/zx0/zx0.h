#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

// ZX0, Einar Saukas's compressor for the Z80, in the form PackZX0 writes it
// and its DepackZX0 routine reads it: ZX0's first format, the one its later
// versions call "classic" (zx0 -c). The compressor looks for the best way to
// put the file, as zx0 does without -q: what it writes is, byte for byte,
// what PackZX0 writes.

namespace tuxcmp::zx0 {

// The largest offset the format has: a match is found no further back.
constexpr int kMaxOffset = 32640;
// The largest file compressed here. Every byte is compared with the 32640
// before it, which takes some seconds for the CPC's 64K; a file of 1M takes
// minutes.
constexpr int kMaxInput = 1 << 20;

struct Packed {
    std::vector<uint8_t> data;
    // How far beyond the end of the decompressed data the compressed data
    // must end for the decompression to be done in place, the one over the
    // end of the other: the data are then read before they are written over.
    int delta = 0;
};

// Told how far the compression has gone, in bytes of the file; false stops it.
using Progress = std::function<bool(int done, int total)>;

// The file compressed. Nothing for an empty file, one longer than
// kMaxInput, or when `progress` says to stop.
std::optional<Packed> compress(std::span<const uint8_t> input, const Progress& progress = {});

// What compressed data decompress to; nothing if they are not compressed
// data, or decompress to more than `limit` bytes.
std::optional<std::vector<uint8_t>> decompress(std::span<const uint8_t> packed, size_t limit = kMaxInput);

// The 64K of the CPC's memory.
using Memory = std::array<uint8_t, 0x10000>;

// What DepackZX0 does in the machine's memory, instruction by instruction:
// it reads from `source` and writes from `destination`, both going round at
// the end of the 64K. When the compressed and the decompressed data lie over
// each other, what it decompresses is as the machine's would be.
struct Run {
    uint16_t sourceEnd = 0;       // the first byte after those read
    uint16_t destinationEnd = 0;  // the first byte after those written
    int written = 0;
};
// Nothing if more than `limit` bytes were written before the end of the
// data was found.
std::optional<Run> depack(Memory& memory, uint16_t source, uint16_t destination, int limit = 0x10000);

}  // namespace tuxcmp::zx0
