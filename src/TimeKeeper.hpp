// SPDX-License-Identifier: GPL-3.0-or-later
//
// Keeps the ESP32 system clock (UTC) correct and converts it to local time.
//   - At boot the time is read from the RTC chip (DS3231 or DS1302, kept in UTC).
//   - Local time uses the POSIX TZ rule from the config (DST handled by the C library).
//   - NTP is queried at most once per day (clockcore::NtpSchedule).
//   - Every manual/NTP/API time change is written back to the RTC.
#pragma once

#include <Arduino.h>
#include <time.h>

#include <string>

#include "clockcore/TimeUtil.hpp"

class TimeKeeper
{
public:
    void begin();
    void applyTimeZone(const std::string& posixTz);

    // Valid = the clock has a plausible time (RTC kept it or it was set since boot).
    bool timeValid() const;
    time_t nowUtc() const { return time(nullptr); }
    struct tm localTime() const;

    void setUtc(time_t utc, const char* source);
    // Sets the clock from a local date/time (interpreted with the active TZ rule).
    void setLocal(int year, int month, int day, int hour, int minute, int second);

    // Call often. Queries NTP when allowed by the schedule.
    void loop(bool networkUp, bool ntpEnabled, const std::string& ntpServer);

    time_t lastNtpSyncUtc() const { return lastNtpUtc_; }
    bool hasRtc() const { return rtcPresent_; }

private:
    bool queryNtp(const char* server, uint32_t& unixSeconds);
    void syncFromRtc();

    clockcore::NtpSchedule ntp_;
    bool rtcPresent_ = false;
    time_t lastNtpUtc_ = 0;
    uint32_t lastRtcReadMs_ = 0;
};
