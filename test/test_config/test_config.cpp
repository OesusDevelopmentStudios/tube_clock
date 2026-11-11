// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "clockcore/Config.hpp"
#include "clockcore/TzTable.hpp"

using namespace clockcore;

void setUp()
{
}

void tearDown()
{
}

static bool parse(const std::string& json, Config& cfg, std::string* err = nullptr)
{
    return parseConfig(json.data(), json.size(), cfg, err);
}

static std::string readFixture(const char* name)
{
    std::ifstream f(std::string("test/fixtures/") + name);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void test_defaults_from_empty_object()
{
    Config cfg;
    cfg.name = "changed";
    TEST_ASSERT_TRUE(parse("{}", cfg));
    TEST_ASSERT_EQUAL_STRING("", cfg.name.c_str());
    TEST_ASSERT_EQUAL(int(WifiMode::Off), int(cfg.wifi.mode));
    TEST_ASSERT_FALSE(cfg.api.enabled);
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", cfg.time.ntpServer.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC0", cfg.time.tzPosix.c_str());
    TEST_ASSERT_EQUAL(int(Pattern::Rainbow), int(cfg.backlight.pattern));
}

void test_defaults_are_valid()
{
    // The firmware falls back to the defaults when /config.json is missing, so they must validate.
    Config def;
    TEST_ASSERT_EQUAL(0, int(validateConfig(def).size()));
    // Enabling the API without a token is not.
    def.api.enabled = true;
    TEST_ASSERT_EQUAL(1, int(validateConfig(def).size()));
}

void test_full_fixture_roundtrip()
{
    std::string json = readFixture("config_full.json");
    TEST_ASSERT_TRUE_MESSAGE(!json.empty(), "fixture not found (run from project root)");
    Config cfg;
    std::string err;
    TEST_ASSERT_TRUE_MESSAGE(parse(json, cfg, &err), err.c_str());
    TEST_ASSERT_EQUAL_STRING("tubeclock-test", cfg.name.c_str());
    TEST_ASSERT_EQUAL(int(WifiMode::Static), int(cfg.wifi.mode));
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", cfg.wifi.ip.c_str());
    TEST_ASSERT_EQUAL_STRING("correct horse", cfg.wifi.password.c_str());
    TEST_ASSERT_EQUAL_STRING("Europe/Prague", cfg.time.tzName.c_str());
    TEST_ASSERT_EQUAL_STRING("cz.pool.ntp.org", cfg.time.ntpServer.c_str());
    TEST_ASSERT_EQUAL(8, cfg.display.lcdBrightness);
    TEST_ASSERT_EQUAL(int(Pattern::Pulse), int(cfg.backlight.pattern));
    TEST_ASSERT_EQUAL(300, cfg.backlight.colorPhase);

    std::string out = serializeConfig(cfg, true);
    Config again;
    TEST_ASSERT_TRUE_MESSAGE(parse(out, again, &err), err.c_str());
    TEST_ASSERT_EQUAL_STRING(out.c_str(), serializeConfig(again, true).c_str());
}

void test_serialize_without_secrets()
{
    Config cfg;
    TEST_ASSERT_TRUE(parse(readFixture("config_full.json"), cfg));
    std::string out = serializeConfig(cfg, false);
    TEST_ASSERT_NULL(std::strstr(out.c_str(), "correct horse"));
    TEST_ASSERT_NULL(std::strstr(out.c_str(), "00112233445566778899aabbccddeeff"));
    TEST_ASSERT_NOT_NULL(std::strstr(out.c_str(), "\"has_password\": true"));
}

void test_patch_is_partial_and_atomic()
{
    Config cfg;
    TEST_ASSERT_TRUE(parse(readFixture("config_full.json"), cfg));
    std::string patch = R"({"display":{"lcd_brightness":3},"backlight":{"pattern":"breath"}})";
    TEST_ASSERT_TRUE(applyConfigPatch(patch.data(), patch.size(), cfg));
    TEST_ASSERT_EQUAL(3, cfg.display.lcdBrightness);
    TEST_ASSERT_EQUAL(int(Pattern::Breath), int(cfg.backlight.pattern));
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", cfg.wifi.ip.c_str()); // untouched

    std::string bad = R"({"display":{"lcd_brightness":1},"wifi":{"ip":"999.1.1.1"}})";
    std::string err;
    TEST_ASSERT_FALSE(applyConfigPatch(bad.data(), bad.size(), cfg, &err));
    TEST_ASSERT_EQUAL(3, cfg.display.lcdBrightness); // unchanged after failed patch
    TEST_ASSERT_EQUAL_STRING("wifi.ip is not a valid IPv4 address", err.c_str());
}

void test_rejects_invalid_values()
{
    Config cfg;
    std::string err;
    TEST_ASSERT_FALSE(parse("not json", cfg, &err));
    TEST_ASSERT_FALSE(parse("[1,2]", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"wifi":{"mode":"wps"}})", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"backlight":{"pattern":"disco"}})", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"wifi":{"mode":"dhcp","ssid":""}})", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"wifi":{"mode":"dhcp","ssid":"x","password":"short"}})", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"api":{"enabled":true,"token":"xyz"}})", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"time":{"tz_posix":"Europe/Prague"}})", cfg, &err));
    TEST_ASSERT_FALSE(parse(R"({"name":"bad name"})", cfg, &err));
    // Open networks (no password) are allowed.
    TEST_ASSERT_TRUE(parse(R"({"wifi":{"mode":"dhcp","ssid":"cafe"}})", cfg, &err));
}

void test_normalize_clamps()
{
    Config cfg;
    std::string err;
    TEST_ASSERT_TRUE(parse(R"({"display":{"lcd_brightness":99},
                         "backlight":{"intensity":-4,"color_phase":-1}})",
        cfg,
        &err));
    TEST_ASSERT_EQUAL(kLcdBrightnessMax, cfg.display.lcdBrightness);
    TEST_ASSERT_EQUAL(0, cfg.backlight.intensity);
    TEST_ASSERT_EQUAL(kColorPhaseMax - 1, cfg.backlight.colorPhase);
}

void test_strip_dst()
{
    TEST_ASSERT_EQUAL_STRING("CET-1", stripDst("CET-1CEST,M3.5.0,M10.5.0/3").c_str());
    TEST_ASSERT_EQUAL_STRING("EST5", stripDst("EST5EDT,M3.2.0,M11.1.0").c_str());
    TEST_ASSERT_EQUAL_STRING("NST3:30", stripDst("NST3:30NDT,M3.2.0,M11.1.0").c_str());
    TEST_ASSERT_EQUAL_STRING("<+0545>-5:45", stripDst("<+0545>-5:45").c_str());
    TEST_ASSERT_EQUAL_STRING("AEST-10", stripDst("AEST-10AEDT,M10.1.0,M4.1.0/3").c_str());
    TEST_ASSERT_EQUAL_STRING("<-03>3", stripDst("<-03>3<-02>,M3.5.0/-2,M10.5.0/-1").c_str());
    TEST_ASSERT_EQUAL_STRING("", stripDst("garbage").c_str());
}

void test_effective_tz()
{
    Config cfg;
    cfg.time.tzPosix = "CET-1CEST,M3.5.0,M10.5.0/3";
    cfg.time.dst = true;
    TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", effectivePosixTz(cfg).c_str());
    cfg.time.dst = false;
    TEST_ASSERT_EQUAL_STRING("CET-1", effectivePosixTz(cfg).c_str());
}

void test_every_table_zone_is_valid()
{
    for (int i = 0; i < tzTableSize(); ++i)
    {
        const TzEntry& z = tzTable()[i];
        TEST_ASSERT_TRUE_MESSAGE(isValidPosixTz(z.posix), z.name);
        TEST_ASSERT_FALSE_MESSAGE(stripDst(z.posix).empty(), z.name);
    }

    TEST_ASSERT_EQUAL(0, tzIndexOf("Etc/UTC"));
    TEST_ASSERT_TRUE(tzIndexOf("Europe/Prague") > 0);
    TEST_ASSERT_EQUAL(-1, tzIndexOf("Mars/Olympus_Mons"));
}

void test_ipv4_and_hostname()
{
    TEST_ASSERT_TRUE(isValidIPv4("10.0.0.1"));
    TEST_ASSERT_TRUE(isValidIPv4("255.255.255.0"));
    TEST_ASSERT_FALSE(isValidIPv4("256.0.0.1"));
    TEST_ASSERT_FALSE(isValidIPv4("1.2.3"));
    TEST_ASSERT_FALSE(isValidIPv4("1.2.3.4.5"));
    TEST_ASSERT_FALSE(isValidIPv4("01.2.3.4"));
    TEST_ASSERT_FALSE(isValidIPv4(""));
    TEST_ASSERT_TRUE(isValidHostName("pool.ntp.org"));
    TEST_ASSERT_TRUE(isValidHostName("tubeclock-a1b2c3"));
    TEST_ASSERT_FALSE(isValidHostName("-bad"));
    TEST_ASSERT_FALSE(isValidHostName("bad-"));
    TEST_ASSERT_FALSE(isValidHostName("a..b"));
    TEST_ASSERT_FALSE(isValidHostName("has space"));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults_from_empty_object);
    RUN_TEST(test_defaults_are_valid);
    RUN_TEST(test_full_fixture_roundtrip);
    RUN_TEST(test_serialize_without_secrets);
    RUN_TEST(test_patch_is_partial_and_atomic);
    RUN_TEST(test_rejects_invalid_values);
    RUN_TEST(test_normalize_clamps);
    RUN_TEST(test_strip_dst);
    RUN_TEST(test_effective_tz);
    RUN_TEST(test_every_table_zone_is_valid);
    RUN_TEST(test_ipv4_and_hostname);
    return UNITY_END();
}
