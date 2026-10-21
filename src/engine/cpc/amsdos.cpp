#include "cpc/amsdos.h"

#include <cstring>

namespace tuxcmp::cpc {

namespace {

// What CP/M keeps for itself, and what is not a character at all.
bool usable(char c)
{
    return c > ' ' && c <= '~' && !std::strchr("<>.,;:=?*[]", c);
}

char capital(char c)
{
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

unsigned checksum(std::span<const uint8_t> header)
{
    unsigned sum = 0;
    for (size_t i = 0; i < 67; ++i)
        sum += header[i];
    return sum;
}

}  // namespace

std::optional<Header> readHeader(std::span<const uint8_t> file)
{
    if (file.size() < kHeaderSize)
        return std::nullopt;
    const unsigned sum = checksum(file);
    // A file of zeroes adds up to zero too, and has no header.
    if (sum == 0 || sum != static_cast<unsigned>(file[67] | file[68] << 8))
        return std::nullopt;
    Header header;
    header.type = file[18];
    header.loadAddress = static_cast<uint16_t>(file[21] | file[22] << 8);
    header.entryAddress = static_cast<uint16_t>(file[26] | file[27] << 8);
    header.length = file[64] | file[65] << 8 | file[66] << 16;
    return header;
}

std::span<const uint8_t> withoutHeader(std::span<const uint8_t> file)
{
    const std::optional<Header> header = readHeader(file);
    if (!header)
        return file;
    const std::span<const uint8_t> after = file.subspan(kHeaderSize);
    // A length of nothing, or of more than there is: all that follows.
    const auto length = static_cast<size_t>(header->length);
    return header->length > 0 && length < after.size() ? after.first(length) : after;
}

std::vector<uint8_t> makeHeader(const std::string& name, const Header& header)
{
    std::vector<uint8_t> out(kHeaderSize, 0);
    const std::string proper = discName(name).value_or("FILE");
    const size_t dot = proper.find('.');
    const std::string stem = proper.substr(0, dot), extension = dot == std::string::npos ? "" : proper.substr(dot + 1);
    std::memset(&out[1], ' ', 11);
    std::memcpy(&out[1], stem.data(), stem.size());
    std::memcpy(&out[9], extension.data(), extension.size());
    out[18] = header.type;
    // The length twice in 16 bits, as AMSDOS writes it, and in 24.
    out[19] = out[24] = out[64] = static_cast<uint8_t>(header.length);
    out[20] = out[25] = out[65] = static_cast<uint8_t>(header.length >> 8);
    out[66] = static_cast<uint8_t>(header.length >> 16);
    out[21] = static_cast<uint8_t>(header.loadAddress);
    out[22] = static_cast<uint8_t>(header.loadAddress >> 8);
    out[26] = static_cast<uint8_t>(header.entryAddress);
    out[27] = static_cast<uint8_t>(header.entryAddress >> 8);
    const unsigned sum = checksum(out);
    out[67] = static_cast<uint8_t>(sum);
    out[68] = static_cast<uint8_t>(sum >> 8);
    return out;
}

std::vector<uint8_t> withHeader(const std::string& name, const Header& header, std::span<const uint8_t> data)
{
    Header complete = header;
    complete.length = static_cast<int>(data.size());
    std::vector<uint8_t> file = makeHeader(name, complete);
    file.insert(file.end(), data.begin(), data.end());
    return file;
}

std::optional<std::string> discName(const std::string& name)
{
    const size_t dot = name.rfind('.');
    std::string stem = name.substr(0, dot), extension = dot == std::string::npos ? "" : name.substr(dot + 1);
    if (stem.empty() || stem.size() > 8 || extension.size() > 3)
        return std::nullopt;
    for (std::string* part : {&stem, &extension})
        for (char& c : *part) {
            c = capital(c);
            if (!usable(c))
                return std::nullopt;
        }
    return extension.empty() ? stem : stem + "." + extension;
}

std::string nameLike(const std::string& name, const std::string& extension)
{
    // The file's name without its folder and its extension.
    std::string stem = name.substr(name.find_last_of("/\\") == std::string::npos ? 0 : name.find_last_of("/\\") + 1);
    if (const size_t dot = stem.find('.'); dot != std::string::npos && dot > 0)
        stem.resize(dot);
    std::string kept;
    for (const char c : stem)
        if (usable(c) && kept.size() < 8)
            kept += capital(c);
    if (kept.empty())
        kept = "FILE";
    std::string tail;
    for (const char c : extension)
        if (usable(c) && tail.size() < 3)
            tail += capital(c);
    return tail.empty() ? kept : kept + "." + tail;
}

}  // namespace tuxcmp::cpc
