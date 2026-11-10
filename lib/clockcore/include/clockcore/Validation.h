// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dependency free validation helpers shared by the firmware and the desktop app
// (the app compiles this file directly, see app/CMakeLists.txt).
#pragma once

#include <string>

namespace clockcore {

// "CET-1CEST,M3.5.0,M10.5.0/3" -> "CET-1". Returns the input unchanged if it has no DST part
// and an empty string if it cannot be parsed.
std::string
stripDst(const std::string& posix);

// Basic syntax check of a POSIX TZ rule (std name + offset [+ dst name [offset] [,rules]]).
bool
isValidPosixTz(const std::string& posix);

bool
isValidIPv4(const std::string& s);
bool
isValidHostName(const std::string& s); // RFC 1123 label or dotted name, <= 63 chars
bool
isValidToken(const std::string& s); // 16..64 hex chars

} // namespace clockcore
