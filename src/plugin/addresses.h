#pragma once

#include <cstdint>
#include <optional>

#include <QString>

namespace tuxcmp {

// Addresses as the windows show them and read them: in hexadecimal, after
// a '&' as the CPC writes them; '#', '$', "0x" or nothing before the digits
// read as well.

inline QString addressText(int address)
{
    return QStringLiteral("&%1").arg(address & 0xFFFF, 4, 16, QLatin1Char('0')).toUpper();
}

inline std::optional<uint16_t> readAddress(QString text)
{
    text = text.trimmed();
    for (const char* prefix : {"&", "#", "$", "0x", "0X"})
        if (text.startsWith(QLatin1String(prefix))) {
            text = text.mid(static_cast<qsizetype>(qstrlen(prefix)));
            break;
        }
    bool ok = false;
    const uint value = text.toUInt(&ok, 16);
    if (!ok || text.isEmpty() || value > 0xFFFF)
        return std::nullopt;
    return static_cast<uint16_t>(value);
}

}  // namespace tuxcmp
