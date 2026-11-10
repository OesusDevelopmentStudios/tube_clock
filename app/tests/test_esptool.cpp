// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "Esptool.h"

class TestEsptool : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void arguments()
  {
    QCOMPARE(Esptool::chipInfoArgs(QStringLiteral("/dev/ttyUSB0"), 460800),
             QStringList({ "--chip", "esp32", "--port", "/dev/ttyUSB0", "--baud", "460800", "flash-id" }));
    QCOMPARE(Esptool::backupArgs(QStringLiteral("/dev/ttyUSB0"), 115200, QStringLiteral("/tmp/b.bin"), 0x800000).mid(6),
             QStringList({ "read-flash", "0x0", "0x800000", "/tmp/b.bin" }));
    const QStringList flash =
      Esptool::flashArgs(QStringLiteral("/dev/ttyUSB0"),
                         460800,
                         { { 0x1000, QStringLiteral("boot.bin") }, { 0x140000, QStringLiteral("fs.bin") } });
    QCOMPARE(flash.mid(6),
             QStringList({ "write-flash",
                           "--flash-mode",
                           "keep",
                           "--flash-freq",
                           "keep",
                           "--flash-size",
                           "keep",
                           "0x1000",
                           "boot.bin",
                           "0x140000",
                           "fs.bin" }));
    QVERIFY(!flash.contains(QStringLiteral("--erase-all")));
    QCOMPARE(Esptool::restoreArgs(QStringLiteral("p"), 1, QStringLiteral("b.bin")).mid(6),
             QStringList({ "write-flash", "0x0", "b.bin" }));
  }

  void progress_data()
  {
    QTest::addColumn<QString>("line");
    QTest::addColumn<double>("expected");
    QTest::newRow("v5 write") << "Writing at 0x00010000 [=====>      ]  10.4% 65536/998960 bytes..." << 10.4;
    QTest::newRow("v5 read") << "\x1b[KReading from 0x00001000 [   ]   0.1% 4096/4194304 bytes..." << 0.1;
    QTest::newRow("v4 write") << "Writing at 0x00010000... (12 %)" << 12.0;
    QTest::newRow("v4 read") << "1048576 (25 %)" << -1.0;
    QTest::newRow("done") << "Wrote 998960 bytes (642621 compressed) at 0x00010000 in 14.8 seconds" << -1.0;
    QTest::newRow("other") << "Hash of data verified." << -1.0;
  }

  void progress()
  {
    QFETCH(QString, line);
    QFETCH(double, expected);
    const auto p = Esptool::parseProgress(line);
    if (expected < 0) {
      QVERIFY(!p.has_value());
    } else {
      QVERIFY(p.has_value());
      QCOMPARE(*p, expected);
    }
  }

  void chipInfoV5()
  {
    // Captured from esptool 5.2 talking to an EleksTube clock.
    const QString out = QStringLiteral("Connected to ESP32 on /dev/ttyUSB0:\n"
                                       "Chip type:          ESP32-D0WD (revision v1.0)\n"
                                       "Features:           Wi-Fi, BT, Dual Core + LP Core, 240MHz\n"
                                       "Crystal frequency:  40MHz\n"
                                       "MAC:                98:CD:AC:57:4B:B8\n"
                                       "Flash Manufacturer: 5e\nDetected flash size: 4MB\n");
    const auto info = Esptool::parseChipInfo(out);
    QVERIFY(info.valid());
    QCOMPARE(info.chip, QStringLiteral("ESP32-D0WD (revision v1.0)"));
    QCOMPARE(info.mac, QStringLiteral("98:cd:ac:57:4b:b8"));
    QCOMPARE(info.flashSize, QStringLiteral("4MB"));
  }

  void chipInfoV4()
  {
    const auto info = Esptool::parseChipInfo(
      QStringLiteral("Chip is ESP32-D0WDQ6 (revision v1.0)\nMAC: 24:0a:c4:00:00:01\nDetected flash size: 4MB\n"));
    QCOMPARE(info.chip, QStringLiteral("ESP32-D0WDQ6 (revision v1.0)"));
    QVERIFY(info.valid());
  }

  void fatalError()
  {
    QCOMPARE(Esptool::parseFatalError(QStringLiteral(
               "Connecting....\n\x1b[1;31mA fatal error occurred: Corrupt data, expected 0x1000 bytes\x1b[0m\n")),
             QStringLiteral("Corrupt data, expected 0x1000 bytes"));
    QVERIFY(Esptool::parseFatalError(QStringLiteral("all good")).isEmpty());
  }

  void runnerReportsMissingProgram()
  {
    qputenv("TUBECLOCK_ESPTOOL", "/nonexistent/esptool");
    EsptoolRunner r;
    QSignalSpy done(&r, &EsptoolRunner::finished);
    r.start({ QStringLiteral("version") });
    QVERIFY(done.wait(5000));
    QCOMPARE(done.first().at(0).toBool(), false);
    qunsetenv("TUBECLOCK_ESPTOOL");
  }

  void runnerParsesProcessOutput()
  {
    // A fake esptool: prints progress with \r and fails with esptool's error format.
    qputenv("TUBECLOCK_ESPTOOL", "sh -c");
    EsptoolRunner r;
    QSignalSpy prog(&r, &EsptoolRunner::progress);
    QSignalSpy done(&r, &EsptoolRunner::finished);
    r.start({ QStringLiteral("printf 'Writing at 0x0 [=] 50.0%% 1/2 bytes\\rWriting at 0x0 [==] 100.0%% 2/2 bytes\\n"
                             "A fatal error occurred: Port busy\\n'; exit 2") });
    QVERIFY(done.wait(5000));
    QCOMPARE(prog.size(), 2);
    QCOMPARE(prog.at(1).at(0).toDouble(), 100.0);
    QCOMPARE(done.first().at(0).toBool(), false);
    QCOMPARE(done.first().at(1).toString(), QStringLiteral("Port busy"));
    qunsetenv("TUBECLOCK_ESPTOOL");
  }
};

QTEST_GUILESS_MAIN(TestEsptool)
#include "test_esptool.moc"
