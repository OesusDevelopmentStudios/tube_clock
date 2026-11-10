// SPDX-License-Identifier: GPL-3.0-or-later
//
// Clock configuration. Stored as /config.json on the LittleFS partition.
// The desktop app writes the initial file into the filesystem image before
// flashing; later changes come from the on-clock menu or the HTTP API.
// Schema documentation: docs/config-format.md
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "clockcore/Validation.h"

namespace clockcore {

enum class WifiMode : uint8_t
{
  Off,
  Dhcp,
  Static
};
enum class Pattern : uint8_t
{
  Off,
  Constant,
  Rainbow,
  Pulse,
  Breath,
  Count
};

constexpr int kConfigVersion = 1;
constexpr int kLcdBrightnessMin = 1;
constexpr int kLcdBrightnessMax = 10;
constexpr int kPatternBrightnessMax = 7; // 0..7
constexpr int kColorPhaseMax = 768;      // 0..767, see Backlights::phaseToColor

struct Config
{
  int version = kConfigVersion;
  std::string name; // host name; empty = derived from the MAC address by the firmware

  struct Wifi
  {
    WifiMode mode = WifiMode::Off;
    std::string ssid;
    std::string password;
    std::string ip;
    std::string gateway;
    std::string netmask = "255.255.255.0";
    std::string dns;
  } wifi;

  struct Api
  {
    bool enabled = false; // off until the app provisions a token
    std::string token;    // shared secret for the HTTP API (hex)
  } api;

  struct Time
  {
    std::string tzName = "Etc/UTC";
    std::string tzPosix = "UTC0"; // full rule including DST
    bool dst = true;              // false: the DST part of tzPosix is ignored
    std::string ntpServer = "pool.ntp.org";
    bool ntpEnabled = true;
    bool twelveHour = false;
  } time;

  struct Display
  {
    std::string imageSet; // empty = first available set
    int lcdBrightness = kLcdBrightnessMax;
  } display;

  struct Backlight
  {
    Pattern pattern = Pattern::Rainbow;
    int colorPhase = 0;
    int intensity = kPatternBrightnessMax;
  } backlight;
};

// Parses a complete config document. Unknown keys are ignored, missing keys keep
// their defaults. Returns false (and leaves `out` untouched) on a JSON syntax error
// or when the result does not pass validate().
bool
parseConfig(const char* json, size_t len, Config& out, std::string* error = nullptr);

// Applies a partial document (same schema, any subset of keys) on top of `cfg`.
// All-or-nothing: on error `cfg` stays unchanged.
bool
applyConfigPatch(const char* json, size_t len, Config& cfg, std::string* error = nullptr);

// Serialises the config. With includeSecrets=false the WiFi password and the API
// token are omitted (used for GET /api/config).
std::string
serializeConfig(const Config& cfg, bool includeSecrets);

// Returns a list of human readable problems; empty means valid.
std::vector<std::string>
validateConfig(const Config& cfg);

// Clamps numeric values into their ranges (never fails).
void
normalizeConfig(Config& cfg);

// The TZ rule the clock should use: tzPosix, or only its standard-time part if dst=false.
std::string
effectivePosixTz(const Config& cfg);

const char*
wifiModeName(WifiMode m);
const char*
patternName(Pattern p);
bool
patternFromName(const std::string& name, Pattern& out);

} // namespace clockcore
