// SPDX-License-Identifier: GPL-3.0-or-later
#define ARDUINOJSON_ENABLE_STD_STRING 1
#include <ArduinoJson.h>

#include "clockcore/Config.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace clockcore {

namespace {

const char* const kWifiModes[] = { "off", "dhcp", "static" };
const char* const kPatterns[] = { "off", "constant", "rainbow", "pulse", "breath" };

int
clampInt(int v, int lo, int hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

// ---- JSON helpers --------------------------------------------------------------

void
readString(JsonVariantConst v, std::string& dst)
{
  if (v.is<const char*>())
    dst = v.as<const char*>();
}

void
readBool(JsonVariantConst v, bool& dst)
{
  if (v.is<bool>())
    dst = v.as<bool>();
}

void
readInt(JsonVariantConst v, int& dst)
{
  if (v.is<int>())
    dst = v.as<int>();
}

bool
applyDocument(JsonObjectConst root, Config& cfg, std::string* error)
{
  readInt(root["version"], cfg.version);
  readString(root["name"], cfg.name);

  JsonObjectConst wifi = root["wifi"];
  if (!wifi.isNull()) {
    if (wifi["mode"].is<const char*>()) {
      std::string m = wifi["mode"].as<const char*>();
      bool found = false;
      for (int k = 0; k < 3; ++k) {
        if (m == kWifiModes[k]) {
          cfg.wifi.mode = static_cast<WifiMode>(k);
          found = true;
        }
      }
      if (!found) {
        if (error)
          *error = "wifi.mode must be off, dhcp or static";
        return false;
      }
    }
    readString(wifi["ssid"], cfg.wifi.ssid);
    readString(wifi["password"], cfg.wifi.password);
    readString(wifi["ip"], cfg.wifi.ip);
    readString(wifi["gateway"], cfg.wifi.gateway);
    readString(wifi["netmask"], cfg.wifi.netmask);
    readString(wifi["dns"], cfg.wifi.dns);
  }

  JsonObjectConst api = root["api"];
  if (!api.isNull()) {
    readBool(api["enabled"], cfg.api.enabled);
    readString(api["token"], cfg.api.token);
  }

  JsonObjectConst time = root["time"];
  if (!time.isNull()) {
    readString(time["tz_name"], cfg.time.tzName);
    readString(time["tz_posix"], cfg.time.tzPosix);
    readBool(time["dst"], cfg.time.dst);
    readString(time["ntp_server"], cfg.time.ntpServer);
    readBool(time["ntp_enabled"], cfg.time.ntpEnabled);
    readBool(time["twelve_hour"], cfg.time.twelveHour);
  }

  JsonObjectConst display = root["display"];
  if (!display.isNull()) {
    readString(display["image_set"], cfg.display.imageSet);
    readInt(display["lcd_brightness"], cfg.display.lcdBrightness);
  }

  JsonObjectConst bl = root["backlight"];
  if (!bl.isNull()) {
    if (bl["pattern"].is<const char*>()) {
      if (!patternFromName(bl["pattern"].as<const char*>(), cfg.backlight.pattern)) {
        if (error)
          *error = "backlight.pattern is unknown";
        return false;
      }
    }
    readInt(bl["color_phase"], cfg.backlight.colorPhase);
    readInt(bl["intensity"], cfg.backlight.intensity);
  }
  return true;
}

bool
parseInto(const char* json, size_t len, Config& cfg, std::string* error)
{
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err) {
    if (error)
      *error = std::string("invalid JSON: ") + err.c_str();
    return false;
  }
  if (!doc.is<JsonObjectConst>()) {
    if (error)
      *error = "config must be a JSON object";
    return false;
  }
  Config tmp = cfg;
  if (!applyDocument(doc.as<JsonObjectConst>(), tmp, error))
    return false;
  normalizeConfig(tmp);
  std::vector<std::string> problems = validateConfig(tmp);
  if (!problems.empty()) {
    if (error)
      *error = problems.front();
    return false;
  }
  cfg = tmp;
  return true;
}

} // namespace

bool
parseConfig(const char* json, size_t len, Config& out, std::string* error)
{
  Config cfg; // start from defaults
  if (!parseInto(json, len, cfg, error))
    return false;
  out = cfg;
  return true;
}

bool
applyConfigPatch(const char* json, size_t len, Config& cfg, std::string* error)
{
  return parseInto(json, len, cfg, error);
}

std::string
serializeConfig(const Config& cfg, bool includeSecrets)
{
  JsonDocument doc;
  doc["version"] = cfg.version;
  doc["name"] = cfg.name;

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["mode"] = wifiModeName(cfg.wifi.mode);
  wifi["ssid"] = cfg.wifi.ssid;
  if (includeSecrets)
    wifi["password"] = cfg.wifi.password;
  else
    wifi["has_password"] = !cfg.wifi.password.empty();
  wifi["ip"] = cfg.wifi.ip;
  wifi["gateway"] = cfg.wifi.gateway;
  wifi["netmask"] = cfg.wifi.netmask;
  wifi["dns"] = cfg.wifi.dns;

  JsonObject api = doc["api"].to<JsonObject>();
  api["enabled"] = cfg.api.enabled;
  if (includeSecrets)
    api["token"] = cfg.api.token;

  JsonObject time = doc["time"].to<JsonObject>();
  time["tz_name"] = cfg.time.tzName;
  time["tz_posix"] = cfg.time.tzPosix;
  time["dst"] = cfg.time.dst;
  time["ntp_server"] = cfg.time.ntpServer;
  time["ntp_enabled"] = cfg.time.ntpEnabled;
  time["twelve_hour"] = cfg.time.twelveHour;

  JsonObject display = doc["display"].to<JsonObject>();
  display["image_set"] = cfg.display.imageSet;
  display["lcd_brightness"] = cfg.display.lcdBrightness;

  JsonObject bl = doc["backlight"].to<JsonObject>();
  bl["pattern"] = patternName(cfg.backlight.pattern);
  bl["color_phase"] = cfg.backlight.colorPhase;
  bl["intensity"] = cfg.backlight.intensity;

  std::string out;
  serializeJsonPretty(doc, out);
  return out;
}

void
normalizeConfig(Config& cfg)
{
  cfg.display.lcdBrightness = clampInt(cfg.display.lcdBrightness, kLcdBrightnessMin, kLcdBrightnessMax);
  cfg.backlight.intensity = clampInt(cfg.backlight.intensity, 0, kPatternBrightnessMax);
  cfg.backlight.colorPhase = ((cfg.backlight.colorPhase % kColorPhaseMax) + kColorPhaseMax) % kColorPhaseMax;
  if (static_cast<int>(cfg.backlight.pattern) >= static_cast<int>(Pattern::Count))
    cfg.backlight.pattern = Pattern::Rainbow;
}

std::vector<std::string>
validateConfig(const Config& cfg)
{
  std::vector<std::string> p;
  if (cfg.version != kConfigVersion)
    p.push_back("unsupported config version");
  if (!cfg.name.empty() && !isValidHostName(cfg.name))
    p.push_back("name must be a valid host name (a-z, 0-9, '-')");

  if (cfg.wifi.mode != WifiMode::Off) {
    if (cfg.wifi.ssid.empty() || cfg.wifi.ssid.size() > 32)
      p.push_back("wifi.ssid must be 1-32 characters");
    if (!cfg.wifi.password.empty() && (cfg.wifi.password.size() < 8 || cfg.wifi.password.size() > 63))
      p.push_back("wifi.password must be empty (open network) or 8-63 characters");
  }
  if (cfg.wifi.mode == WifiMode::Static) {
    if (!isValidIPv4(cfg.wifi.ip))
      p.push_back("wifi.ip is not a valid IPv4 address");
    if (!isValidIPv4(cfg.wifi.gateway))
      p.push_back("wifi.gateway is not a valid IPv4 address");
    if (!isValidIPv4(cfg.wifi.netmask))
      p.push_back("wifi.netmask is not a valid IPv4 address");
    if (!cfg.wifi.dns.empty() && !isValidIPv4(cfg.wifi.dns))
      p.push_back("wifi.dns is not a valid IPv4 address");
  }

  if (cfg.api.enabled && !isValidToken(cfg.api.token))
    p.push_back("api.token must be 16-64 hex characters when the API is enabled");

  if (!isValidPosixTz(cfg.time.tzPosix))
    p.push_back("time.tz_posix is not a valid POSIX TZ rule");
  if (cfg.time.tzName.empty() || cfg.time.tzName.size() > 48)
    p.push_back("time.tz_name must be 1-48 characters");
  if (cfg.time.ntpEnabled && !isValidHostName(cfg.time.ntpServer) && !isValidIPv4(cfg.time.ntpServer))
    p.push_back("time.ntp_server must be a host name or IPv4 address");

  if (cfg.display.imageSet.size() > 20)
    p.push_back("display.image_set is too long");
  return p;
}

std::string
effectivePosixTz(const Config& cfg)
{
  if (cfg.time.dst)
    return cfg.time.tzPosix;
  std::string s = stripDst(cfg.time.tzPosix);
  return s.empty() ? cfg.time.tzPosix : s;
}

const char*
wifiModeName(WifiMode m)
{
  return kWifiModes[static_cast<int>(m)];
}

const char*
patternName(Pattern p)
{
  int i = static_cast<int>(p);
  return i < static_cast<int>(Pattern::Count) ? kPatterns[i] : "rainbow";
}

bool
patternFromName(const std::string& name, Pattern& out)
{
  for (int i = 0; i < static_cast<int>(Pattern::Count); ++i) {
    if (name == kPatterns[i]) {
      out = static_cast<Pattern>(i);
      return true;
    }
  }
  return false;
}

} // namespace clockcore
