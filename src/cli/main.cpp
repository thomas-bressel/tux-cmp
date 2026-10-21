// tuxcmp: compresses a file with ZX0 as PackZX0 does, into a file of the
// same name followed by .ZX0; with -d, decompresses one.
//
//   tuxcmp SHOP.SCR                 SHOP.SCR.ZX0
//   tuxcmp -o SHOP.ZX0 SHOP.SCR     SHOP.ZX0
//   tuxcmp -d SHOP.SCR.ZX0          SHOP.SCR
//
// A file with an AMSDOS header is compressed without it, and the compressed
// file is given one, as PackZX0 gives it: the original's type and
// addresses, with the compressed length.

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "base/version.h"
#include "cpc/amsdos.h"
#include "zx0/zx0.h"

namespace {

std::optional<std::vector<uint8_t>> readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

bool writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

// The name a header gives a file written to `path`: "SHOP.SCR.ZX0" is
// SHOP.ZX0 on a disc.
std::string headerName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.rfind('.');
    return tuxcmp::cpc::nameLike(name, dot == std::string::npos ? std::string() : name.substr(dot + 1));
}

void usage()
{
    std::printf("TuxCMP %s, ZX0 compressor for the Amstrad CPC, after PackZX0.\n\n"
                "  tuxcmp [-o OUTPUT] FILE       compresses FILE into FILE.ZX0, or OUTPUT\n"
                "  tuxcmp -d [-o OUTPUT] FILE    decompresses FILE, into FILE without .ZX0, or OUTPUT\n\n"
                "A file with an AMSDOS header is compressed without it; the compressed file\n"
                "then has one, with the original's type and addresses.\n",
                tuxcmp::versionString());
}

}  // namespace

int main(int argc, char* argv[])
{
    bool unpack = false;
    std::string output, input;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "-help" || arg == "--help") {
            usage();
            return 0;
        }
        if (arg == "-d") {
            unpack = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            return 2;
        } else if (input.empty()) {
            input = arg;
        } else {
            std::fprintf(stderr, "One file at a time: %s\n", arg.c_str());
            return 2;
        }
    }
    if (input.empty()) {
        usage();
        return 2;
    }
    const auto file = readFile(input);
    if (!file) {
        std::fprintf(stderr, "Cannot read %s\n", input.c_str());
        return 1;
    }
    const auto header = tuxcmp::cpc::readHeader(*file);
    const std::span<const uint8_t> data = tuxcmp::cpc::withoutHeader(*file);

    std::vector<uint8_t> result;
    if (unpack) {
        const auto unpacked = tuxcmp::zx0::decompress(data);
        if (!unpacked) {
            std::fprintf(stderr, "%s is not a file compressed with ZX0\n", input.c_str());
            return 1;
        }
        if (output.empty()) {
            output = input;
            if (output.size() > 4 && (output.ends_with(".ZX0") || output.ends_with(".zx0")))
                output.resize(output.size() - 4);
            else
                output += ".unpacked";
        }
        result = header ? tuxcmp::cpc::withHeader(headerName(output), *header, *unpacked) : *unpacked;
        std::printf("%s: %zu bytes -> %s: %zu bytes\n", input.c_str(), data.size(), output.c_str(), unpacked->size());
    } else {
        if (data.empty() || data.size() > static_cast<size_t>(tuxcmp::zx0::kMaxInput)) {
            std::fprintf(stderr, "%s is empty, or longer than %d bytes\n", input.c_str(), tuxcmp::zx0::kMaxInput);
            return 1;
        }
        if (header)
            std::printf("Amsdos header detected, will write Amsdos header to destination file\n");
        const auto packed = tuxcmp::zx0::compress(data);
        if (!packed) {
            std::fprintf(stderr, "%s could not be compressed\n", input.c_str());
            return 1;
        }
        if (output.empty())
            output = input + ".ZX0";
        result = header ? tuxcmp::cpc::withHeader(headerName(output), *header, packed->data) : packed->data;
        std::printf("Source file size=%zu\noutput file size=%zu (gain=%.2f%%), delta=%d\n", data.size(), packed->data.size(),
                    100.0 - 100.0 * static_cast<double>(packed->data.size()) / static_cast<double>(data.size()), packed->delta);
    }
    if (!writeFile(output, result)) {
        std::fprintf(stderr, "Cannot write %s\n", output.c_str());
        return 1;
    }
    return 0;
}
