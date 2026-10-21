// TuxCMP in TuxAPE's own CPC, which tests/ cannot do: DepackZX0.asm and
// the program's source assembled by TuxAPE's assembler, to the bytes TuxCMP
// writes; a loader typed into BASIC and saved by it, to the tokens TuxCMP
// writes; and loaders run from a disc by AMSDOS and BASIC, in a CPC 6128
// with its ROMs: a picture to the screen, a program run where it goes back,
// 40K decompressed over the end of their compressed data.
//
// Built and run by run.sh, against TuxAPE's tuxape_core.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>

#include "core/assembler.h"
#include "core/autotype.h"
#include "core/cpc.h"
#include "core/discfiles.h"
#include "core/screen_text.h"
#include "core/setup.h"

#include "cpc/amsdos.h"
#include "cpc/basic.h"
#include "cpc/depacker.h"
#include "cpc/loader.h"
#include "zx0/zx0.h"

using namespace tuxape;
namespace {

int failures = 0;

}  // namespace

#define EXPECT(c)                                                            \
    do {                                                                     \
        if (!(c)) {                                                          \
            std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #c);       \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static std::vector<uint8_t> load(const std::string& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
static std::string text(const std::string& p) { std::ifstream f(p); return {std::istreambuf_iterator<char>(f), {}}; }

static std::vector<uint8_t> assembled(const std::string& source, uint16_t* start = nullptr) {
    const AsmResult r = assemble(source, "check.asm");
    for (const auto& e : r.errors) std::printf("asm error line %d: %s\n", e.line, e.message.c_str());
    std::vector<uint8_t> out;
    if (!r.memory.empty()) { if (start) *start = r.memory[0].address; }
    for (const auto& b : r.memory) out.insert(out.end(), b.data.begin(), b.data.end());
    return out;
}

struct Machine {
    Cpc cpc;
    AutoType keys{cpc.keyboard()};
    void frames(int n) { for (int i = 0; i < n; ++i) { keys.frame(); cpc.runFrame(); } }
    bool ready() {
        std::string s = readScreenText(cpc);
        while (!s.empty() && s.back() == '\n') s.pop_back();
        return s.size() >= 7 && s.compare(s.size() - 7, 7, "Ready\n?") == 0;
    }
    bool command(const char* line) {
        keys.type(line); keys.type("\n");
        for (int i = 0; i < 50 * 60; ++i) { frames(1); if (!keys.active() && ready()) return true; }
        std::printf("timed out after: %s\n", line); return false;
    }
    void enter(const std::string& line) { keys.type(line); keys.type("\n"); while (keys.active()) frames(1); frames(5); }
    bool ramIs(uint16_t at, const std::vector<uint8_t>& want) {
        for (size_t i = 0; i < want.size(); ++i) if (cpc.memory().readRam(static_cast<uint16_t>(at + i)) != want[i]) return false;
        return true;
    }
};

static std::unique_ptr<Machine> boot() {
    auto m = std::make_unique<Machine>();
    std::string error;
    if (!setupStockMachine(m->cpc, CpcModel::Cpc6128, defaultRomDir(), &error)) { std::printf("no ROMs: %s\n", error.c_str()); return nullptr; }
    m->cpc.fdc().setFast(true);
    auto blank = std::make_unique<Disc>(); blank->format(defaultDiscFormat()); blank->modified = false;
    m->cpc.fdc().drive(0).disc = std::move(blank);
    m->frames(150);
    return m;
}

static void putLoader(Machine& m, const tuxcmp::cpc::Loader& loader, const std::vector<uint8_t>& packed) {
    const auto files = tuxcmp::cpc::makeLoaderFiles(loader, packed);
    EXPECT(files.has_value());
    DiscFiles disc(*m.cpc.fdc().drive(0).disc);
    EXPECT(disc.write(loader.packedName, files->packed));
    EXPECT(disc.write(loader.programName, files->program));
    EXPECT(disc.write(loader.basicName, files->basic));
}

int main(int argc, char** argv) {
    const std::string golden = argv[1];
    // ---- 1. The routine and the program, by TuxAPE's assembler ----
    const std::string depack = text(golden + "/DepackZX0.asm");
    const std::vector<uint8_t> routine = assembled("org &A000\n" + depack);
    EXPECT(routine == tuxcmp::cpc::depackRoutine(0xA000));
    std::printf("DepackZX0.asm at &A000: %zu bytes\n", routine.size());
    for (const auto& call : {tuxcmp::cpc::DepackerCall{0x8D40, 0x8D90, 0xC000, std::nullopt},
                             tuxcmp::cpc::DepackerCall{0x0400, 0x1234, 0x4000, uint16_t(0x4010)},
                             tuxcmp::cpc::DepackerCall{0xA600, 0x0450, 0x0400, uint16_t(0x0400)}}) {
        uint16_t start = 0;
        const std::vector<uint8_t> fromSource = assembled(tuxcmp::cpc::depackerSource(call, "X.ZX0"), &start);
        EXPECT(start == call.origin && fromSource == tuxcmp::cpc::depackerProgram(call));
    }

    // ---- 2. A picture: RUN"SHOP" ----
    const std::vector<uint8_t> shop = load(golden + "/SHOP.SCR");
    const tuxcmp::zx0::Packed shopPacked{load(golden + "/SHOP.SCR.ZX0"), 2};
    {
        auto m = boot(); if (!m) return 77;
        const auto loader = tuxcmp::cpc::suggestLoader("SHOP.ZX0", 0xC000, std::nullopt, 16384, shopPacked);
        putLoader(*m, loader, shopPacked.data);
        // The listing typed in and saved by BASIC itself: the same tokens.
        const std::string listing = tuxcmp::cpc::loaderListing(loader);
        size_t at = 0;
        while (at < listing.size()) { const size_t nl = listing.find('\n', at); m->enter(listing.substr(at, nl - at)); at = nl + 1; }
        EXPECT(m->command("SAVE\"TYPED\""));
        DiscFiles disc(*m->cpc.fdc().drive(0).disc);
        std::optional<std::vector<uint8_t>> typed;
        for (const auto& f : disc.list()) if (f.name == "TYPED.BAS") typed = disc.read(f);
        const auto mine = tuxcmp::cpc::tokenize(listing);
        EXPECT(typed && mine);
        if (typed && mine) {
            const auto header = tuxcmp::cpc::readHeader(*typed);
            std::printf("BASIC saved %d bytes, TuxCMP's are %zu\n", header ? header->length : -1, mine->size());
            const bool same = header && std::equal(mine->begin(), mine->end(), typed->begin() + 128);
            EXPECT(same);
            if (!same) { for (size_t i = 0; i < mine->size() + 4; ++i) std::printf("%02X/%02X ", (*typed)[128 + i], i < mine->size() ? (*mine)[i] : 0); std::printf("\n"); }
        }
        EXPECT(m->command("NEW"));
        std::printf("%s", listing.c_str());
        m->keys.type("RUN\"SHOP\"\n");
        bool shown = false;
        for (int i = 0; i < 50 * 60 && !shown; ++i) { m->frames(1); shown = !m->keys.active() && m->ramIs(0xC000, shop); }
        EXPECT(shown);
        EXPECT(!m->ready());  // it waits for a key
        m->keys.type(" ");
        m->frames(100);
        // Back in BASIC: MODE clears the picture for its Ready to be read.
        const bool back = m->command("MODE 1");
        EXPECT(back);
        std::printf("picture: shown=%d back=%d\n", shown, back);
    }

    // ---- 3. A program at &4000, run there ----
    {
        auto m = boot(); if (!m) return 77;
        std::vector<uint8_t> code = assembled("org &4000\nld a,&42\nld (&9000),a\nloop: jr loop\n");
        std::mt19937 random(5);
        for (int i = 0; i < 10000; ++i) code.push_back(static_cast<uint8_t>(random() % 7 * 3));
        const auto packed = tuxcmp::zx0::compress(code);
        const auto loader = tuxcmp::cpc::suggestLoader("GAME.ZX0", 0x4000, uint16_t(0x4000), static_cast<int>(code.size()), *packed);
        EXPECT(tuxcmp::cpc::works(tuxcmp::cpc::checkLoader(loader, code, packed->data)));
        putLoader(*m, loader, packed->data);
        std::printf("%s", tuxcmp::cpc::loaderListing(loader).c_str());
        m->keys.type("RUN\"GAME\"\n");
        bool ran = false;
        for (int i = 0; i < 50 * 60 && !ran; ++i) { m->frames(1); ran = m->cpc.memory().readRam(0x9000) == 0x42; }
        EXPECT(ran && m->ramIs(0x4000, code));
        std::printf("program: ran=%d\n", ran);
    }

    // ---- 4. 40K at &0400, in place ----
    {
        auto m = boot(); if (!m) return 77;
        std::vector<uint8_t> code = assembled("org &0400\nld a,&55\nld (&C800),a\nloop: jr loop\n");
        std::mt19937 random(9);
        while (code.size() < 0xA640 - 0x0400) code.push_back(code.size() % 50 < 35 ? static_cast<uint8_t>(random()) : static_cast<uint8_t>(code.size() / 64));
        const auto packed = tuxcmp::zx0::compress(code);
        const auto loader = tuxcmp::cpc::suggestLoader("BIG.ZX0", 0x0400, uint16_t(0x0400), static_cast<int>(code.size()), *packed);
        const auto found = tuxcmp::cpc::checkLoader(loader, code, packed->data);
        EXPECT(tuxcmp::cpc::works(found));
        std::printf("in place: %zu -> %zu bytes, delta %d, source &%04X program &%04X\n", code.size(), packed->data.size(), packed->delta, loader.source, loader.program);
        putLoader(*m, loader, packed->data);
        std::printf("%s", tuxcmp::cpc::loaderListing(loader).c_str());
        m->keys.type("RUN\"BIG\"\n");
        bool ran = false;
        for (int i = 0; i < 50 * 120 && !ran; ++i) { m->frames(1); ran = m->cpc.memory().readRam(0xC800) == 0x55; }
        EXPECT(ran && m->ramIs(0x0400, code));
        std::printf("in place: ran=%d\n", ran);
        if (!ran) std::printf("%s\n", readScreenText(m->cpc).c_str());
    }
    std::printf(failures ? "cpccheck: %d failure(s)\n" : "cpccheck: all good\n", failures);
    return failures ? 1 : 0;
}
