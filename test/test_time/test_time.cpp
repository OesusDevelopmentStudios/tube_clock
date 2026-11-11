// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include <cstdlib>
#include <ctime>

#include "clockcore/Config.hpp"
#include "clockcore/TimeUtil.hpp"

using namespace clockcore;

void setUp()
{
}

void tearDown()
{
}

static void assertDigits(const Digits& d, const char* expected)
{
    char got[7];
    for (int i = 0; i < 6; ++i)
    {
        got[i] = char('0' + d.v[i]);
    }

    got[6] = 0;
    TEST_ASSERT_EQUAL_STRING(expected, got);
}

void test_days_in_month()
{
    TEST_ASSERT_EQUAL(31, daysInMonth(2026, 1));
    TEST_ASSERT_EQUAL(28, daysInMonth(2026, 2));
    TEST_ASSERT_EQUAL(29, daysInMonth(2028, 2));
    TEST_ASSERT_EQUAL(28, daysInMonth(2100, 2));
    TEST_ASSERT_EQUAL(29, daysInMonth(2000, 2));
    TEST_ASSERT_EQUAL(30, daysInMonth(2026, 11));
    TEST_ASSERT_EQUAL(0, daysInMonth(2026, 13));
}

void test_time_digits()
{
    assertDigits(timeDigits(9, 5, 7, false), "090507");
    assertDigits(timeDigits(23, 59, 59, false), "235959");
    assertDigits(timeDigits(0, 0, 0, true), "120000");
    assertDigits(timeDigits(13, 30, 0, true), "013000");
    assertDigits(timeDigits(12, 1, 2, true), "120102");
}

void test_date_digits_ddmmyy()
{
    assertDigits(dateDigits(10, 11, 2026), "101126");
    assertDigits(dateDigits(1, 2, 2000), "010200");
}

void test_ntp_request()
{
    uint8_t pkt[kNtpPacketSize];
    buildNtpRequest(pkt);
    TEST_ASSERT_EQUAL_HEX8(0x23, pkt[0]); // LI 0, VN 4, mode 3
    for (size_t i = 1; i < kNtpPacketSize; ++i)
    {
        TEST_ASSERT_EQUAL_HEX8(0, pkt[i]);
    }
}

static void makeResponse(uint8_t (&pkt)[kNtpPacketSize],
    uint32_t ntpSeconds,
    uint8_t li = 0,
    uint8_t mode = 4,
    uint8_t stratum = 2)
{
    for (auto& b : pkt)
    {
        b = 0;
    }

    pkt[0] = uint8_t((li << 6) | (4 << 3) | mode);
    pkt[1] = stratum;
    pkt[40] = uint8_t(ntpSeconds >> 24);
    pkt[41] = uint8_t(ntpSeconds >> 16);
    pkt[42] = uint8_t(ntpSeconds >> 8);
    pkt[43] = uint8_t(ntpSeconds);
}

void test_ntp_response()
{
    uint8_t pkt[kNtpPacketSize];
    uint32_t unixSecs = 0;
    // 2026-11-10 00:00:00 UTC = 1794268800 Unix
    makeResponse(pkt, 1794268800UL + 2208988800UL);
    TEST_ASSERT_TRUE(parseNtpResponse(pkt, sizeof(pkt), unixSecs));
    TEST_ASSERT_EQUAL_UINT32(1794268800UL, unixSecs);

    // Era 1 (after 2036-02-07): NTP seconds wrapped around.
    makeResponse(pkt, 1000);
    TEST_ASSERT_TRUE(parseNtpResponse(pkt, sizeof(pkt), unixSecs));
    TEST_ASSERT_EQUAL_UINT32(4294967296ULL - 2208988800ULL + 1000, unixSecs);

    makeResponse(pkt, 3900000000UL, 3);
    TEST_ASSERT_FALSE(parseNtpResponse(pkt, sizeof(pkt), unixSecs)); // unsynchronised
    makeResponse(pkt, 3900000000UL, 0, 3);
    TEST_ASSERT_FALSE(parseNtpResponse(pkt, sizeof(pkt), unixSecs)); // not server mode
    makeResponse(pkt, 3900000000UL, 0, 4, 0);
    TEST_ASSERT_FALSE(parseNtpResponse(pkt, sizeof(pkt), unixSecs)); // kiss-o'-death
    makeResponse(pkt, 0);
    TEST_ASSERT_FALSE(parseNtpResponse(pkt, sizeof(pkt), unixSecs));
    TEST_ASSERT_FALSE(parseNtpResponse(pkt, 47, unixSecs));
}

void test_ntp_schedule_once_per_day()
{
    NtpSchedule s;
    TEST_ASSERT_TRUE(s.due(0)); // first attempt immediately
    // Before the first success: retry every minute (DNS may not be ready right after connecting).
    s.onFailure(1000);
    TEST_ASSERT_FALSE(s.due(1000 + NtpSchedule::kFirstSyncRetryMs - 1));
    TEST_ASSERT_TRUE(s.due(1000 + NtpSchedule::kFirstSyncRetryMs));
    const uint64_t t = 5000000;
    s.onSuccess(t);
    TEST_ASSERT_TRUE(s.hasSynced());
    TEST_ASSERT_FALSE(s.due(t + 1));
    TEST_ASSERT_FALSE(s.due(t + NtpSchedule::kSyncIntervalMs - 1));
    TEST_ASSERT_TRUE(s.due(t + NtpSchedule::kSyncIntervalMs));
    // After a success, failures retry hourly.
    const uint64_t t2 = t + NtpSchedule::kSyncIntervalMs;
    s.onFailure(t2);
    TEST_ASSERT_FALSE(s.due(t2 + NtpSchedule::kFirstSyncRetryMs));
    TEST_ASSERT_TRUE(s.due(t2 + NtpSchedule::kRetryIntervalMs));
}

// Local time conversion uses the C library with the POSIX rule, like on the clock.
static struct tm localFor(const char* tz, time_t utc)
{
    setenv("TZ", tz, 1);
    tzset();
    struct tm out;
    localtime_r(&utc, &out);
    return out;
}

void test_posix_rules_with_libc()
{
    const time_t summer = 1782907200; // 2026-07-01 12:00:00 UTC
    const time_t winter = 1767268800; // 2026-01-01 12:00:00 UTC
    const char* prague = "CET-1CEST,M3.5.0,M10.5.0/3";
    TEST_ASSERT_EQUAL(14, localFor(prague, summer).tm_hour);
    TEST_ASSERT_EQUAL(13, localFor(prague, winter).tm_hour);
    // "Use summer time" off -> standard time all year.
    std::string noDst = stripDst(prague);
    TEST_ASSERT_EQUAL(13, localFor(noDst.c_str(), summer).tm_hour);
    TEST_ASSERT_EQUAL(13, localFor(noDst.c_str(), winter).tm_hour);
    // Southern hemisphere: DST in January.
    const char* sydney = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    TEST_ASSERT_EQUAL(23, localFor(sydney, winter).tm_hour);
    TEST_ASSERT_EQUAL(22, localFor(sydney, summer).tm_hour);
    // Half-hour offset.
    TEST_ASSERT_EQUAL(17, localFor("<+0545>-5:45", winter).tm_hour);
    TEST_ASSERT_EQUAL(45, localFor("<+0545>-5:45", winter).tm_min);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_days_in_month);
    RUN_TEST(test_time_digits);
    RUN_TEST(test_date_digits_ddmmyy);
    RUN_TEST(test_ntp_request);
    RUN_TEST(test_ntp_response);
    RUN_TEST(test_ntp_schedule_once_per_day);
    RUN_TEST(test_posix_rules_with_libc);
    return UNITY_END();
}
