# Credits

## Original project: EleksTubeHAX

Tube Clock is a fork of **EleksTubeHAX**, an aftermarket firmware for EleksTube style IPS clocks.

- Original repository: <https://github.com/SmittyHalibut/EleksTubeHAX>
- Maintained fork it is based on: <https://github.com/aly-fly/EleksTubeHAX>
- Where it started: [Reddit: Hacking the EleksTube IPS clock](https://www.reddit.com/r/arduino/comments/mq5td9/hacking_the_elekstube_ips_clock_anyone_tried_it/)

EleksTubeHAX authors and contributors:

- **Mark Smith** (Smitty), [@SmittyHalibut](https://github.com/SmittyHalibut): original firmware and the hardware
  reverse engineering ([docs/hardware/reverse-engineering-notes.md](docs/hardware/reverse-engineering-notes.md))
- **Aljaz Ogrin** (aly-fly), [@aly-fly](https://github.com/aly-fly): maintainer, clock faces, many features
- **Clemens Sutor** (Martinius79), [@Martinius79](https://github.com/Martinius79): support for many clock models,
  maintenance
- Code contributions by @icebreaker-ch, @meddle99, @OggyP and @bitrot-alpha
- Home Assistant support by @victorvuelma and @gamba69
- MarvelTubes Mini integration help by @Hans99

From EleksTubeHAX, Tube Clock still uses or builds on:

- the pin mappings and hardware knowledge of the supported clocks (`src/GLOBAL_DEFINES.hpp`)
- the display chip select, button and backlight drivers (`src/ChipSelect.cpp`, `src/Buttons.cpp`, `src/Backlights.cpp`)
- the patched TFT_eSPI copy (`lib/modified_TFT_eSPI`)
- the bundled clock faces (`data/`, converted to PNG) and the artwork in `assets/`
- the hardware photos and notes in `docs/hardware/`

## Third-party software

### Firmware

| Component | Author | License | Source |
| --- | --- | --- | --- |
| TFT_eSPI (modified copy in `lib/modified_TFT_eSPI`) | Bodmer | FreeBSD + MIT (see the library) | <https://github.com/Bodmer/TFT_eSPI> |
| Adafruit NeoPixel | Adafruit | LGPL-3.0 | <https://github.com/adafruit/Adafruit_NeoPixel> |
| RTClib | Adafruit | MIT | <https://github.com/adafruit/RTClib> |
| Rtc (DS1302) | Michael C. Miller (Makuna) | LGPL-3.0 | <https://github.com/Makuna/Rtc> |
| ArduinoJson | Benoît Blanchon | MIT | <https://github.com/bblanchon/ArduinoJson> |
| pngle (PNG decoder, includes miniz) | kikuchan | MIT | <https://github.com/kikuchan/pngle> |
| Arduino core for the ESP32 | Espressif | LGPL-2.1 | <https://github.com/espressif/arduino-esp32> |

### Desktop app

| Component | Author | License | Source |
| --- | --- | --- | --- |
| Qt 6 | The Qt Company | LGPL-3.0 | <https://www.qt.io> |
| KDE Frameworks (Kirigami, KI18n, KCoreAddons, KDNSSD) | KDE | LGPL-2.0-or-later | <https://develop.kde.org> |
| QtKeychain | Frank Osterfeld and contributors | BSD-3-Clause | <https://github.com/frankosterfeld/qtkeychain> |
| littlefs (vendored in `app/3rdparty/littlefs`) | Arm Limited, the littlefs authors | BSD-3-Clause | <https://github.com/littlefs-project/littlefs> |
| esptool (bundled in the Flatpak) | Espressif | GPL-2.0-or-later | <https://github.com/espressif/esptool> |

### Clock face sets by others

Not bundled. These are image sets you can download and import into the app:

- <https://github.com/upiir/ips_clock_100x_themes>
- <https://github.com/upiir/EleksTube_ips_custom_theme>
- <https://github.com/upiir/rgb_glow_tube_clock>
