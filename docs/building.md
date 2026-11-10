# Building

## Firmware

Requirements: [PlatformIO](https://platformio.org) (`pip install platformio`).

```bash
pio test -e native                      # unit tests of lib/clockcore on the PC
pio run -e EleksTube                    # one model
pio run -e EleksTube -e EleksTube_Gen2 -e SI_HAI -e PunkCyber
python3 scripts/collect_firmware.py     # bundle for the app -> dist/firmware/
```

| Environment | Clock |
| --- | --- |
| `EleksTube` | EleksTube IPS (original) |
| `EleksTube_Gen2` | EleksTube IPS Gen2 / Classic / Pro / PR1 / PR2 |
| `SI_HAI` | Si Hai |
| `PunkCyber` | PunkCyber / RGB Glow Tube / PCBway |
| `native` | Unit tests only |

The firmware version is `BUILDVER` in [platformio.ini](../platformio.ini).

For development you can flash with PlatformIO directly (this does what the app does, but `uploadfs` writes the
`data/` folder without a `config.json`, so the clock starts with defaults):

```bash
pio run -e EleksTube -t upload --upload-port /dev/ttyUSB0
pio run -e EleksTube -t uploadfs --upload-port /dev/ttyUSB0
pio device monitor -p /dev/ttyUSB0
```

## Desktop app

Requirements (Fedora package names): `cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel
qt6-qtserialport-devel kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel kf6-kdnssd-devel
qtkeychain-qt6-devel` and `esptool` at run time.

```bash
cmake -S app -B build/app -G Ninja -DTUBECLOCK_FIRMWARE_DIR=dist/firmware
cmake --build build/app
ctest --test-dir build/app --output-on-failure
cmake --install build/app --prefix ~/.local     # optional
```

Running from the build tree without installing:

```bash
TUBECLOCK_FIRMWARE_DIR=$PWD/dist/firmware TUBECLOCK_IMAGESETS_DIR=$PWD/data build/app/src/ui/tubeclock
```

The core library and its tests also build without the KDE frameworks: `-DTUBECLOCK_BUILD_GUI=OFF`.

| Environment variable | Purpose |
| --- | --- |
| `TUBECLOCK_FIRMWARE_DIR` | Firmware bundle (`manifest.json` + binaries) |
| `TUBECLOCK_IMAGESETS_DIR` | Bundled image sets |
| `TUBECLOCK_ESPTOOL` | esptool command, e.g. `python3 -m esptool` |

### Tests

| Test | Covers |
| --- | --- |
| `test_clockconfig` | config JSON, validation, the shared fixture |
| `test_timezones` | TZif footer parsing; every system zone gives a rule the firmware accepts |
| `test_imageset` | conversion, PNG encoding, file naming, imports, bundled sets |
| `test_littlefs` | file system image round trip, size checks, compatibility with PlatformIO's `mklittlefs` (if installed) |
| `test_esptool` | command lines, progress and error parsing, process handling |
| `test_firmwarebundle` | manifest loading and checksum verification |
| `test_clockclient` | HTTP client against a fake clock |
| `ui_smoke` | opens every page of the app and fails on QML errors |

## Flatpak

Requirements: `flatpak-builder` and the KDE runtime (`flatpak install flathub org.kde.Sdk//6.11 org.kde.Platform//6.11`).
Build the firmware bundle first (see above), then:

```bash
flatpak-builder --user --install --force-clean build-flatpak \
    packaging/flatpak/io.github.OesusDevelopmentStudios.TubeClock.yml
flatpak run io.github.OesusDevelopmentStudios.TubeClock
```

The app's tests run during the build. esptool and its Python dependencies are pinned in
`packaging/flatpak/python3-esptool.json`; regenerate it with `scripts/gen_flatpak_python_deps.py`.

## Code style

| Language | Rule | Config | Command |
| --- | --- | --- | --- |
| C / C++ | Mozilla style, 120 columns | [.clang-format](../.clang-format), [.clang-format-ignore](../.clang-format-ignore) | `clang-format -i <files>` |
| Python | flake8, 120 columns | [.flake8](../.flake8) | `flake8 scripts test/fixtures` |

Third-party code (`lib/modified_TFT_eSPI`, `app/3rdparty`) and generated files are excluded.
Both tools can be installed with `pip install clang-format flake8`.

## CI

[.github/workflows/build.yml](../.github/workflows/build.yml) runs the firmware unit tests, builds all models and the
firmware bundle, builds the Flatpak (including the app tests) and, for `v*` tags, creates a GitHub release with
the firmware bundle and the Flatpak bundle.

## Generated files

| File | Generator |
| --- | --- |
| `lib/clockcore/src/TzTable.cpp` | `scripts/gen_tz_table.py` (menu time zones from the system tz database) |
| `test/fixtures/png_fixtures.h` | `test/fixtures/gen_png_fixtures.py` |
| `packaging/flatpak/python3-esptool.json` | `scripts/gen_flatpak_python_deps.py` |
| `data/*.png` | `scripts/convert_legacy_faces.py` (one-off, from the EleksTubeHAX BMP faces) |
