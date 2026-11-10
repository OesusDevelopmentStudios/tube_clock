# User guide

This guide walks through setting up a clock with the Tube Clock app and managing it afterwards.

> [!WARNING]
> Writing firmware can damage the clock. Do not unplug the clock or close the app while it is being written.

## Before you start

- A supported clock: EleksTube IPS, EleksTube Gen2, Si Hai or PunkCyber (see the [README](../README.md)).
- A USB **data** cable. Many cables only carry power and will not work.
- Permission to use serial ports: your user should be in the `dialout` group
  (`sudo usermod -aG dialout $USER`, then log out and in again).
- For WiFi: the name and password of a **2.4 GHz** network. The clock cannot use 5 GHz.

The first time the app starts, it explains the risks. You have to confirm that you understand them before
you can continue.

## Setting up a clock

Choose **Set Up a Clock…** in the side bar. The setup has five steps.

### 1. Connect

1. Pick the **clock model**. Firmware for the wrong model can leave the tubes dark. You can fix that by flashing
   again with the right model, but choose carefully.
2. Pick the **USB port**. Ports with the USB chips used on these clocks (CH340, CP210x, FTDI) are listed
   first; on Linux the clock is usually `/dev/ttyUSB0`.
3. Click **Connect to Clock**. The app reads the chip type and the MAC address of the clock.
   If this fails, try the **115200 baud** speed, another cable or another USB port.
4. Click **Back Up Now**. The complete flash memory (4, 8 or 16 MB) is saved to `~/Documents/TubeClock/backups/`
   as `<mac>_<date>.bin`, together with a `.sha256` checksum. This takes about two minutes per 4 MB.
   **Keep this file**: it is the only way back to the original firmware, and it only works on this clock.

You can skip the backup only after confirming a warning, for example when you already have one.

### 2. Network

- **Clock name**: used as the network name (`<name>.local`). The default is `tubeclock-` followed by the end of the
  MAC address.
- **WiFi**: *Off*, *Automatic address (DHCP)* or *Static IP address* (IP, subnet mask, gateway, DNS).
- **Remote access**: lets this app control the clock over WiFi. The app generates a random pairing key, writes it to
  the clock and stores it in your system wallet (KWallet or the Secret Service). Without the key nobody else on the
  network can change the clock.

### 3. Time

- **Time zone**: search for your city. The app stores the zone's rules on the clock, so summer time switches happen
  automatically.
- **Summer time**: turn this off to keep standard (winter) time all year.
- **Network time**: the clock asks a time server at most once a day. The default is the free
  [NTP Pool](https://www.ntppool.org) server group for your country, for example `cz.pool.ntp.org`.
- Tube brightness, backlight pattern, colour and brightness.

### 4. Images

The image sets shipped with the app are pre-selected. Uncheck the sets you do not want, and use the radio button to
pick the set the clock shows first. The bar shows how much of the clock's storage is used (about 2.7 MB on a 4 MB clock, more on 8 and 16 MB clocks).

**Import Image Set…** turns your own pictures into a set. See [image-sets.md](image-sets.md) for how to name the
files.

### 5. Write

Check the summary and click **Write to Clock**. The app does the following:

1. checks the firmware files of the app (checksums)
2. builds the clock's file system with your settings and image sets
3. writes the firmware, partition table and file system over USB

The clock restarts when it is done. With WiFi it joins your network and appears under **Clocks** after a few
seconds. Without WiFi, set the time on the clock itself (middle button, then **Time**).

## Managing a clock over WiFi

**Clocks** lists clocks found on the network, plus clocks you set up before. Clocks with a static IP that do not
show up can be added with **Add by Address…**. Select a clock to:

- see its firmware, address, time and free space
- **Sync with Computer Time**: sends the computer's time
- turn the tubes on or off
- change time zone, summer time, time server, brightness and backlight
- switch, delete or upload image sets
- change the WiFi settings (the clock restarts; wrong settings make it unreachable until it is set up again over USB)
- **Turn Off Remote Access**: the app can no longer reach the clock. Turn it back on in the clock's menu
  (**Remote access**).

If the clock was set up on another computer, the app asks for the pairing key. It is stored in the other
computer's wallet, under "Tube Clock".

## Restoring a backup

Open **Backups**, select a backup, choose the USB port and type `RESTORE` to confirm. The whole flash memory of the
clock is overwritten with the backup, which brings back the firmware the clock had when the backup was made. Details:
[flashing-and-backup.md](flashing-and-backup.md).

## Troubleshooting

| Problem | What to try |
| --- | --- |
| The app cannot reach a clock that answers in the browser | Run `flatpak run io.github.OesusDevelopmentStudios.TubeClock --resolve <name>.local` to check that the name resolves; or add the clock by its IP address. |
| The clock's port is not in the list | Click the refresh button next to the list. Run `flatpak run io.github.OesusDevelopmentStudios.TubeClock --list-ports` (or `tubeclock --list-ports`) to see which ports the app can access. |
| "Permission denied" on the port | Add your user to the `dialout` group and log in again. |
| "Failed to connect" | Use a data cable, try another USB port, close other programs that use the port (serial monitors). |
| "Corrupt data" / timeouts | Choose 115200 baud. |
| The clock does not appear under Clocks | Check that WiFi is on, that the network is 2.4 GHz, and that mDNS is not blocked. Add the clock by its IP address. |
| "The clock rejected the pairing token" | Remote access was set up from another computer, or the clock was set up again. Enter the current key, or set the clock up again. |
| The tubes show "Set time" | The clock has no valid time (no WiFi and a flat RTC battery). Set the time in the menu or with **Sync with Computer Time**. |
| The tubes show "No WiFi" | The clock has been offline for over a minute. It keeps retrying; check the network. |
