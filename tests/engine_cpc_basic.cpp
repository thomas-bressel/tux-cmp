// BASIC programs made into tokens as the CPC keeps them: a loader's lines,
// byte for byte; numbers of every size; strings and remarks left as they
// are; and what is not known here refused.

#include "check.h"
#include "cpc/basic.h"

namespace {

using Bytes = std::vector<uint8_t>;

// One line's bytes, from its tokens: the length and the number before
// them, the zero after.
Bytes line(int number, const Bytes& tokens)
{
    const auto length = static_cast<uint8_t>(tokens.size() + 5);
    Bytes out = {length, 0, static_cast<uint8_t>(number), static_cast<uint8_t>(number >> 8)};
    out.insert(out.end(), tokens.begin(), tokens.end());
    out.push_back(0);
    return out;
}

Bytes program(std::initializer_list<Bytes> lines)
{
    Bytes out;
    for (const Bytes& one : lines)
        out.insert(out.end(), one.begin(), one.end());
    out.push_back(0);
    out.push_back(0);
    return out;
}

}  // namespace

int main()
{
    using tuxcmp::cpc::tokenize;

    // A loader, as BASIC keeps it once typed.
    const auto loader = tokenize("10 MODE 1\n"
                                 "20 MEMORY &3FFF\n"
                                 "30 LOAD\"SHOP.ZX0\",&4000\n"
                                 "40 CALL &A000\n"
                                 "50 CALL &BB18\n");
    const Bytes want = program({
        line(10, {0xAD, ' ', 0x0F}),
        line(20, {0xAA, ' ', 0x1C, 0xFF, 0x3F}),
        line(30, {0xA8, '"', 'S', 'H', 'O', 'P', '.', 'Z', 'X', '0', '"', ',', 0x1C, 0x00, 0x40}),
        line(40, {0x83, ' ', 0x1C, 0x00, 0xA0}),
        line(50, {0x83, ' ', 0x1C, 0x18, 0xBB}),
    });
    CHECK(loader.has_value());
    CHECK(loader && *loader == want);

    // Integers: a digit, a byte, a word.
    CHECK(tokenize("10 MODE 0") == program({line(10, {0xAD, ' ', 0x0E})}));
    CHECK(tokenize("10 BORDER 26") == program({line(10, {0x82, ' ', 0x19, 26})}));
    CHECK(tokenize("10 CALL 40000") == std::nullopt);
    CHECK(tokenize("10 CALL 30000") == program({line(10, {0x83, ' ', 0x1A, 0x30, 0x75})}));
    CHECK(tokenize("10 INK 1,24") == program({line(10, {0xA2, ' ', 0x0F, ',', 0x19, 24})}));
    // Statements one after the other, keywords in small letters.
    CHECK(tokenize("100 cls:mode 2") == program({line(100, {0x8A, 0x01, 0xAD, ' ', 0x10})}));
    // A remark, and a string, are kept as they are.
    CHECK(tokenize("10 REM Mode 1: x = &C000") == program({line(10, {0xC5, ' ', 'M', 'o', 'd', 'e', ' ', '1', ':', ' ', 'x', ' ', '=', ' ', '&', 'C', '0', '0', '0'})}));
    CHECK(tokenize("10 RUN\"A:B\"") == program({line(10, {0xCA, '"', 'A', ':', 'B', '"'})}));
    CHECK(tokenize("10 ' note") == program({line(10, {0x01, 0xC0, ' ', 'n', 'o', 't', 'e'})}));
    // Longest keyword first: MODE, not MOD; DIM, not DI.
    CHECK(tokenize("10 DI:EI") == program({line(10, {0xDB, 0x01, 0xDC})}));
    // Blank lines are skipped, a CR before the line feed too.
    CHECK(tokenize("\r\n10 CLS\r\n\n20 CLS\r\n") == program({line(10, {0x8A}), line(20, {0x8A})}));

    // Not known here: variables, operators; lines out of order or without
    // a number; nothing at all.
    CHECK(!tokenize("10 A=1"));
    CHECK(!tokenize("10 MODE X"));
    CHECK(!tokenize("10 PRINT 1+1"));
    CHECK(!tokenize("20 CLS\n10 CLS"));
    CHECK(!tokenize("CLS"));
    CHECK(!tokenize(""));
    CHECK(!tokenize("10 CALL &10000"));
    return checkSummary("engine_cpc_basic");
}
