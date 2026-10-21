// tuxcmp run as a user runs it: PackZX0's pictures compressed to the files
// PackZX0 made, beside them or under another name; a file with an AMSDOS
// header compressed without it and given one; decompressed back; and what
// it says when it cannot.

#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.h"
#include "cpc/amsdos.h"
#include "golden.h"

namespace {

namespace fs = std::filesystem;

int run(const std::string& arguments)
{
    const std::string command = std::string("\"") + TUXCMP_PROGRAM + "\" " + arguments + " > /dev/null 2>&1";
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::vector<uint8_t> read(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

void write(const fs::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

}  // namespace

int main()
{
    const fs::path folder = fs::temp_directory_path() / ("tuxcmp_cli_" + std::to_string(::getpid()));
    fs::create_directories(folder);
    const auto at = [&](const char* name) { return (folder / name).string(); };

    // As PackZX0: SHOP.SCR into SHOP.SCR.ZX0.
    write(at("SHOP.SCR"), golden::read("SHOP.SCR"));
    CHECK_EQ(run("\"" + at("SHOP.SCR") + "\""), 0);
    CHECK(read(at("SHOP.SCR.ZX0")) == golden::read("SHOP.SCR.ZX0"));
    // Under another name, and back.
    CHECK_EQ(run("-o \"" + at("other.zx0") + "\" \"" + at("SHOP.SCR") + "\""), 0);
    CHECK(read(at("other.zx0")) == golden::read("SHOP.SCR.ZX0"));
    fs::remove(at("SHOP.SCR"));
    CHECK_EQ(run("-d \"" + at("SHOP.SCR.ZX0") + "\""), 0);
    CHECK(read(at("SHOP.SCR")) == golden::read("SHOP.SCR"));

    // With an AMSDOS header: compressed without it, given one.
    const std::vector<uint8_t> picture = golden::read("meta.scr");
    write(at("META.SCR"), tuxcmp::cpc::withHeader("META.SCR", {tuxcmp::cpc::kBinary, 0xC000, 0, 0}, picture));
    CHECK_EQ(run("\"" + at("META.SCR") + "\""), 0);
    const std::vector<uint8_t> packed = read(at("META.SCR.ZX0"));
    const auto header = tuxcmp::cpc::readHeader(packed);
    CHECK(header && header->type == tuxcmp::cpc::kBinary && header->loadAddress == 0xC000);
    CHECK(header && header->length == static_cast<int>(golden::read("meta.scr.ZX0").size()));
    CHECK(packed.size() > 128 && std::vector<uint8_t>(packed.begin() + 128, packed.end()) == golden::read("meta.scr.ZX0"));
    CHECK(std::string(packed.begin() + 1, packed.begin() + 12) == "META    ZX0");
    // Decompressed, it is the file again, header and all.
    CHECK_EQ(run("-d -o \"" + at("back.scr") + "\" \"" + at("META.SCR.ZX0") + "\""), 0);
    const std::vector<uint8_t> back = read(at("back.scr"));
    CHECK(back.size() == 128 + picture.size() && std::equal(picture.begin(), picture.end(), back.begin() + 128));

    // What it cannot do.
    CHECK_EQ(run("\"" + at("nothing.bin") + "\""), 1);
    write(at("empty.bin"), {});
    CHECK_EQ(run("\"" + at("empty.bin") + "\""), 1);
    write(at("noise.zx0"), {0, 0, 0, 0});
    CHECK_EQ(run("-d \"" + at("noise.zx0") + "\""), 1);
    CHECK_EQ(run("-x"), 2);
    CHECK_EQ(run(""), 2);
    CHECK_EQ(run("-help"), 0);

    fs::remove_all(folder);
    return checkSummary("cli_tuxcmp");
}
