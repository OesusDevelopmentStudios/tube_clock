// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finds USB serial ports without relying on udev.
//
// QSerialPortInfo gets USB vendor/product data from the udev database, which is not
// available inside the Flatpak sandbox (/run/udev is not shared). Sysfs is, so the
// ports are also listed from /dev/ttyUSB* and /dev/ttyACM* and their USB details are
// read from /sys/class/tty/<name>/device/...
#pragma once

#include <QList>
#include <QString>

namespace SerialDiscovery {

struct Port
{
  QString path;         // "/dev/ttyUSB0"
  quint16 vendorId = 0; // 0 = unknown
  quint16 productId = 0;
  QString description; // USB product string, e.g. "USB Serial"
  bool likelyClock = false;
};

// USB bridges found on EleksTube style clocks (WCH CH340/CH9102, Silicon Labs CP210x, FTDI).
bool
isClockBridge(quint16 vendorId);

// Reads the USB ids and product name of a tty device from sysfs. Returns false if the
// device is not below a USB device.
bool
usbInfoFromSysfs(const QString& ttyName, Port& port, const QString& sysRoot = QStringLiteral("/sys"));

// All USB serial ports: the ones QSerialPortInfo reports plus /dev/ttyUSB* and /dev/ttyACM*,
// with likely clock bridges first.
QList<Port>
scan(const QString& devDir = QStringLiteral("/dev"),
     const QString& sysRoot = QStringLiteral("/sys"),
     bool useQtEnumeration = true);

} // namespace SerialDiscovery
