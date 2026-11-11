// SPDX-License-Identifier: GPL-3.0-or-later
#include "clockcore/Ui.hpp"

#include "clockcore/TimeUtil.hpp"

namespace clockcore
{

namespace
{

int wrap(int v, int count)
{
    if (count <= 0)
    {
        return 0;
    }

    return ((v % count) + count) % count;
}

int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

bool isLiveItem(MenuItem item)
{
    return item == MenuItem::Timezone || item == MenuItem::ImageSet || item == MenuItem::Pattern
        || item == MenuItem::PatternBrightness || item == MenuItem::LcdBrightness || item == MenuItem::RemoteAccess;
}

} // namespace

const char* UiController::itemTitle(MenuItem item)
{
    switch (item)
    {
        case MenuItem::Time:
            return "Time";
        case MenuItem::Date:
            return "Date";
        case MenuItem::Timezone:
            return "Timezone";
        case MenuItem::ImageSet:
            return "Image set";
        case MenuItem::Pattern:
            return "Pattern";
        case MenuItem::PatternBrightness:
            return "Pattern brightness";
        case MenuItem::LcdBrightness:
            return "LCD brightness";
        case MenuItem::RemoteAccess:
            return "Remote access";
        default:
            return "";
    }
}

bool UiController::itemVisible(MenuItem item) const
{
    if (item == MenuItem::ImageSet)
    {
        return values_.imageSetCount > 1;
    }

    return item < MenuItem::Count;
}

Legend UiController::legend() const
{
    if (screen_ == UiScreen::MenuList)
    {
        return { "Prev", "Enter", "Next", "Exit" };
    }

    if (screen_ == UiScreen::Edit)
    {
        const bool more = field_ == EditField::Hour || field_ == EditField::Year || field_ == EditField::Month;
        return { "-", more ? "Next" : "OK", "+", "Back" };
    }

    return { "", "", "", "" };
}

UiEffects UiController::press(Button b, uint32_t nowMs)
{
    lastInputMs_ = nowMs;
    switch (screen_)
    {
        case UiScreen::DisplayOff: {
            UiEffects fx;
            screen_ = UiScreen::Clock;
            fx.displayOn = true;
            fx.redraw = true;
            return fx;
        }

        case UiScreen::Clock:
            return pressClock(b);
        case UiScreen::MenuList:
            return pressMenuList(b);
        case UiScreen::Edit:
            return pressEdit(b);
    }

    return {};
}

UiEffects UiController::tick(uint32_t nowMs)
{
    if (!inMenu() || nowMs - lastInputMs_ < kTimeoutMs)
    {
        return {};
    }

    UiEffects fx;
    if (screen_ == UiScreen::Edit && isLiveItem(item_))
    {
        values_ = original_;
        fx.applyLive = true;
    }

    UiEffects leave = leaveMenu();
    fx.redraw = leave.redraw;
    fx.saveConfig = leave.saveConfig;
    return fx;
}

UiEffects UiController::setDisplay(bool on)
{
    UiEffects fx;
    if (on)
    {
        if (screen_ == UiScreen::DisplayOff)
        {
            screen_ = UiScreen::Clock;
            fx.displayOn = true;
            fx.redraw = true;
        }

        return fx;
    }

    if (screen_ == UiScreen::DisplayOff)
    {
        return fx;
    }

    if (inMenu())
    {
        // Same as a timeout: revert an unfinished live edit, keep accepted changes.
        fx = tick(lastInputMs_ + kTimeoutMs);
    }

    screen_ = UiScreen::DisplayOff;
    fx.displayOff = true;
    fx.redraw = false;
    return fx;
}

UiEffects UiController::pressClock(Button b)
{
    UiEffects fx;
    switch (b)
    {
        case Button::Left:
        case Button::Right:
            mode_ = mode_ == ClockMode::Time ? ClockMode::Date : ClockMode::Time;
            fx.redraw = true;
            break;
        case Button::Mode:
            values_ = snapshot_ ? snapshot_() : UiValues{};
            screen_ = UiScreen::MenuList;
            item_ = MenuItem::Time;
            field_ = EditField::None;
            dirty_ = false;
            fx.redraw = true;
            break;
        case Button::Power:
            screen_ = UiScreen::DisplayOff;
            fx.displayOff = true;
            break;
    }

    return fx;
}

UiEffects UiController::pressMenuList(Button b)
{
    UiEffects fx;
    switch (b)
    {
        case Button::Left:
            stepItem(-1);
            fx.redraw = true;
            break;
        case Button::Right:
            stepItem(+1);
            fx.redraw = true;
            break;
        case Button::Mode:
            beginEdit();
            fx.redraw = true;
            break;
        case Button::Power:
            return leaveMenu();
    }

    return fx;
}

UiEffects UiController::pressEdit(Button b)
{
    UiEffects fx;
    fx.redraw = true;
    switch (b)
    {
        case Button::Left:
            changeValue(-1, fx);
            break;
        case Button::Right:
            changeValue(+1, fx);
            break;
        case Button::Power:
            // Cancel: restore the values from before the edit.
            if (isLiveItem(item_))
            {
                values_ = original_;
                fx.applyLive = true;
            }
            else
            {
                values_ = original_;
            }

            screen_ = UiScreen::MenuList;
            field_ = EditField::None;
            break;
        case Button::Mode:
            switch (field_)
            {
                case EditField::Hour:
                    field_ = EditField::Minute;
                    break;
                case EditField::Minute:
                    fx.commitTime = true;
                    screen_ = UiScreen::MenuList;
                    field_ = EditField::None;
                    break;
                case EditField::Year:
                    field_ = EditField::Month;
                    break;
                case EditField::Month:
                    field_ = EditField::Day;
                    break;
                case EditField::Day:
                    fx.commitDate = true;
                    screen_ = UiScreen::MenuList;
                    field_ = EditField::None;
                    break;
                default: {
                    // Accept a live value. It was already applied while changing it.
                    const UiValues& o = original_;
                    const UiValues& v = values_;
                    if (o.tz != v.tz || o.imageSet != v.imageSet || o.pattern != v.pattern
                        || o.patternBrightness != v.patternBrightness || o.lcdBrightness != v.lcdBrightness
                        || o.remoteAccess != v.remoteAccess)
                    {
                        dirty_ = true;
                    }

                    screen_ = UiScreen::MenuList;
                    field_ = EditField::None;
                    break;
                }
            }

            break;
    }

    return fx;
}

UiEffects UiController::leaveMenu()
{
    UiEffects fx;
    screen_ = UiScreen::Clock;
    field_ = EditField::None;
    fx.redraw = true;
    fx.saveConfig = dirty_;
    dirty_ = false;
    return fx;
}

void UiController::stepItem(int dir)
{
    const int count = static_cast<int>(MenuItem::Count);
    int i = static_cast<int>(item_);
    for (int n = 0; n < count; ++n)
    {
        i = wrap(i + dir, count);
        if (itemVisible(static_cast<MenuItem>(i)))
        {
            break;
        }
    }

    item_ = static_cast<MenuItem>(i);
}

void UiController::beginEdit()
{
    original_ = values_;
    screen_ = UiScreen::Edit;
    switch (item_)
    {
        case MenuItem::Time:
            field_ = EditField::Hour;
            break;
        case MenuItem::Date:
            field_ = EditField::Year;
            break;
        default:
            field_ = EditField::Value;
            break;
    }
}

void UiController::clampDay()
{
    const int dim = daysInMonth(values_.year, values_.month);
    if (values_.day > dim)
    {
        values_.day = dim;
    }

    if (values_.day < 1)
    {
        values_.day = 1;
    }
}

void UiController::changeValue(int dir, UiEffects& fx)
{
    UiValues& v = values_;
    switch (field_)
    {
        case EditField::Hour:
            v.hour = wrap(v.hour + dir, 24);
            return;
        case EditField::Minute:
            v.minute = wrap(v.minute + dir, 60);
            return;
        case EditField::Year:
            v.year = kMinYear + wrap(v.year - kMinYear + dir, kMaxYear - kMinYear + 1);
            clampDay();
            return;
        case EditField::Month:
            v.month = 1 + wrap(v.month - 1 + dir, 12);
            clampDay();
            return;
        case EditField::Day:
            v.day = 1 + wrap(v.day - 1 + dir, daysInMonth(v.year, v.month));
            return;
        default:
            break;
    }

    switch (item_)
    {
        case MenuItem::Timezone:
            v.tz = wrap(v.tz + dir, v.tzCount);
            break;
        case MenuItem::ImageSet:
            v.imageSet = wrap(v.imageSet + dir, v.imageSetCount);
            break;
        case MenuItem::Pattern:
            v.pattern = wrap(v.pattern + dir, v.patternCount);
            break;
        case MenuItem::PatternBrightness:
            v.patternBrightness = clamp(v.patternBrightness + dir, 0, 7);
            break;
        case MenuItem::LcdBrightness:
            v.lcdBrightness = clamp(v.lcdBrightness + dir, 1, 10);
            break;
        case MenuItem::RemoteAccess:
            v.remoteAccess = !v.remoteAccess;
            break;
        default:
            return;
    }

    fx.applyLive = true;
}

} // namespace clockcore
