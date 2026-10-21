#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// AMSDOS's files: their names on a disc, and the 128 bytes AMSDOS puts
// ahead of all but its ASCII files, which say what the file is, where it
// loads and how long it is.

namespace tuxcmp::cpc {

constexpr size_t kHeaderSize = 128;

enum FileType : uint8_t { kBasic = 0, kProtectedBasic = 1, kBinary = 2 };

struct Header {
    uint8_t type = kBinary;
    uint16_t loadAddress = 0;
    uint16_t entryAddress = 0;
    int length = 0;  // of what follows the header
};

// The header a file starts with, if it has one: its checksum tells.
std::optional<Header> readHeader(std::span<const uint8_t> file);
// The file without its header, if it has one.
std::span<const uint8_t> withoutHeader(std::span<const uint8_t> file);
std::vector<uint8_t> makeHeader(const std::string& name, const Header& header);
// A header followed by the data it is for.
std::vector<uint8_t> withHeader(const std::string& name, const Header& header, std::span<const uint8_t> data);

// A name as a disc has it: at most eight characters, a dot and at most
// three, in capitals, none of those CP/M keeps for itself. Nothing for a
// name that cannot be one.
std::optional<std::string> discName(const std::string& name);
// A name for a disc made from any other, a file's on the computer say, with
// another extension: "Shop screen.scr" and "ZX0" give "SHOPSCRE.ZX0".
std::string nameLike(const std::string& name, const std::string& extension);

}  // namespace tuxcmp::cpc
