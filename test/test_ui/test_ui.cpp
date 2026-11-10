// SPDX-License-Identifier: GPL-3.0-or-later
#include <unity.h>

#include "clockcore/Ui.h"

using namespace clockcore;

void
setUp()
{
}
void
tearDown()
{
}

static UiValues
snapshot()
{
  UiValues v;
  v.hour = 10;
  v.minute = 30;
  v.year = 2026;
  v.month = 1;
  v.day = 31;
  v.tz = 2;
  v.tzCount = 5;
  v.imageSet = 0;
  v.imageSetCount = 3;
  v.pattern = 1;
  v.patternCount = 5;
  v.patternBrightness = 7;
  v.lcdBrightness = 10;
  v.remoteAccess = true;
  return v;
}

static uint32_t now = 1000;
static UiEffects
press(UiController& ui, Button b)
{
  return ui.press(b, now += 100);
}

void
test_arrows_toggle_time_and_date()
{
  UiController ui(snapshot);
  TEST_ASSERT_EQUAL(int(ClockMode::Time), int(ui.clockMode()));
  TEST_ASSERT_TRUE(press(ui, Button::Right).redraw);
  TEST_ASSERT_EQUAL(int(ClockMode::Date), int(ui.clockMode()));
  press(ui, Button::Left);
  TEST_ASSERT_EQUAL(int(ClockMode::Time), int(ui.clockMode()));
}

void
test_power_turns_display_off_and_any_key_on()
{
  UiController ui(snapshot);
  UiEffects fx = press(ui, Button::Power);
  TEST_ASSERT_TRUE(fx.displayOff);
  TEST_ASSERT_EQUAL(int(UiScreen::DisplayOff), int(ui.screen()));
  fx = press(ui, Button::Mode); // consumed: does not open the menu
  TEST_ASSERT_TRUE(fx.displayOn);
  TEST_ASSERT_EQUAL(int(UiScreen::Clock), int(ui.screen()));
}

void
test_menu_navigation_and_legend()
{
  UiController ui(snapshot);
  press(ui, Button::Mode);
  TEST_ASSERT_EQUAL(int(UiScreen::MenuList), int(ui.screen()));
  TEST_ASSERT_EQUAL(int(MenuItem::Time), int(ui.item()));
  Legend l = ui.legend();
  TEST_ASSERT_EQUAL_STRING("Prev", l.left);
  TEST_ASSERT_EQUAL_STRING("Enter", l.mode);
  TEST_ASSERT_EQUAL_STRING("Next", l.right);
  TEST_ASSERT_EQUAL_STRING("Exit", l.power);

  const MenuItem order[] = {
    MenuItem::Date,          MenuItem::Timezone,     MenuItem::ImageSet, MenuItem::Pattern, MenuItem::PatternBrightness,
    MenuItem::LcdBrightness, MenuItem::RemoteAccess, MenuItem::Time
  };
  for (MenuItem expected : order) {
    press(ui, Button::Right);
    TEST_ASSERT_EQUAL(int(expected), int(ui.item()));
  }
  press(ui, Button::Left);
  TEST_ASSERT_EQUAL(int(MenuItem::RemoteAccess), int(ui.item()));

  UiEffects fx = press(ui, Button::Power);
  TEST_ASSERT_EQUAL(int(UiScreen::Clock), int(ui.screen()));
  TEST_ASSERT_FALSE(fx.saveConfig); // nothing changed
}

void
test_image_set_item_hidden_with_one_set()
{
  UiController ui([] {
    UiValues v = snapshot();
    v.imageSetCount = 1;
    return v;
  });
  press(ui, Button::Mode);
  press(ui, Button::Right); // Date
  press(ui, Button::Right); // Timezone
  press(ui, Button::Right); // skips ImageSet
  TEST_ASSERT_EQUAL(int(MenuItem::Pattern), int(ui.item()));
}

void
test_set_time_hour_then_minute()
{
  UiController ui(snapshot);
  press(ui, Button::Mode); // menu, item Time
  press(ui, Button::Mode); // edit hour
  TEST_ASSERT_EQUAL(int(EditField::Hour), int(ui.field()));
  TEST_ASSERT_EQUAL_STRING("Next", ui.legend().mode);
  TEST_ASSERT_EQUAL_STRING("Back", ui.legend().power);
  for (int i = 0; i < 15; ++i)
    press(ui, Button::Right); // 10 -> 25 wraps to 1
  TEST_ASSERT_EQUAL(1, ui.values().hour);
  press(ui, Button::Left);
  press(ui, Button::Left); // 23
  TEST_ASSERT_EQUAL(23, ui.values().hour);
  press(ui, Button::Mode); // minutes
  TEST_ASSERT_EQUAL(int(EditField::Minute), int(ui.field()));
  TEST_ASSERT_EQUAL_STRING("OK", ui.legend().mode);
  for (int i = 0; i < 31; ++i)
    press(ui, Button::Right); // 30 -> 61 wraps to 1
  TEST_ASSERT_EQUAL(1, ui.values().minute);
  UiEffects fx = press(ui, Button::Mode);
  TEST_ASSERT_TRUE(fx.commitTime);
  TEST_ASSERT_EQUAL(int(UiScreen::MenuList), int(ui.screen()));
}

void
test_cancel_time_edit()
{
  UiController ui(snapshot);
  press(ui, Button::Mode);
  press(ui, Button::Mode);
  press(ui, Button::Right);
  UiEffects fx = press(ui, Button::Power);
  TEST_ASSERT_FALSE(fx.commitTime);
  TEST_ASSERT_EQUAL(10, ui.values().hour);
  TEST_ASSERT_EQUAL(int(UiScreen::MenuList), int(ui.screen()));
}

void
test_set_date_year_month_day_with_clamping()
{
  UiController ui(snapshot); // 2026-01-31
  press(ui, Button::Mode);
  press(ui, Button::Right); // Date
  press(ui, Button::Mode);  // year
  TEST_ASSERT_EQUAL(int(EditField::Year), int(ui.field()));
  press(ui, Button::Right);
  press(ui, Button::Right); // 2028 (leap year)
  press(ui, Button::Mode);  // month
  TEST_ASSERT_EQUAL(int(EditField::Month), int(ui.field()));
  press(ui, Button::Right); // February -> day clamps to 29
  TEST_ASSERT_EQUAL(2, ui.values().month);
  TEST_ASSERT_EQUAL(29, ui.values().day);
  press(ui, Button::Mode); // day
  TEST_ASSERT_EQUAL(int(EditField::Day), int(ui.field()));
  press(ui, Button::Right); // 29 -> wraps to 1
  TEST_ASSERT_EQUAL(1, ui.values().day);
  press(ui, Button::Left); // back to 29
  TEST_ASSERT_EQUAL(29, ui.values().day);
  UiEffects fx = press(ui, Button::Mode);
  TEST_ASSERT_TRUE(fx.commitDate);
  TEST_ASSERT_EQUAL(2028, ui.values().year);
}

void
test_year_wraps_in_range()
{
  UiController ui([] {
    UiValues v = snapshot();
    v.year = kMinYear;
    return v;
  });
  press(ui, Button::Mode);
  press(ui, Button::Right);
  press(ui, Button::Mode);
  press(ui, Button::Left);
  TEST_ASSERT_EQUAL(kMaxYear, ui.values().year);
}

void
test_live_value_apply_accept_and_save()
{
  UiController ui(snapshot);
  press(ui, Button::Mode);
  for (int i = 0; i < 6; ++i)
    press(ui, Button::Right); // LcdBrightness
  TEST_ASSERT_EQUAL(int(MenuItem::LcdBrightness), int(ui.item()));
  press(ui, Button::Mode);
  UiEffects fx = press(ui, Button::Right); // already at max
  TEST_ASSERT_EQUAL(10, ui.values().lcdBrightness);
  fx = press(ui, Button::Left);
  TEST_ASSERT_TRUE(fx.applyLive);
  TEST_ASSERT_EQUAL(9, ui.values().lcdBrightness);
  press(ui, Button::Mode);       // accept
  fx = press(ui, Button::Power); // exit menu
  TEST_ASSERT_TRUE(fx.saveConfig);
}

void
test_live_value_cancel_restores()
{
  UiController ui(snapshot);
  press(ui, Button::Mode);
  press(ui, Button::Right);
  press(ui, Button::Right); // Timezone
  press(ui, Button::Mode);
  press(ui, Button::Right);
  press(ui, Button::Right);
  press(ui, Button::Right); // 2 -> 5 wraps to 0
  TEST_ASSERT_EQUAL(0, ui.values().tz);
  UiEffects fx = press(ui, Button::Power);
  TEST_ASSERT_TRUE(fx.applyLive);
  TEST_ASSERT_EQUAL(2, ui.values().tz);
  fx = press(ui, Button::Power);
  TEST_ASSERT_FALSE(fx.saveConfig);
}

void
test_brightness_clamps()
{
  UiController ui(snapshot);
  press(ui, Button::Mode);
  for (int i = 0; i < 5; ++i)
    press(ui, Button::Right); // PatternBrightness
  press(ui, Button::Mode);
  for (int i = 0; i < 10; ++i)
    press(ui, Button::Left);
  TEST_ASSERT_EQUAL(0, ui.values().patternBrightness);
}

void
test_timeout_reverts_and_saves_accepted()
{
  UiController ui(snapshot);
  press(ui, Button::Mode);
  for (int i = 0; i < 7; ++i)
    press(ui, Button::Right); // RemoteAccess
  press(ui, Button::Mode);
  press(ui, Button::Right); // toggle off
  press(ui, Button::Mode);  // accept
  TEST_ASSERT_FALSE(ui.values().remoteAccess);
  press(ui, Button::Left); // LcdBrightness
  press(ui, Button::Mode);
  press(ui, Button::Left); // 9, not accepted
  TEST_ASSERT_EQUAL(9, ui.values().lcdBrightness);

  TEST_ASSERT_FALSE(ui.tick(now + UiController::kTimeoutMs - 1).redraw);
  UiEffects fx = ui.tick(now + UiController::kTimeoutMs);
  TEST_ASSERT_TRUE(fx.applyLive);
  TEST_ASSERT_TRUE(fx.saveConfig);
  TEST_ASSERT_EQUAL(int(UiScreen::Clock), int(ui.screen()));
  TEST_ASSERT_EQUAL(10, ui.values().lcdBrightness);
  TEST_ASSERT_FALSE(ui.values().remoteAccess);
}

void
test_remote_display_control()
{
  UiController ui(snapshot);
  UiEffects fx = ui.setDisplay(true); // already on
  TEST_ASSERT_FALSE(fx.displayOn);
  press(ui, Button::Mode); // open the menu
  press(ui, Button::Right);
  press(ui, Button::Right); // Timezone
  press(ui, Button::Mode);
  press(ui, Button::Right); // live change, not accepted
  fx = ui.setDisplay(false);
  TEST_ASSERT_TRUE(fx.displayOff);
  TEST_ASSERT_TRUE(fx.applyLive); // reverted
  TEST_ASSERT_EQUAL(2, ui.values().tz);
  TEST_ASSERT_EQUAL(int(UiScreen::DisplayOff), int(ui.screen()));
  fx = ui.setDisplay(true);
  TEST_ASSERT_TRUE(fx.displayOn);
  TEST_ASSERT_EQUAL(int(UiScreen::Clock), int(ui.screen()));
}

int
main(int, char**)
{
  UNITY_BEGIN();
  RUN_TEST(test_arrows_toggle_time_and_date);
  RUN_TEST(test_power_turns_display_off_and_any_key_on);
  RUN_TEST(test_menu_navigation_and_legend);
  RUN_TEST(test_image_set_item_hidden_with_one_set);
  RUN_TEST(test_set_time_hour_then_minute);
  RUN_TEST(test_cancel_time_edit);
  RUN_TEST(test_set_date_year_month_day_with_clamping);
  RUN_TEST(test_year_wraps_in_range);
  RUN_TEST(test_live_value_apply_accept_and_save);
  RUN_TEST(test_live_value_cancel_restores);
  RUN_TEST(test_brightness_clamps);
  RUN_TEST(test_timeout_reverts_and_saves_accepted);
  RUN_TEST(test_remote_display_control);
  return UNITY_END();
}
