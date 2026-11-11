# AGENTS.md — Tube Clock

## Agent instructions

- Chat responses: use the language of the IDE/system locale (Windows: `Get-Culture` / `$PSUICulture`,
  Linux/macOS: `LANG`). Never hardcode a language; if no locale is available, mirror the user's message.
- Code comments, identifiers, `Serial.print` / debug output, commit messages, documentation: **English**
- **Never commit or push on your own!** Only after an explicit instruction from the user.
- **Never switch branches** unless explicitly asked to.
- **Never push to a `main` branch unless the user explicitly asks for it.** Confirm first and run
  `git branch --show-current`.
- **Never stash, pop, reset, or run any other git operation** (including `git mv` / `git rm`, which change the
  index) without explicit permission. Move or delete files with plain file operations.
- **Any action that changes the branch or working tree (edit, commit, push, stash, pop, reset, checkout) requires the user's approval first.**
- **No machine-local or private values in tracked files.** COM ports, credentials, host names, pairing tokens and
  machine-specific paths never go into `platformio.ini`, `AGENTS.md`, docs, code, test fixtures or commit messages.
  Never add `upload_port` / `monitor_port` to `platformio.ini`; pass the port on the command line
  (`pio run -e <env> -t upload --upload-port <PORT>`).
- **Building and testing are allowed, writing to a clock is not.** `pio run`, `pio test -e native`, `cmake`,
  `ctest` and `flatpak-builder` (without `--install`) may be run freely. `-t upload`, `-t uploadfs`,
  `pio device monitor`, every `esptool` call and running the app's flash/restore functions happen only after an
  explicit user request, for that one action, and only with the port the user names.
- **Never use `--erase-all` or `write-flash` from offset `0x0`** without explicit confirmation; it wipes the
  image sets and configuration stored on the clock (a restore of a backup is the only exception, on request).

## Guardrails

- Do not modify unless explicitly requested: `lib/modified_TFT_eSPI/**`, `.github/workflows/**`,
  `partition_*.csv`, `BUILDVER` in `platformio.ini`, `app/3rdparty/**`.
- `lib/modified_TFT_eSPI/library.properties` is required: it makes PlatformIO build only the library's root
  sources (`Extensions/*.cpp` are included from `TFT_eSPI.cpp` and fail to compile on their own).
- The flash layouts (`partition_4MB.csv`, `_8MB`, `_16MB`; the app area must be identical) are shared by the firmware,
  `scripts/collect_firmware.py` and the app
  (via `manifest.json`). Changing it breaks existing clocks' image sets and needs a migration plan.
- `lib/clockcore` must stay free of Arduino/ESP-IDF headers; it is compiled on the PC by the unit tests and partly by
  the app.
- The config schema lives in two places (`lib/clockcore/src/clockcore/Config.cpp`,
  `app/src/core/ClockConfig.cpp`). Change both, plus `docs/config-format.md` and `test/fixtures/config_full.json`.
- After changes, verify:
  - firmware: `pio test -e native` and `pio run -e <affected env>` (all four envs if shared code changed)
  - app: `cmake --build build/app && ctest --test-dir build/app`
- Code style: C/C++ follows `.clang-format` (Allman braces, 4-space indent, 120 columns; `clang-format -i`) plus
  two manual rules: a blank line after a closing `}` followed by more statements, and `break;` inside the braces
  of a braced case block. Headers are `.hpp`, next to their `.cpp` (see `docs/building.md`, "Code style").
  Python passes `flake8` with 120 columns (`.flake8`). Do not reformat third-party or generated files.
- Preserve line endings: LF for all text files, CRLF only for the `*.url` shortcuts in `docs/hardware` (see
  `.gitattributes`).
- No wholesale reformatting, no unrelated refactors, no renames inside a change request.
- Before any commit: run `git branch --show-current` and `git status`.

## Repository hygiene

- Firmware binaries are **not** committed. CI builds them (`dist/firmware`, ignored) and attaches them to releases.
- `data/*.png` (bundled image sets) and `assets/**` (original artwork) are intentionally committed. Do not delete
  them. Git LFS is not used.
- Binary files that are not meant to be read: `*.bin`, `*.png`, `*.bmp`, `*.jpg`, `*.pdf`, `*.xlsx`, `*.docx`.

## Project structure

| Path | Content |
| --- | --- |
| `src/` | Firmware (sources and headers): `main.cpp`, `Display`, `Backlights`, `Buttons`, `ChipSelect`, `TimeKeeper`, `Storage`, `Network`, `ApiServer` |
| `src/GLOBAL_DEFINES.hpp` | Pins of the 4 supported clocks; also copied into TFT_eSPI as `User_Setup.h` by `scripts/script_configure_tft_lib.py` |
| `lib/clockcore/` | Hardware independent logic (`src/clockcore/`, included as `"clockcore/X.hpp"`): config, UI state machine, image set naming, time/NTP, PNG decoding, TZ table |
| `test/` | Unity tests for `pio test -e native`; `test/fixtures` is shared with the app tests |
| `app/` | Desktop app: `src/core` (tested library), `src/ui` (QML + controllers), `tests`, `3rdparty/littlefs` |
| `packaging/` | `.desktop`, AppStream metainfo, icon, Flatpak manifest |
| `scripts/` | `collect_firmware.py`, generators (`gen_*`), `convert_legacy_faces.py`, PlatformIO pre-scripts |
| `data/` | Image sets shipped with the app (`<set>_<screen>_<digit>.png`) |
| `docs/` | User and developer documentation, hardware notes |

## Quick start

```bash
pio test -e native                                     # firmware unit tests
pio run -e EleksTube -e EleksTube_Gen2 -e SI_HAI -e PunkCyber
python3 scripts/collect_firmware.py                    # -> dist/firmware
cmake -S app -B build/app -G Ninja -DTUBECLOCK_FIRMWARE_DIR=dist/firmware
cmake --build build/app && ctest --test-dir build/app --output-on-failure
```

More: [docs/building.md](docs/building.md), [docs/architecture.md](docs/architecture.md).

## Hardware notes

- Supported: EleksTube (DS3231), EleksTube Gen2 (DS3231, CS shift register on GPIO 9/10), Si Hai (DS1302),
  PunkCyber (EleksTube board, reversed tube order: `SCREEN_TO_DIGIT`). All: ESP32, ST7789 135×240, 4/8/16 MB flash.
- The firmware is built for 4 MB only; the app patches the flash size into the boot loader and app headers
  (`app/src/core/EspImage.cpp`) and writes the partition table of the detected size.
- The CH340 USB bridge fails at 921600 baud; use 460800 (default) or 115200.
- Logical screen 0 is the leftmost tube everywhere in the code; only `SCREEN_TO_DIGIT()` knows the wiring.
