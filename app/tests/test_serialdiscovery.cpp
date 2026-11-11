// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "SerialDiscovery.hpp"

// Builds a fake /dev and /sys like the ones inside the Flatpak sandbox (no udev).
class TestSerialDiscovery : public QObject
{
    Q_OBJECT
private:
    static void write(const QString& path, const QByteArray& data)
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

    // A USB tty: /sys/devices/usb1/<usbDev>/<usbDev>:1.0/<tty>/tty/<tty>, linked from class/tty.
    static void addUsbTty(const QString& sys,
        const QString& tty,
        const QString& usbDev,
        const QByteArray& vid,
        const QByteArray& pid,
        const QByteArray& product)
    {
        const QString dev = sys + QStringLiteral("/devices/usb1/") + usbDev;
        write(dev + QStringLiteral("/idVendor"), vid + "\n");
        write(dev + QStringLiteral("/idProduct"), pid + "\n");
        write(dev + QStringLiteral("/product"), product + "\n");
        const QString node = dev + QStringLiteral("/%1:1.0/%2/tty/%2").arg(usbDev, tty);
        QVERIFY(QDir().mkpath(node));
        QVERIFY(QDir().mkpath(sys + QStringLiteral("/class/tty")));
        QVERIFY(QFile::link(node, sys + QStringLiteral("/class/tty/") + tty));
    }

private Q_SLOTS:
    void findsPortsWithoutUdev()
    {
        QTemporaryDir root;
        const QString dev = root.filePath(QStringLiteral("dev"));
        const QString sys = root.filePath(QStringLiteral("sys"));
        write(dev + QStringLiteral("/ttyACM0"), "");
        write(dev + QStringLiteral("/ttyUSB0"), "");
        write(dev + QStringLiteral("/ttyS0"), ""); // built-in UART: ignored
        addUsbTty(sys, QStringLiteral("ttyUSB0"), QStringLiteral("1-7"), "1a86", "7523", "USB Serial");
        addUsbTty(sys, QStringLiteral("ttyACM0"), QStringLiteral("1-3"), "28de", "1205", "Steam Controller");

        const QList<SerialDiscovery::Port> ports = SerialDiscovery::scan(dev, sys, false);
        QCOMPARE(ports.size(), 2);
        // The CH340 clock bridge comes first although ttyACM0 sorts before ttyUSB0.
        QCOMPARE(ports.at(0).path, dev + QStringLiteral("/ttyUSB0"));
        QCOMPARE(ports.at(0).vendorId, quint16(0x1a86));
        QCOMPARE(ports.at(0).productId, quint16(0x7523));
        QCOMPARE(ports.at(0).description, QStringLiteral("USB Serial"));
        QVERIFY(ports.at(0).likelyClock);
        QCOMPARE(ports.at(1).description, QStringLiteral("Steam Controller"));
        QVERIFY(!ports.at(1).likelyClock);
    }

    void portWithoutSysfsInfoIsStillListed()
    {
        QTemporaryDir root;
        const QString dev = root.filePath(QStringLiteral("dev"));
        write(dev + QStringLiteral("/ttyUSB3"), "");
        const QList<SerialDiscovery::Port> ports
            = SerialDiscovery::scan(dev, root.filePath(QStringLiteral("sys")), false);
        QCOMPARE(ports.size(), 1);
        QCOMPARE(ports.at(0).vendorId, quint16(0));
        QVERIFY(!ports.at(0).likelyClock);
    }

    void bridges()
    {
        QVERIFY(SerialDiscovery::isClockBridge(0x1a86));
        QVERIFY(SerialDiscovery::isClockBridge(0x10c4));
        QVERIFY(SerialDiscovery::isClockBridge(0x0403));
        QVERIFY(!SerialDiscovery::isClockBridge(0x28de));
    }
};

QTEST_GUILESS_MAIN(TestSerialDiscovery)
#include "test_serialdiscovery.moc"
