# Clock controls

The buttons, from left to right: **◀ left**, **● middle (mode)**, **▶ right**, **⏻ power**.

## Clock face

| Button | Action |
| --- | --- |
| ◀ or ▶ | Switch between **time** (`HH MM SS`) and **date** (`DD MM YY`) |
| ● | Open the settings menu |
| ⏻ | Turn the tubes and the backlight LEDs off. The clock keeps running: time, WiFi, time sync and remote access stay active. Any button turns the tubes on again. |

## Settings menu

While the menu is open, the tubes keep showing the clock, so changes to the image set, brightness or backlight
are visible right away. Small bars are drawn over the digits:

- **tube 1** (top bar): the menu item, and while editing, which part (e.g. "Set hour")
- **tube 2** (top bar): the current value
- **tubes 3-6** (bottom bars): what each button does, with an icon, in the order of the buttons

| | ◀ | ● | ▶ | ⏻ |
| --- | --- | --- | --- | --- |
| Choosing an item | Prev | Enter | Next | Exit |
| Changing a value | − | OK / Next | + | Back |

The menu closes by itself after 30 seconds without a button press. Changes that were accepted with ● are saved;
a value that was being changed but not accepted is reverted.

### Items

| Item | How it works |
| --- | --- |
| **Time** | ● → set the **hour** with ◀/▶ → ● → set the **minutes** → ● saves (seconds start at 00). While editing, the tubes show the new time. |
| **Date** | ● → **year** → ● → **month** → ● → **day** → ● saves. The day is limited to the length of the month (29 February only in leap years). |
| **Timezone** | ◀/▶ go through a list of about 70 common zones; the offset from UTC is shown under the name. A zone set by the app that is not in the list appears as an extra entry. |
| **Image set** | Switches between the image sets stored on the clock. Only shown if there is more than one. |
| **Pattern** | Backlight pattern: Off, Constant, Rainbow, Pulse, Breath. The colour is set in the app. |
| **Pattern brightness** | Backlight brightness 0-7. |
| **LCD brightness** | Tube brightness 1-10. |
| **Remote access** | On/Off for control by the app over WiFi. Needs a clock that was paired with the app ("Not paired with app" otherwise). |

⏻ while changing a value cancels the change; ⏻ in the item list leaves the menu.

## Status messages

| Message | Meaning |
| --- | --- |
| "Set time" (bottom of tube 1) | The clock has no valid time. Set it in the menu or from the app. |
| "No WiFi" (bottom of tube 6) | WiFi is configured but has been disconnected for more than a minute. |

## Time keeping

- The time is kept in UTC by the clock's battery-backed RTC chip and converted with the time zone's rules, so
  summer time changes happen automatically.
- With WiFi and network time enabled, the clock asks the time server once after starting and then at most once a
  day. Until the first sync has succeeded, a failed request is retried after a minute; later failures are retried
  after an hour.
- Times set in the menu or from the app are written to the RTC chip immediately.
