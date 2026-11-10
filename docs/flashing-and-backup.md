# Flashing and backups

> [!WARNING]
> Writing to the clock's flash memory can make it unusable. Keep the backup of the original firmware.

## Flash layouts

The app detects the flash size of the clock (4, 8 or 16 MB) and uses the matching layout. The firmware area is
the same in all of them; larger chips get a larger file system for image sets.

| Offset | Size | Content |
| --- | --- | --- |
| `0x1000` | | Second stage boot loader |
| `0x8000` | | Partition table |
| `0x9000` | 28 KiB | NVS (used by the WiFi driver) |
| `0x10000` | 1216 KiB | Firmware (factory app, no OTA) |
| `0x140000` | 2752 KiB / 6848 KiB / 15040 KiB | LittleFS: `config.json` and the image sets |
| end − 64 KiB | 64 KiB | Core dump |

Partition tables: [partition_4MB.csv](../partition_4MB.csv), [partition_8MB.csv](../partition_8MB.csv),
[partition_16MB.csv](../partition_16MB.csv).

The firmware is built once, for 4 MB. For larger chips the app writes the real flash size into the header of the
boot loader and the app image (byte 3) and recomputes the SHA-256 that ESP-IDF appends to both images, which is what
`esptool write-flash --flash-size` does for the boot loader. Without this the firmware would only use the first
4 MB.

## What the app writes

The app does **not** erase the whole chip. It writes these images with esptool (`write-flash`, flash mode, frequency
and size are kept as they are):

| Offset | File |
| --- | --- |
| `0x1000` | `bootloader.bin` |
| `0x8000` | `partitions.bin` of the detected flash size |
| `0x10000` | `app.bin` (firmware for the selected model, flash size set in the header) |
| `0x140000` | file system image, built by the app (config + image sets) |

The offsets come from the firmware bundle's `manifest.json` (written by `scripts/collect_firmware.py` from the
partition table). Before writing, the app checks the SHA-256 of every firmware file.

The equivalent command line, for reference:

```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 app.bin 0x140000 littlefs.bin
```

## Backups

A backup is the complete flash memory (4, 8 or 16 MB, as detected), read with:

```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 read-flash 0x0 0x800000 <mac>_<date>.bin   # 8 MB clock
```

The app stores backups in `~/Documents/TubeClock/backups/` together with a `<file>.sha256` checksum, and shows
"checksum OK" in the **Backups** page when the file is unchanged.

Things to know:

- A backup belongs to **one** clock. Parts of the flash are tied to the chip (e.g. encryption or calibration data on
  some models), so a backup from another clock may not work. The file name starts with the clock's MAC address.
- 921600 baud is unreliable with the CH340 USB chip on these clocks; the app uses 460800 (or 115200 if selected).

## Restoring

**Backups → Restore Selected Backup** writes the backup back to the whole chip:

```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash 0x0 <backup>.bin
```

The app first connects to the clock, only accepts a backup of exactly the clock's flash size and asks you to type
`RESTORE` to confirm.

## If the clock does not start any more

- Flash again with the app; the boot loader of the ESP32 is in ROM and cannot be damaged by writing the flash.
- If the clock does not answer at all, its USB chip may not be able to reset the ESP32 into download mode (some
  clones, e.g. PunkCyber boards without the CH340, need an external USB-serial adapter). GPIO0 must be low while the
  ESP32 starts; see the board notes in [hardware/](hardware).
- Restore the backup.
