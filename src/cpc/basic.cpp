#include "cpc/basic.h"

#include <array>
#include <cctype>
#include <sstream>
#include <string_view>

namespace tuxcmp::cpc {

namespace {

// The statements' keywords, from token &80, as BASIC 1.1 has them; those of
// more than one word are left out.
constexpr std::array<std::string_view, 0x60> kKeywords = {
    "AFTER", "AUTO", "BORDER", "CALL", "CAT", "CHAIN", "CLEAR", "CLG", "CLOSEIN", "CLOSEOUT", "CLS", "CONT",
    "DATA", "DEF", "DEFINT", "DEFREAL", "DEFSTR", "DEG", "DELETE", "DIM", "DRAW", "DRAWR", "EDIT", "ELSE",
    "END", "ENT", "ENV", "ERASE", "ERROR", "EVERY", "FOR", "GOSUB", "GOTO", "IF", "INK", "INPUT",
    "KEY", "LET", "LINE", "LIST", "LOAD", "LOCATE", "MEMORY", "MERGE", "MID$", "MODE", "MOVE", "MOVER",
    "NEXT", "NEW", "ON", "", "", "", "OPENIN", "OPENOUT", "ORIGIN", "OUT", "PAPER", "PEN",
    "PLOT", "PLOTR", "POKE", "PRINT", "", "RAD", "RANDOMIZE", "READ", "RELEASE", "REM", "RENUM", "RESTORE",
    "RESUME", "RETURN", "RUN", "SAVE", "SOUND", "SPEED", "STOP", "SYMBOL", "TAG", "TAGOFF", "TROFF", "TRON",
    "WAIT", "WEND", "WHILE", "WIDTH", "WINDOW", "WRITE", "ZONE", "DI", "EI", "FILL", "GRAPHICS", "MASK"};

constexpr uint8_t kStatementSeparator = 0x01;
constexpr uint8_t kRemToken = 0xC5;
constexpr uint8_t kQuoteRemark = 0xC0;
constexpr uint8_t kSmallInteger = 0x0E;  // 0 to 9: &0E to &17
constexpr uint8_t kByteInteger = 0x19;
constexpr uint8_t kWordInteger = 0x1A;
constexpr uint8_t kHexInteger = 0x1C;

void word(std::vector<uint8_t>& out, unsigned value)
{
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

// The keyword at the start of `text`, the longest that is there; -1 if none.
int keywordAt(std::string_view text, size_t* length)
{
    int found = -1;
    *length = 0;
    for (size_t token = 0; token < kKeywords.size(); ++token) {
        const std::string_view keyword = kKeywords[token];
        if (keyword.empty() || keyword.size() <= *length || keyword.size() > text.size())
            continue;
        bool same = true;
        for (size_t i = 0; i < keyword.size() && same; ++i)
            same = std::toupper(static_cast<unsigned char>(text[i])) == keyword[i];
        if (same) {
            found = static_cast<int>(token);
            *length = keyword.size();
        }
    }
    return found;
}

// A line's tokens, after its number.
std::optional<std::vector<uint8_t>> tokens(std::string_view text)
{
    std::vector<uint8_t> out;
    size_t at = 0;
    while (at < text.size()) {
        const char c = text[at];
        if (c == '"') {
            // A string, to its closing quote or the end of the line.
            const size_t end = text.find('"', at + 1);
            const size_t stop = end == std::string_view::npos ? text.size() : end + 1;
            out.insert(out.end(), text.begin() + static_cast<std::ptrdiff_t>(at), text.begin() + static_cast<std::ptrdiff_t>(stop));
            at = stop;
        } else if (c == '\'') {
            // A remark to the end of the line, after its own separator.
            out.push_back(kStatementSeparator);
            out.push_back(kQuoteRemark);
            out.insert(out.end(), text.begin() + static_cast<std::ptrdiff_t>(at + 1), text.end());
            at = text.size();
        } else if (c == '&' && at + 1 < text.size() && std::isxdigit(static_cast<unsigned char>(text[at + 1]))) {
            unsigned value = 0;
            ++at;
            while (at < text.size() && std::isxdigit(static_cast<unsigned char>(text[at]))) {
                value = value * 16 + static_cast<unsigned>(std::stoi(std::string(1, text[at]), nullptr, 16));
                if (value > 0xFFFF)
                    return std::nullopt;
                ++at;
            }
            out.push_back(kHexInteger);
            word(out, value);
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            unsigned value = 0;
            while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) {
                value = value * 10 + static_cast<unsigned>(text[at] - '0');
                if (value > 32767)
                    return std::nullopt;
                ++at;
            }
            if (value < 10) {
                out.push_back(static_cast<uint8_t>(kSmallInteger + value));
            } else if (value < 256) {
                out.push_back(kByteInteger);
                out.push_back(static_cast<uint8_t>(value));
            } else {
                out.push_back(kWordInteger);
                word(out, value);
            }
        } else if (c == ':') {
            out.push_back(kStatementSeparator);
            ++at;
        } else if (c == ' ' || c == ',' || c == '(' || c == ')' || c == '#') {
            out.push_back(static_cast<uint8_t>(c));
            ++at;
        } else {
            size_t length = 0;
            const int token = keywordAt(text.substr(at), &length);
            if (token < 0)
                return std::nullopt;
            out.push_back(static_cast<uint8_t>(0x80 + token));
            at += length;
            // A remark: the rest of the line as it is.
            if (0x80 + token == kRemToken) {
                out.insert(out.end(), text.begin() + static_cast<std::ptrdiff_t>(at), text.end());
                at = text.size();
            }
        }
    }
    return out;
}

}  // namespace

std::optional<std::vector<uint8_t>> tokenize(const std::string& listing)
{
    std::vector<uint8_t> program;
    std::istringstream lines(listing);
    std::string line;
    int last = 0;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.find_first_not_of(' ') == std::string::npos)
            continue;
        size_t at = line.find_first_not_of(' ');
        int number = 0;
        while (at < line.size() && std::isdigit(static_cast<unsigned char>(line[at]))) {
            number = number * 10 + (line[at] - '0');
            if (number > 65535)
                return std::nullopt;
            ++at;
        }
        if (number <= last)
            return std::nullopt;
        last = number;
        // One space after the number is the one BASIC puts back when it lists.
        if (at < line.size() && line[at] == ' ')
            ++at;
        const auto body = tokens(std::string_view(line).substr(at));
        if (!body)
            return std::nullopt;
        word(program, static_cast<unsigned>(2 + 2 + body->size() + 1));
        word(program, static_cast<unsigned>(number));
        program.insert(program.end(), body->begin(), body->end());
        program.push_back(0);
    }
    if (program.empty())
        return std::nullopt;
    word(program, 0);
    return program;
}

}  // namespace tuxcmp::cpc
