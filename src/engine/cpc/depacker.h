#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The Z80 program a loader calls: it decompresses data put in memory by the
// loader, with DepackZX0 (Demoniak's routine for PackZX0's files, in
// tests/golden/DepackZX0.asm), and then goes back to BASIC or runs what it
// decompressed.
//
//     ld hl,source       ; the compressed data
//     ld de,destination  ; where they go
//     call DepkZX0
//     ret                ; or: jp entry
//   DepkZX0: ...

namespace tuxcmp::cpc {

struct DepackerCall {
    uint16_t origin = 0;       // where the program is loaded
    uint16_t source = 0;       // where the compressed data are
    uint16_t destination = 0;  // where they go
    std::optional<uint16_t> entry;  // run there once they are; else back to BASIC
};

// DepackZX0 alone, assembled at `origin`: HL the compressed data, DE where
// they go. It uses AF, BC, DE, HL and four bytes of stack.
std::vector<uint8_t> depackRoutine(uint16_t origin);
// The program, the routine after its call.
std::vector<uint8_t> depackerProgram(const DepackerCall& call);
// Its length: the same whatever the addresses.
int depackerLength(bool withEntry);
// The program as a source for TuxAPE's assembler (and WinAPE's), which
// assembles to the bytes depackerProgram() gives.
std::string depackerSource(const DepackerCall& call, const std::string& packedName);

}  // namespace tuxcmp::cpc
