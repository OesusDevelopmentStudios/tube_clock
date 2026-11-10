// SPDX-License-Identifier: GPL-3.0-or-later
#include "SerialDiscovery.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSerialPortInfo>

#include <algorithm>

namespace SerialDiscovery {

namespace {

QString
readLine(const QString& path)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return QString();
  return QString::fromUtf8(f.readAll()).trimmed();
}

} // namespace

bool
isClockBridge(quint16 vendorId)
{
  return vendorId == 0x1a86 || vendorId == 0x10c4 || vendorId == 0x0403;
}

bool
usbInfoFromSysfs(const QString& ttyName, Port& port, const QString& sysRoot)
{
  // /sys/class/tty/ttyUSB0 -> .../usb1/1-7/1-7:1.0/ttyUSB0/tty/ttyUSB0. The USB device
  // directory (with idVendor) is a few levels up from the tty node.
  QString dir = QFileInfo(QDir(sysRoot).filePath(QStringLiteral("class/tty/") + ttyName)).canonicalFilePath();
  const QString root = QFileInfo(sysRoot).canonicalFilePath();
  for (int depth = 0; depth < 8 && !dir.isEmpty() && dir != root && dir.startsWith(root); ++depth) {
    if (QFile::exists(dir + QStringLiteral("/idVendor"))) {
      bool ok = false;
      port.vendorId = quint16(readLine(dir + QStringLiteral("/idVendor")).toUInt(&ok, 16));
      port.productId = quint16(readLine(dir + QStringLiteral("/idProduct")).toUInt(nullptr, 16));
      port.description = readLine(dir + QStringLiteral("/product"));
      return ok;
    }
    dir = QFileInfo(dir).path();
  }
  return false;
}

QList<Port>
scan(const QString& devDir, const QString& sysRoot, bool useQtEnumeration)
{
  QList<Port> ports;
  auto add = [&ports](const Port& p) {
    for (const Port& e : ports)
      if (e.path == p.path)
        return;
    ports.append(p);
  };

  if (useQtEnumeration) {
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
      Port p;
      p.path = info.systemLocation();
      if (info.hasVendorIdentifier()) {
        p.vendorId = info.vendorIdentifier();
        p.productId = info.productIdentifier();
        p.description = info.description();
      } else if (!usbInfoFromSysfs(info.portName(), p, sysRoot)) {
        continue; // built-in serial ports (ttyS*): the clocks are always USB
      }
      add(p);
    }
  }

  // Fallback without udev: USB serial devices always have these names on Linux.
  const QDir dev(devDir);
  for (const QString& name : dev.entryList(
         { QStringLiteral("ttyUSB*"), QStringLiteral("ttyACM*") }, QDir::System | QDir::Files, QDir::Name)) {
    Port p;
    p.path = dev.filePath(name);
    usbInfoFromSysfs(name, p, sysRoot); // optional details
    add(p);
  }

  for (Port& p : ports)
    p.likelyClock = isClockBridge(p.vendorId);
  std::stable_sort(
    ports.begin(), ports.end(), [](const Port& a, const Port& b) { return a.likelyClock && !b.likelyClock; });
  return ports;
}

} // namespace SerialDiscovery
