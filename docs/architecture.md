# Architecture

```
 ┌──────────────── Desktop app (Flatpak) ─────────────────┐          ┌──────────── Clock (ESP32) ────────────┐
 │ QML pages (Kirigami)                                   │          │ main.cpp: loop                         │
 │   └ controllers: FlashController, DeviceController,    │   USB    │   ├ Buttons → clockcore::UiController  │
 │     ClockListModel, ImageSetModel, ConfigForm          │ ───────▶ │   ├ Display (PNG → RGB565, overlays)   │
 │ core library (tested, no UI)                           │ esptool  │   ├ Backlights (WS2812)                │
 │   ├ ClockConfig  ── same schema ──────────────────────────────────┼─▶ ├ Storage: /config.json, image sets  │
 │   ├ ImageSet     ── same naming rules (lib/clockcore) ────────────┼─▶ ├ TimeKeeper: RTC, TZ, NTP 1×/day   │
 │   ├ LittleFsImage (builds the file system offline)     │          │   ├ Network: WiFi, mDNS                │
 │   ├ Esptool (detect, backup, write, restore)           │   WiFi   │   └ ApiServer: HTTP + token            │
 │   └ ClockClient (HTTP) ────────────────────────────────┼────────▶ │                                        │
 └────────────────────────────────────────────────────────┘          └────────────────────────────────────────┘
```

## Firmware

- **`lib/clockcore`**: everything that does not need hardware. It is unit tested on the PC (`pio test -e native`),
  and the app compiles parts of it too.
  - `Config`: JSON schema, validation, defaults, POSIX TZ handling (`stripDst`)
  - `Ui`: the button state machine (clock/date, display off, menu, editing, timeout, legend texts). It returns
    *effects* (redraw, apply live values, commit time/date, save) instead of touching hardware.
  - `ImageSet`: file naming, the screen 0 fallback, complete set detection
  - `TimeUtil`: digit layout for time and date, the NTP packet format, `NtpSchedule` (once a day, retry after 1 h)
  - `Png565`: streaming PNG decoding (pngle) into the RGB565 frame buffer, with brightness scaling
  - `TzTable`: the time zones offered in the menu (generated)
- **`src/`**: the hardware glue.
  - `Display`: TFT_eSPI with chip select through the shift register. It draws digit images plus overlay bars.
    When only the digit changes, only the image rows between the bars are sent, so the bars do not flicker.
  - `TimeKeeper`: the ESP32 system clock in UTC, `TZ` from the config, the RTC chip, and NTP over UDP.
  - `Storage`: LittleFS, the config file, and the image library.
  - `Network`, `ApiServer`: WiFi (DHCP/static/off), mDNS, and the HTTP API.
  - `main.cpp`: wires it together and owns the state the API uses (`App.h`).
- LCD brightness is done in software: the colours are scaled while decoding, because these boards switch the
  displays' power but have no backlight PWM.

## App

- **Core** (`app/src/core`, unit tested): `ClockConfig` mirrors the firmware schema and reuses
  `clockcore/Validation`. `ImageSet` converts and encodes pictures. `LittleFsImage` (vendored littlefs, disk format
  2.0, the geometry of the ESP32 core) builds the partition image. `Esptool` builds command lines and parses
  esptool's output. `FirmwareBundle` reads the bundled firmware manifest. `ClockClient` talks to the HTTP API.
- **UI** (`app/src/ui`): QML pages and the QObject controllers they use. Pairing tokens live in the system keyring
  (QtKeychain); known clocks and settings are in `QSettings`.
- **Setup flow**: detect (MAC, flash size) → backup → configure → build the file system → `write-flash` of the boot
  loader, partition table, app and file system. Nothing is compiled on the user's machine: one prebuilt firmware per
  model serves every configuration.

## Design decisions

| Decision | Reason |
| --- | --- |
| Settings in `config.json` on LittleFS instead of compiled in | One firmware for everybody; the app writes the settings into the file system image |
| PNG images | Common format, lossless, about 3× smaller than the old BMPs; the app converts anything else |
| `<set>_<screen>_<digit>` with screen 0 fallback | Per-tube designs are possible without storing 60 files for simple sets |
| POSIX TZ rules from tzdata | Correct summer time worldwide without a time zone database on the clock |
| NTP at most once a day | Requirement; the RTC chip keeps time in between |
| Token in the keyring, plain HTTP | Simple and enough for a home network; the token stops others on the LAN from changing the clock |
| Menu as overlay bars | Settings changes are visible on the tubes while the menu is open |
