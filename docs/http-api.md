# HTTP API

When WiFi is on and remote access (`api.enabled`) is on, the clock runs an HTTP server on port 80. The desktop app
uses it; any HTTP client can. Implementation: [src/ApiServer.cpp](../src/ApiServer.cpp).

## Discovery

The clock announces itself with mDNS as `<name>.local` and the service `_tubeclock._tcp` with these TXT records:
`model` (e.g. `EleksTube`), `fw` (firmware version), `id` (the clock name).

```bash
avahi-browse -rt _tubeclock._tcp
```

## Authentication

Every endpoint except `GET /api/info` requires the pairing token:

```
Authorization: Bearer <token>
```

The token is the `api.token` value of [config.json](config-format.md); the app generates 32 random hex characters
when it sets up a clock. Requests without a valid token get `401`. The connection is plain HTTP, so use it on a
trusted network only.

## Responses

All responses are JSON. Errors have the form `{"error": "<message>"}` with status 400, 401 or 404.

## Endpoints

### `GET /api/info` (no token)

```json
{
  "api_version": 1, "name": "tubeclock-a1b2c3", "model": "EleksTube",
  "model_name": "EleksTube IPS Tube Clock", "firmware": "2.0.0", "uptime_s": 5021,
  "time_valid": true, "utc": 1794268800, "last_ntp_utc": 1794265200,
  "tz_name": "Europe/Prague", "ip": "192.168.1.50", "rssi": -61,
  "fs_total": 2818048, "fs_used": 712704, "image_set": "nixie-tube", "display_on": true
}
```

### `GET /api/config`

The configuration without secrets: `wifi.password` is replaced by `wifi.has_password`, and `api.token` is
left out.

### `PATCH /api/config` (also `POST`)

A partial config document, e.g. `{"display": {"lcd_brightness": 4}}`. The result is validated as a whole and saved.
Answer: `{"ok": true, "reboot_required": false}`. Changes to `wifi.*` or `name` take effect after a restart
(`reboot_required: true`).

### `POST /api/time`

```json
{"utc": 1794268800}
```

Sets the clock (system time and RTC chip) to the given Unix time. Accepted range: 2024-2100.

### `GET /api/sets`

```json
{"current": "nixie-tube", "sets": [{"name": "nixie-tube", "complete": true}]}
```

### `GET /api/files`

Image files only: `{"files": [{"name": "nixie-tube_0_0.png", "size": 9817}], "fs_total": …, "fs_used": …}`.

### `PUT /api/files/<name>`

Uploads one image file; the body is the PNG data. `<name>` must be a valid image name
(`<set>_<screen>_<digit>.png`, see [image-sets.md](image-sets.md)). The clock refuses uploads that would leave less
than 16 KiB free. An existing file is replaced.

```bash
curl -X PUT -H "Authorization: Bearer $TOKEN" --data-binary @retro_0_7.png http://tubeclock-a1b2c3.local/api/files/retro_0_7.png
```

### `DELETE /api/files/<name>`, `DELETE /api/sets/<set>`

Delete one image file, or all files of a set.

### `POST /api/display`

`{"on": false}` turns the tubes and LEDs off (like the power button), `{"on": true}` turns them on.

### `POST /api/reboot`

Restarts the clock about half a second after answering.
