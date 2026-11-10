/*
 * Tube Clock firmware
 *
 * Replacement firmware for EleksTube style IPS tube clocks, configured and managed by
 * the Tube Clock desktop app. Derived from EleksTubeHAX
 * (https://github.com/aly-fly/EleksTubeHAX, originally https://github.com/SmittyHalibut/EleksTubeHAX)
 * by Mark Smith, Aljaz Ogrin, Martinius79 and contributors.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <Arduino.h>
#include <esp_mac.h>

#include "ApiServer.h"
#include "App.h"
#include "Backlights.h"
#include "Buttons.h"
#include "Display.h"
#include "GLOBAL_DEFINES.h"
#include "Network.h"
#include "Storage.h"
#include "TimeKeeper.h"
#include "clockcore/Config.h"
#include "clockcore/TimeUtil.h"
#include "clockcore/TzTable.h"
#include "clockcore/Ui.h"

using namespace clockcore;

namespace {

Config gConfig;
std::string gHostName;
Display gDisplay;
Backlights gBacklights;
Buttons gButtons;
TimeKeeper gTimeKeeper;
Network gNetwork;
ApiServer gApi;
ImageLibrary gImages;

// Time zone that is configured but not part of the on-clock list (set by the app).
// It is offered as an extra "custom" entry at the end of the menu list.
std::string gCustomTzName, gCustomTzPosix;

bool gMenuDirty = true;
bool gClockDirty = true;
int gLastSecond = -1;
uint32_t gRebootAtMs = 0;
bool gApiRestart = false;

UiValues
snapshot();
UiController gUi(snapshot);

// ---- helpers -----------------------------------------------------------------------

std::string
defaultHostName()
{
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[24];
  snprintf(buf, sizeof(buf), "tubeclock-%02x%02x%02x", mac[3], mac[4], mac[5]);
  return buf;
}

bool
apiShouldRun()
{
  return gConfig.api.enabled && isValidToken(gConfig.api.token) && gNetwork.enabled();
}

void
updateApi()
{
  if (apiShouldRun())
    gApi.start(gConfig.api.token);
  else
    gApi.stop();
}

void
applyTimeZone()
{
  gTimeKeeper.applyTimeZone(effectivePosixTz(gConfig));
}

// Minutes east of UTC for the active TZ rule right now.
int
utcOffsetMinutes()
{
  const time_t now = time(nullptr);
  struct tm l;
  localtime_r(&now, &l);
  // days_from_civil (H. Hinnant) to turn the local broken-down time into seconds.
  int y = l.tm_year + 1900;
  const unsigned m = unsigned(l.tm_mon + 1), d = unsigned(l.tm_mday);
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = unsigned(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long long days = (long long)era * 146097 + (long long)doe - 719468;
  const long long localSecs = days * 86400 + l.tm_hour * 3600 + l.tm_min * 60 + l.tm_sec;
  return int((localSecs - (long long)now) / 60);
}

UiValues
snapshot()
{
  UiValues v;
  const struct tm t = gTimeKeeper.localTime();
  v.hour = t.tm_hour;
  v.minute = t.tm_min;
  v.year = t.tm_year + 1900;
  if (v.year < kMinYear)
    v.year = kMinYear;
  if (v.year > kMaxYear)
    v.year = kMaxYear;
  v.month = t.tm_mon + 1;
  v.day = t.tm_mday;

  const int idx = tzIndexOf(gConfig.time.tzName);
  if (idx >= 0) {
    gCustomTzName.clear();
    v.tzCount = tzTableSize();
    v.tz = idx;
  } else {
    gCustomTzName = gConfig.time.tzName;
    gCustomTzPosix = gConfig.time.tzPosix;
    v.tzCount = tzTableSize() + 1;
    v.tz = tzTableSize();
  }
  v.imageSetCount = gImages.sets().empty() ? 1 : int(gImages.sets().size());
  v.imageSet = gImages.currentIndex();
  v.pattern = int(gConfig.backlight.pattern);
  v.patternCount = int(Pattern::Count);
  v.patternBrightness = gConfig.backlight.intensity;
  v.lcdBrightness = gConfig.display.lcdBrightness;
  v.remoteAccess = gConfig.api.enabled;
  return v;
}

// Applies the live values of the menu to the config and the hardware.
void
applyLiveValues(const UiValues& v)
{
  if (v.tz < tzTableSize()) {
    gConfig.time.tzName = tzTable()[v.tz].name;
    gConfig.time.tzPosix = tzTable()[v.tz].posix;
  } else if (!gCustomTzName.empty()) {
    gConfig.time.tzName = gCustomTzName;
    gConfig.time.tzPosix = gCustomTzPosix;
  }
  applyTimeZone();

  if (!gImages.sets().empty() && v.imageSet < int(gImages.sets().size()))
    gConfig.display.imageSet = gImages.select(gImages.sets()[size_t(v.imageSet)]);

  gConfig.backlight.pattern = Pattern(v.pattern);
  gConfig.backlight.intensity = v.patternBrightness;
  gBacklights.refresh();

  gConfig.display.lcdBrightness = v.lcdBrightness;
  gDisplay.setBrightnessLevel(v.lcdBrightness);

  // Remote access needs a token from the app; without one it stays off.
  gConfig.api.enabled = v.remoteAccess && isValidToken(gConfig.api.token);
  updateApi();
}

void
saveConfig()
{
  if (!storage::saveConfig(gConfig))
    Serial.println("Config: save failed");
}

void
handleEffects(const UiEffects& fx)
{
  if (fx.displayOff) {
    gDisplay.powerOff();
    gBacklights.powerOff();
  }
  if (fx.displayOn) {
    gDisplay.powerOn();
    gBacklights.powerOn();
  }
  const UiValues& v = gUi.values();
  if (fx.applyLive)
    applyLiveValues(v);
  if (fx.commitTime) {
    const struct tm t = gTimeKeeper.localTime();
    gTimeKeeper.setLocal(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, v.hour, v.minute, 0);
  }
  if (fx.commitDate) {
    const struct tm t = gTimeKeeper.localTime();
    gTimeKeeper.setLocal(v.year, v.month, v.day, t.tm_hour, t.tm_min, t.tm_sec);
  }
  if (fx.saveConfig)
    saveConfig();
  if (fx.redraw || fx.applyLive || fx.displayOn) {
    gMenuDirty = true;
    gClockDirty = true;
  }
}

// ---- rendering --------------------------------------------------------------------

// What the tubes show: the clock, or the value being edited in the time/date menu.
Digits
currentDigits(const struct tm& t)
{
  if (gUi.screen() == UiScreen::Edit) {
    const UiValues& v = gUi.values();
    if (gUi.item() == MenuItem::Time)
      return timeDigits(v.hour, v.minute, 0, false);
    if (gUi.item() == MenuItem::Date)
      return dateDigits(v.day, v.month, v.year);
  }
  return gUi.clockMode() == ClockMode::Time ? timeDigits(t.tm_hour, t.tm_min, t.tm_sec, gConfig.time.twelveHour)
                                            : dateDigits(t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
}

void
renderDigits()
{
  const struct tm t = gTimeKeeper.localTime();
  if (!gClockDirty && t.tm_sec == gLastSecond)
    return;
  gLastSecond = t.tm_sec;
  gClockDirty = false;
  if (!gUi.inMenu()) {
    // Status badges (bottom bar) only outside the menu, where the legend lives.
    Display::Overlay first, last;
    first.label = gTimeKeeper.timeValid() ? "" : "Set time";
    last.label = gNetwork.offlineText();
    for (int s = 0; s < NUM_DIGITS; ++s)
      gDisplay.setOverlay(s, s == 0 ? first : (s == NUM_DIGITS - 1 ? last : Display::Overlay()));
  }
  const Digits d = currentDigits(t);
  // Right to left: the seconds change most often and should update first.
  for (int screen = NUM_DIGITS - 1; screen >= 0; --screen)
    gDisplay.showDigit(screen, d.v[screen], gImages.path(screen, d.v[screen]));
}

void
renderMenuOverlays()
{
  if (!gMenuDirty || !gUi.inMenu())
    return;
  gMenuDirty = false;
  const UiValues& v = gUi.values();
  const bool editing = gUi.screen() == UiScreen::Edit;
  const MenuItem item = gUi.item();
  char value[48] = "";
  char detail[48] = "";
  const char* field = "";

  switch (item) {
    case MenuItem::Time:
      snprintf(value, sizeof(value), "%02d:%02d", v.hour, v.minute);
      field = gUi.field() == EditField::Hour ? "Set hour" : (gUi.field() == EditField::Minute ? "Set minute" : "");
      break;
    case MenuItem::Date:
      snprintf(value, sizeof(value), "%02d.%02d.%04d", v.day, v.month, v.year);
      field = gUi.field() == EditField::Year    ? "Set year"
              : gUi.field() == EditField::Month ? "Set month"
              : gUi.field() == EditField::Day   ? "Set day"
                                                : "";
      break;
    case MenuItem::Timezone: {
      const char* name = v.tz < tzTableSize() ? tzTable()[v.tz].name : gCustomTzName.c_str();
      snprintf(value, sizeof(value), "%s", name);
      const int off = utcOffsetMinutes();
      snprintf(detail, sizeof(detail), "UTC%c%02d:%02d", off < 0 ? '-' : '+', abs(off) / 60, abs(off) % 60);
      break;
    }
    case MenuItem::ImageSet:
      snprintf(value, sizeof(value), "%s", gImages.current().c_str());
      snprintf(detail, sizeof(detail), "%d of %d", v.imageSet + 1, v.imageSetCount);
      break;
    case MenuItem::Pattern: {
      static const char* const names[] = { "Off", "Constant", "Rainbow", "Pulse", "Breath" };
      snprintf(value, sizeof(value), "%s", names[v.pattern % int(Pattern::Count)]);
      break;
    }
    case MenuItem::PatternBrightness:
      snprintf(value, sizeof(value), "%d", v.patternBrightness);
      snprintf(detail, sizeof(detail), "0 - 7");
      break;
    case MenuItem::LcdBrightness:
      snprintf(value, sizeof(value), "%d", v.lcdBrightness);
      snprintf(detail, sizeof(detail), "1 - 10");
      break;
    case MenuItem::RemoteAccess:
      snprintf(value, sizeof(value), "%s", v.remoteAccess ? "On" : "Off");
      if (!isValidToken(gConfig.api.token))
        snprintf(detail, sizeof(detail), "Not paired with app");
      else if (!gNetwork.enabled())
        snprintf(detail, sizeof(detail), "WiFi is off");
      else
        snprintf(detail, sizeof(detail), "%s", gNetwork.connected() ? gNetwork.ip().c_str() : "WiFi offline");
      break;
    default:
      break;
  }

  Display::Overlay o;
  o.title = UiController::itemTitle(item);
  o.detail = field;
  o.titleColor = 0xFD20;
  gDisplay.setOverlay(0, o);

  o = Display::Overlay();
  o.title = value;
  o.detail = detail;
  o.titleColor = editing ? 0xFD20 : TFT_WHITE;
  gDisplay.setOverlay(1, o);

  const Legend l = gUi.legend();
  const Display::Icon icons[4] = {
    Display::Icon::Left, Display::Icon::Mode, Display::Icon::Right, Display::Icon::Power
  };
  const char* labels[4] = { l.left, l.mode, l.right, l.power };
  for (int i = 0; i < 4; ++i) {
    o = Display::Overlay();
    o.icon = icons[i];
    o.label = labels[i];
    gDisplay.setOverlay(2 + i, o);
  }
}

void
bootScreen()
{
  gDisplay.drawBootScreen(0, "Tube Clock", "v" FIRMWARE_VERSION);
  gDisplay.drawBootScreen(1, "Starting", gHostName.c_str());
}

} // namespace

// ---- services for the HTTP API (App.h) ----------------------------------------------

namespace app {

const Config&
config()
{
  return gConfig;
}

bool
patchConfig(const char* json, size_t len, std::string* error, bool* rebootRequired)
{
  Config next = gConfig;
  if (!applyConfigPatch(json, len, next, error))
    return false;
  const auto &a = gConfig.wifi, &b = next.wifi;
  const bool networkChanged = a.mode != b.mode || a.ssid != b.ssid || a.password != b.password || a.ip != b.ip ||
                              a.gateway != b.gateway || a.netmask != b.netmask || a.dns != b.dns ||
                              gConfig.name != next.name;
  const bool tokenChanged = gConfig.api.token != next.api.token;
  // Network settings are applied after a restart; keep the running ones until then.
  Config applied = next;
  applied.wifi = gConfig.wifi;
  applied.name = gConfig.name;
  gConfig = applied;
  if (!storage::saveConfig(next)) {
    if (error)
      *error = "saving the configuration failed";
    return false;
  }
  if (rebootRequired)
    *rebootRequired = networkChanged;

  applyTimeZone();
  gConfig.display.imageSet = gImages.select(gConfig.display.imageSet);
  gDisplay.setBrightnessLevel(gConfig.display.lcdBrightness);
  gBacklights.refresh();
  // The server cannot be restarted from inside its own request handler; loop() does it.
  if (tokenChanged)
    gApiRestart = true;
  gDisplay.invalidate();
  gClockDirty = gMenuDirty = true;
  return true;
}

void
setUtcTime(time_t utc)
{
  gTimeKeeper.setUtc(utc, "app");
  gClockDirty = true;
}

void
filesChanged()
{
  gImages.scan();
  gConfig.display.imageSet = gImages.select(gConfig.display.imageSet);
  gDisplay.invalidate();
  gClockDirty = gMenuDirty = true;
}

void
scheduleReboot(uint32_t delayMs)
{
  gRebootAtMs = millis() + delayMs;
}

void
setDisplayPower(bool on)
{
  handleEffects(gUi.setDisplay(on));
}

bool
displayOn()
{
  return gDisplay.isOn();
}
ImageLibrary&
images()
{
  return gImages;
}
TimeKeeper&
timeKeeper()
{
  return gTimeKeeper;
}
Network&
network()
{
  return gNetwork;
}
const std::string&
hostName()
{
  return gHostName;
}

} // namespace app

// ---- Arduino entry points -----------------------------------------------------------

void
setup()
{
  Serial.begin(115200);
  Serial.println("\nTube Clock v" FIRMWARE_VERSION " (" DEVICE_MODEL_NAME ")");

  storage::begin();
  storage::loadConfig(gConfig);
  gHostName = gConfig.name.empty() ? defaultHostName() : gConfig.name;

  gDisplay.setBrightnessLevel(gConfig.display.lcdBrightness);
  gDisplay.begin();
  bootScreen();

  gBacklights.begin(&gConfig.backlight);
  gButtons.begin();

  gImages.scan();
  gConfig.display.imageSet = gImages.select(gConfig.display.imageSet);

  applyTimeZone();
  gTimeKeeper.begin();

  gNetwork.begin(gConfig.wifi, gHostName);
  updateApi();

  gDisplay.invalidate();
  gClockDirty = true;
}

void
loop()
{
  gButtons.loop();
  const uint32_t now = millis();
  if (gButtons.left.pressed())
    handleEffects(gUi.press(clockcore::Button::Left, now));
  if (gButtons.mode.pressed())
    handleEffects(gUi.press(clockcore::Button::Mode, now));
  if (gButtons.right.pressed())
    handleEffects(gUi.press(clockcore::Button::Right, now));
  if (gButtons.power.pressed())
    handleEffects(gUi.press(clockcore::Button::Power, now));
  handleEffects(gUi.tick(now));

  gNetwork.loop();
  if (gApiRestart) {
    gApiRestart = false;
    gApi.stop();
  }
  if (apiShouldRun() != gApi.running())
    updateApi();
  gApi.loop();
  gTimeKeeper.loop(gNetwork.connected(), gConfig.time.ntpEnabled, gConfig.time.ntpServer);
  gBacklights.loop();

  if (gDisplay.isOn()) {
    renderMenuOverlays();
    renderDigits();
  }

  if (gRebootAtMs != 0 && int32_t(millis() - gRebootAtMs) >= 0)
    ESP.restart();

  delay(5);
}
