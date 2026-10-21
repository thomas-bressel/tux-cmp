// AMSDOS's headers and names: a header made and read back, its checksum
// and its bytes where AMSDOS has them; names made fit for a disc.

#include "check.h"
#include "cpc/amsdos.h"

int main()
{
    using namespace tuxcmp::cpc;

    const std::vector<uint8_t> data(1000, 0x33);
    const std::vector<uint8_t> file = withHeader("shop.zx0", {kBinary, 0x8D90, 0x1234, 0}, data);
    CHECK_EQ(file.size(), 1128);
    CHECK(file[0] == 0 && std::string(file.begin() + 1, file.begin() + 12) == "SHOP    ZX0");
    CHECK_EQ(file[18], 2);
    // The length three times: in 16 bits twice, in 24 bits.
    CHECK(file[19] == 0xE8 && file[20] == 0x03 && file[24] == 0xE8 && file[25] == 0x03);
    CHECK(file[64] == 0xE8 && file[65] == 0x03 && file[66] == 0);
    CHECK(file[21] == 0x90 && file[22] == 0x8D && file[26] == 0x34 && file[27] == 0x12);
    unsigned sum = 0;
    for (int i = 0; i < 67; ++i)
        sum += file[static_cast<size_t>(i)];
    CHECK_EQ(file[67] | file[68] << 8, sum);

    const auto header = readHeader(file);
    CHECK(header && header->type == kBinary && header->loadAddress == 0x8D90 && header->entryAddress == 0x1234);
    CHECK(header && header->length == 1000);
    CHECK_EQ(withoutHeader(file).size(), 1000);
    // A file to the end of its last record: what the header says, no more.
    std::vector<uint8_t> padded = file;
    padded.resize(128 + 1024, 0x1A);
    CHECK(withoutHeader(padded).size() == 1000 && withoutHeader(padded)[999] == 0x33);
    // No header: data alone, zeroes, a checksum that is wrong.
    CHECK(!readHeader(data));
    CHECK(!readHeader(std::vector<uint8_t>(200, 0)));
    std::vector<uint8_t> spoilt = file;
    spoilt[30] ^= 1;
    CHECK(!readHeader(spoilt));
    CHECK_EQ(withoutHeader(spoilt).size(), 1128);

    CHECK(discName("shop.zx0") == "SHOP.ZX0");
    CHECK(discName("LOADER") == "LOADER");
    CHECK(!discName("toolongname.bin"));
    CHECK(!discName("a.long"));
    CHECK(!discName("with space.bin"));
    CHECK(!discName(".bin"));
    CHECK(!discName("a:b.bin"));
    CHECK(nameLike("/home/me/Shop screen.scr", "ZX0") == "SHOPSCRE.ZX0");
    CHECK(nameLike("C:\\cpc\\title.go1", "zx0") == "TITLE.ZX0");
    CHECK(nameLike("meta.scr", "") == "META");
    CHECK(nameLike("...", "BAS") == "FILE.BAS");
    CHECK(nameLike("a.b.c", "ZX0") == "A.ZX0");
    return checkSummary("engine_cpc_amsdos");
}
