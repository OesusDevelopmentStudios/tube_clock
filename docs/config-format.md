# Configuration file

The clock keeps its settings in `/config.json` on its LittleFS file system. The app writes the first version
into the file system image before flashing; later the clock's menu and the HTTP API update it. Writes are atomic:
the clock writes `config.tmp` and renames it over the old file.

If the file is missing or invalid, the clock starts with the defaults: WiFi off, remote access off, UTC.

The parser is in [lib/clockcore/src/clockcore/Config.cpp](../lib/clockcore/src/clockcore/Config.cpp) (firmware) and
[app/src/core/ClockConfig.cpp](../app/src/core/ClockConfig.cpp) (app). Both are tested against the same file,
[test/fixtures/config_full.json](../test/fixtures/config_full.json).

## Example

```json
{
  "version": 1,
  "name": "tubeclock-a1b2c3",
  "wifi": {
    "mode": "static",
    "ssid": "HomeNetwork",
    "password": "********",
    "ip": "192.168.1.50",
    "gateway": "192.168.1.1",
    "netmask": "255.255.255.0",
    "dns": "192.168.1.1"
  },
  "api": { "enabled": true, "token": "00112233445566778899aabbccddeeff" },
  "time": {
    "tz_name": "Europe/Prague",
    "tz_posix": "CET-1CEST,M3.5.0,M10.5.0/3",
    "dst": true,
    "ntp_server": "cz.pool.ntp.org",
    "ntp_enabled": true,
    "twelve_hour": false
  },
  "display": { "image_set": "nixie-tube", "lcd_brightness": 10 },
  "backlight": { "pattern": "rainbow", "color_phase": 0, "intensity": 7 }
}
```

## Fields

| Field | Type / range | Default | Notes |
| --- | --- | --- | --- |
| `version` | `1` | `1` | Format version |
| `name` | host name, ≤ 63 chars | `""` | Empty: `tubeclock-<last 3 MAC bytes>`. Used for DHCP and mDNS (`<name>.local`) |
| `wifi.mode` | `off`, `dhcp`, `static` | `off` | |
| `wifi.ssid` | 1-32 bytes | | Required unless `off` |
| `wifi.password` | empty or 8-63 chars | | Empty = open network. Never returned by the API |
| `wifi.ip`, `gateway`, `netmask` | IPv4 | `netmask` = `255.255.255.0` | Required for `static` |
| `wifi.dns` | IPv4 or empty | | Empty: the gateway |
| `api.enabled` | bool | `false` | HTTP API on port 80 |
| `api.token` | 16-64 hex chars | | Required when `api.enabled`. Never returned by the API |
| `time.tz_name` | IANA name | `Etc/UTC` | Display name, used by the menu's zone list |
| `time.tz_posix` | POSIX TZ rule | `UTC0` | Rule including summer time, from the TZif footer of the zone |
| `time.dst` | bool | `true` | `false`: the summer time part of the rule is ignored |
| `time.ntp_server` | host name / IPv4 | `pool.ntp.org` | |
| `time.ntp_enabled` | bool | `true` | Sync at most once a day |
| `time.twelve_hour` | bool | `false` | 12-hour display |
| `display.image_set` | set name | `""` | Empty or unknown: first complete set (alphabetical) |
| `display.lcd_brightness` | 1-10 | `10` | Out of range values are clamped |
| `backlight.pattern` | `off`, `constant`, `rainbow`, `pulse`, `breath` | `rainbow` | |
| `backlight.color_phase` | 0-767 | `0` | Colour wheel position for constant/pulse/breath |
| `backlight.intensity` | 0-7 | `7` | |

Unknown fields are ignored. A config that fails validation is rejected as a whole: by the API with an error
message, and at boot by falling back to the defaults.
