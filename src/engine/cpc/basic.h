#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Locomotive BASIC's programs, as the CPC keeps them in memory from &0170
// and AMSDOS on a disc: each line its length, its number, its tokens and a
// zero; two zeroes after the last.

namespace tuxcmp::cpc {

// Where a BASIC program starts in memory: its header's load address.
constexpr uint16_t kBasicStart = 0x0170;

// A program typed as text, a line to a line, each starting with its number,
// made into tokens as BASIC does when the lines are typed. What is known is
// what a loader is made of: the keywords of the statements, strings,
// integers in decimal and in hexadecimal (&C000), ':' between statements,
// commas and brackets, and remarks. Nothing for anything else, variables
// and operators among them, or a line out of order.
std::optional<std::vector<uint8_t>> tokenize(const std::string& listing);

}  // namespace tuxcmp::cpc
