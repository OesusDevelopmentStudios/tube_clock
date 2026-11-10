// SPDX-License-Identifier: GPL-3.0-or-later
//
// Button driven user interface of the clock as a pure state machine.
//
//   Clock screen   : LEFT/RIGHT toggle time <-> date, MODE opens the menu,
//                    POWER switches the displays off (the clock keeps running).
//   Display off    : any button switches the displays back on.
//   Menu list      : LEFT/RIGHT previous/next item, MODE enters the item, POWER exits.
//   Editing        : LEFT/RIGHT change the value, MODE accepts (time and date step
//                    through hour -> minute and year -> month -> day), POWER cancels.
//
// The firmware feeds button presses and the current time into the controller and
// acts on the returned UiEffects. Nothing in here touches hardware.
#pragma once

#include <cstdint>
#include <functional>

namespace clockcore {

enum class Button : uint8_t
{
  Left,
  Mode,
  Right,
  Power
};
enum class UiScreen : uint8_t
{
  Clock,
  DisplayOff,
  MenuList,
  Edit
};
enum class ClockMode : uint8_t
{
  Time,
  Date
};
enum class MenuItem : uint8_t
{
  Time,
  Date,
  Timezone,
  ImageSet,
  Pattern,
  PatternBrightness,
  LcdBrightness,
  RemoteAccess,
  Count
};
enum class EditField : uint8_t
{
  None,
  Hour,
  Minute,
  Year,
  Month,
  Day,
  Value
};

constexpr int kMinYear = 2024;
constexpr int kMaxYear = 2099;

// Values shown and edited in the menu. Indices refer to lists owned by the firmware
// (time zone table, image sets, backlight patterns).
struct UiValues
{
  int hour = 0, minute = 0;
  int year = kMinYear, month = 1, day = 1;
  int tz = 0, tzCount = 1;
  int imageSet = 0, imageSetCount = 1;
  int pattern = 0, patternCount = 1;
  int patternBrightness = 7; // 0..7
  int lcdBrightness = 10;    // 1..10
  bool remoteAccess = true;
};

struct UiEffects
{
  bool redraw = false;     // the visible content changed
  bool displayOn = false;  // switch display power on
  bool displayOff = false; // switch display power off
  bool applyLive = false;  // a live setting (tz, set, pattern, brightness, remote) changed: apply values()
  bool commitTime = false; // set the clock to values().hour:minute:00 (today, local time)
  bool commitDate = false; // set the date to values().year-month-day (keep the time)
  bool saveConfig = false; // persist settings (sent when the menu is left)
};

struct Legend
{
  const char* left;
  const char* mode;
  const char* right;
  const char* power;
};

class UiController
{
public:
  using Snapshot = std::function<UiValues()>;
  static constexpr uint32_t kTimeoutMs = 30000;

  explicit UiController(Snapshot snapshot)
    : snapshot_(std::move(snapshot))
  {
  }

  UiEffects press(Button b, uint32_t nowMs);
  UiEffects tick(uint32_t nowMs); // handles the menu timeout
  // Remote display power control. Turning off leaves an open menu (pending edits are
  // discarded like on a timeout).
  UiEffects setDisplay(bool on);

  UiScreen screen() const { return screen_; }
  ClockMode clockMode() const { return mode_; }
  MenuItem item() const { return item_; }
  EditField field() const { return field_; }
  const UiValues& values() const { return values_; }
  bool inMenu() const { return screen_ == UiScreen::MenuList || screen_ == UiScreen::Edit; }
  bool itemVisible(MenuItem item) const;
  Legend legend() const;

  static const char* itemTitle(MenuItem item);

private:
  UiEffects pressClock(Button b);
  UiEffects pressMenuList(Button b);
  UiEffects pressEdit(Button b);
  UiEffects leaveMenu();
  void stepItem(int dir);
  void beginEdit();
  void changeValue(int dir, UiEffects& fx);
  void clampDay();

  Snapshot snapshot_;
  UiScreen screen_ = UiScreen::Clock;
  ClockMode mode_ = ClockMode::Time;
  MenuItem item_ = MenuItem::Time;
  EditField field_ = EditField::None;
  UiValues values_;
  UiValues original_; // values when the current edit started (restored on cancel)
  bool dirty_ = false;
  uint32_t lastInputMs_ = 0;
};

} // namespace clockcore
