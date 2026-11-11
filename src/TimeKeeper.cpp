// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimeKeeper.hpp"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <sys/time.h>

#include "GLOBAL_DEFINES.hpp"

// Anything before this is treated as "time not set" (RTC lost power, no NTP yet).
static constexpr time_t kMinValidUtc = 1704067200; // 2024-01-01T00:00:00Z
static constexpr uint32_t kRtcResyncMs = 60UL * 60 * 1000;

// ---- RTC chip access -------------------------------------------------------------

#if defined(HAS_RTC_DS1302)
#include <RtcDS1302.h>
#include <ThreeWire.h>
static ThreeWire rtcWire(DS1302_IO, DS1302_SCLK, DS1302_CE);
static RtcDS1302<ThreeWire> rtc(rtcWire);

static bool rtcBegin()
{
    rtc.Begin();
    if (rtc.GetIsWriteProtected())
    {
        rtc.SetIsWriteProtected(false);
    }

    if (!rtc.GetIsRunning())
    {
        rtc.SetIsRunning(true);
    }

    return true; // the DS1302 cannot be detected reliably
}

static bool rtcRead(time_t& utc)
{
    if (!rtc.IsDateTimeValid())
    {
        return false;
    }

    utc = time_t(rtc.GetDateTime().Unix32Time());
    return true;
}

static void rtcWrite(time_t utc)
{
    RtcDateTime t;
    t.InitWithUnix32Time(uint32_t(utc));
    rtc.SetDateTime(t);
}

#elif defined(HAS_RTC_DS3231)
#include <RTClib.h>
#include <Wire.h>
static RTC_DS3231 rtc;

static bool rtcBegin()
{
    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    return rtc.begin();
}

static bool rtcRead(time_t& utc)
{
    if (rtc.lostPower())
    {
        return false;
    }

    utc = time_t(rtc.now().unixtime());
    return true;
}

static void rtcWrite(time_t utc)
{
    rtc.adjust(DateTime(uint32_t(utc)));
}
#endif

// ---- TimeKeeper -------------------------------------------------------------------

void TimeKeeper::begin()
{
    rtcPresent_ = rtcBegin();
    Serial.printf("RTC: %s\n", rtcPresent_ ? "found" : "not found");
    syncFromRtc();
}

void TimeKeeper::syncFromRtc()
{
    lastRtcReadMs_ = millis();
    time_t utc;
    if (!rtcPresent_ || !rtcRead(utc) || utc < kMinValidUtc)
    {
        return;
    }

    const time_t sys = time(nullptr);
    // Only step the system clock if it is unset or drifted by more than a second.
    if (sys < kMinValidUtc || llabs((long long)(sys - utc)) > 1)
    {
        struct timeval tv = { utc, 0 };
        settimeofday(&tv, nullptr);
    }
}

void TimeKeeper::applyTimeZone(const std::string& posixTz)
{
    setenv("TZ", posixTz.c_str(), 1);
    tzset();
    Serial.printf("Time zone: %s\n", posixTz.c_str());
}

bool TimeKeeper::timeValid() const
{
    return time(nullptr) >= kMinValidUtc;
}

struct tm TimeKeeper::localTime() const
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return t;
}

void TimeKeeper::setUtc(time_t utc, const char* source)
{
    struct timeval tv = { utc, 0 };
    settimeofday(&tv, nullptr);
    if (rtcPresent_)
    {
        rtcWrite(utc);
    }

    Serial.printf("Time set from %s: %ld\n", source, long(utc));
}

void TimeKeeper::setLocal(int year, int month, int day, int hour, int minute, int second)
{
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1; // let the TZ rule decide
    setUtc(mktime(&t), "menu");
}

void TimeKeeper::loop(bool networkUp, bool ntpEnabled, const std::string& ntpServer)
{
    const uint32_t nowMs = millis();
    if (nowMs - lastRtcReadMs_ >= kRtcResyncMs)
    {
        syncFromRtc(); // the DS3231 is far more accurate than the ESP32 clock
    }

    if (!networkUp || !ntpEnabled || ntpServer.empty())
    {
        return;
    }

    // 64-bit milliseconds so the schedule survives the 49-day millis() wrap.
    static uint32_t lastMs = 0;
    static uint64_t highMs = 0;
    if (nowMs < lastMs)
    {
        highMs += 1ULL << 32;
    }

    lastMs = nowMs;
    const uint64_t now64 = highMs + nowMs;

    if (!ntp_.due(now64))
    {
        return;
    }

    uint32_t unixSeconds;
    if (queryNtp(ntpServer.c_str(), unixSeconds))
    {
        ntp_.onSuccess(now64);
        lastNtpUtc_ = time_t(unixSeconds);
        setUtc(time_t(unixSeconds), "NTP");
    }
    else
    {
        ntp_.onFailure(now64);
        Serial.println(
            ntp_.hasSynced() ? "NTP: no valid answer, retrying in 1 h" : "NTP: no valid answer, retrying in 1 min");
    }
}

bool TimeKeeper::queryNtp(const char* server, uint32_t& unixSeconds)
{
    IPAddress addr;
    if (!WiFi.hostByName(server, addr))
    {
        Serial.printf("NTP: cannot resolve %s\n", server);
        return false;
    }

    WiFiUDP udp;
    if (!udp.begin(0))
    {
        return false;
    }

    uint8_t packet[clockcore::kNtpPacketSize];
    clockcore::buildNtpRequest(packet);
    bool ok = false;
    if (udp.beginPacket(addr, 123) && udp.write(packet, sizeof(packet)) == sizeof(packet) && udp.endPacket())
    {
        const uint32_t start = millis();
        while (millis() - start < 1500)
        {
            if (udp.parsePacket() >= int(sizeof(packet)))
            {
                udp.read(packet, sizeof(packet));
                ok = clockcore::parseNtpResponse(packet, sizeof(packet), unixSeconds);
                break;
            }

            delay(10);
        }
    }

    udp.stop();
    return ok;
}
