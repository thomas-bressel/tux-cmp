#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// The files of tests/golden: the pictures PackZX0 compressed, and what it
// made of them.

namespace golden {

inline std::string path(const char* name)
{
    return std::string(TUXCMP_GOLDEN_DIR) + "/" + name;
}

inline std::vector<uint8_t> read(const char* name)
{
    std::ifstream file(path(name), std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

// The pictures, each with the file PackZX0 made of it.
struct Pair {
    const char* original;
    const char* packed;
};
inline constexpr Pair kPairs[] = {
    {"SHOP.SCR", "SHOP.SCR.ZX0"},
    {"SCENARFR.SCR", "SCENARFR.SCR.ZX0"},
    {"meta.scr", "meta.scr.ZX0"},
};

}  // namespace golden
