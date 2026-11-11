// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockcore/TimeUtil.hpp"

#include <cstring>

namespace clockcore
{

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
    static const int kDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12)
    {
        return 0;
    }

    return (month == 2 && isLeapYear(year)) ? 29 : kDays[month - 1];
}

static void putPair(Digits& d, int pos, int value)
{
    d.v[pos] = static_cast<uint8_t>((value / 10) % 10);
    d.v[pos + 1] = static_cast<uint8_t>(value % 10);
}

Digits timeDigits(int hour24, int minute, int second, bool twelveHour)
{
    int h = hour24;
    if (twelveHour)
    {
        h = hour24 % 12;
        if (h == 0)
        {
            h = 12;
        }
    }

    Digits d{};
    putPair(d, 0, h);
    putPair(d, 2, minute);
    putPair(d, 4, second);
    return d;
}

Digits dateDigits(int day, int month, int year)
{
    Digits d{};
    putPair(d, 0, day);
    putPair(d, 2, month);
    putPair(d, 4, ((year % 100) + 100) % 100);
    return d;
}

void buildNtpRequest(uint8_t (&packet)[kNtpPacketSize])
{
    std::memset(packet, 0, sizeof(packet));
    packet[0] = (0 << 6) | (4 << 3) | 3; // LI = 0, VN = 4, Mode = 3 (client)
}

bool parseNtpResponse(const uint8_t* packet, size_t len, uint32_t& unixSeconds)
{
    if (packet == nullptr || len < kNtpPacketSize)
    {
        return false;
    }

    const uint8_t li = packet[0] >> 6;
    const uint8_t mode = packet[0] & 0x07;
    const uint8_t stratum = packet[1];
    if (mode != 4 || li == 3 || stratum == 0 || stratum > 15)
    {
        return false;
    }

    // Transmit timestamp, seconds part (big endian) at offset 40.
    const uint32_t ntpSeconds = (uint32_t(packet[40]) << 24) | (uint32_t(packet[41]) << 16)
        | (uint32_t(packet[42]) << 8) | uint32_t(packet[43]);
    if (ntpSeconds == 0)
    {
        return false;
    }

    constexpr uint32_t kUnixEpochInNtp = 2208988800UL; // 1900-01-01 -> 1970-01-01
    // Era 0 ends 2036-02-07; timestamps below the 1970 offset belong to era 1.
    // The uint32 subtraction wraps correctly into Unix time for both eras (until 2106).
    unixSeconds = ntpSeconds - kUnixEpochInNtp;
    return true;
}

} // namespace clockcore
