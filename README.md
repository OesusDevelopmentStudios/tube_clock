# Tube Clock

Replacement firmware and a Linux desktop app for EleksTube style IPS "nixie" tube clocks.

The desktop app (Qt 6 / KDE Kirigami, distributed as a Flatpak) does the following:

- **backs up** the clock's original firmware before anything is changed
- **configures** the firmware before it is written: WiFi (or no WiFi), DHCP or a static IP, time zone with
  automatic summer time, a daily internet time sync, and remote access
- **converts your own pictures** into image sets, with the same picture on every tube or a different one per tube
- **writes** firmware and settings to the clock over USB
- **finds the clock on your network** later on, syncs it with the computer's time, changes settings and uploads
  new image sets

> [!WARNING]
> **Using this software can damage your clock.** Flashing firmware is never risk free. An interrupted
> transfer, the wrong clock model, a bad USB cable or a bug can leave the clock unusable, and it may void your
> warranty. The app always offers to back up the original firmware first; keep that backup. This software comes
> with **absolutely no warranty** (GPL-3.0). Use it at your own risk.

## Supported clocks

All supported clocks have an ESP32, six 135×240 IPS displays and four buttons. Clocks with 4, 8 or 16 MB of flash
are supported; the app detects the size and uses the extra space for image sets.

| Model | Notes |
| --- | --- |
| EleksTube IPS (original) | DS3231 real-time clock |
| EleksTube IPS Gen2 / Classic / Pro / PR1 / PR2 | DS3231 real-time clock |
| Si Hai IPS clock | DS1302 real-time clock |
| PunkCyber / RGB Glow Tube / PCBway | tubes wired in reversed order (handled by the firmware) |

Pictures of the boards are in [docs/hardware](docs/hardware).

## Using the clock

| Button | On the clock face | In the menu |
| --- | --- | --- |
| ◀ / ▶ (left, right) | switch between time (HH MM SS) and date (DD MM YY) | previous / next item, or change the value |
| ● (middle) | open the settings menu | open the item / accept the value |
| ⏻ (power) | tubes and LEDs off (the clock keeps running); any button turns them on again | back / leave the menu |

The menu covers time, date, time zone, image set, backlight pattern, pattern brightness, LCD brightness and remote
access. While it is open, the tubes keep showing the clock. Small bars show the menu item and value on the
first two tubes, and the button legend on the other four, so you can watch changes as you make them.
Details: [docs/clock-controls.md](docs/clock-controls.md).

## Install

Install the Flatpak from the [releases page](https://github.com/OesusDevelopmentStudios/tube_clock/releases):

```bash
flatpak install --user TubeClock.flatpak
flatpak run io.github.OesusDevelopmentStudios.TubeClock
```

Your user needs access to USB serial ports. On most distributions this means membership in the `dialout` group
(`sudo usermod -aG dialout $USER`, then log out and back in).

Then connect the clock with a USB cable and choose **Set Up a Clock**. The step-by-step guide is
[docs/user-guide.md](docs/user-guide.md).

## Documentation

| Topic | Document |
| --- | --- |
| Step-by-step guide for the app | [docs/user-guide.md](docs/user-guide.md) |
| Buttons and on-clock menu | [docs/clock-controls.md](docs/clock-controls.md) |
| Backups, flashing and restoring | [docs/flashing-and-backup.md](docs/flashing-and-backup.md) |
| Image sets: naming, formats, sizes | [docs/image-sets.md](docs/image-sets.md) |
| Configuration file (`config.json`) | [docs/config-format.md](docs/config-format.md) |
| HTTP API of the clock | [docs/http-api.md](docs/http-api.md) |
| Building firmware, app and Flatpak | [docs/building.md](docs/building.md) |
| How it fits together | [docs/architecture.md](docs/architecture.md) |

## Repository layout

```
src/                    firmware (Arduino on ESP32, PlatformIO)
lib/clockcore/          hardware independent firmware logic, unit tested on the PC
lib/modified_TFT_eSPI/  display driver (patched copy of TFT_eSPI)
test/                   firmware unit tests (pio test -e native) and shared fixtures
app/                    desktop app (C++ / Qt 6 / Kirigami) with its tests
packaging/              desktop file, AppStream metadata, icon, Flatpak manifest
data/                   image sets that ship with the app
assets/                 extra clock face artwork from the original project
scripts/                build helpers
docs/                   documentation
```

## Credits

Tube Clock is a fork of [EleksTubeHAX](https://github.com/aly-fly/EleksTubeHAX), started by
Mark Smith ([@SmittyHalibut](https://github.com/SmittyHalibut)) and developed by Aljaz Ogrin
([@aly-fly](https://github.com/aly-fly)), Clemens Sutor ([@Martinius79](https://github.com/Martinius79)) and many
contributors. The hardware research, display and LED drivers, clock faces and much of the firmware's foundation come
from their work. See [CREDITS.md](CREDITS.md) for the full list, including third-party libraries.

## License

[GNU General Public License v3.0](LICENSE), like the original project. New files carry
`SPDX-License-Identifier: GPL-3.0-or-later`. Bundled third-party components keep their own licenses (see
[CREDITS.md](CREDITS.md)).
