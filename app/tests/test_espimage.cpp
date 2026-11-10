// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "EspImage.h"

class TestEspImage : public QObject
{
  Q_OBJECT
private:
  // Minimal image: 24 byte header, some payload, appended SHA-256.
  static QByteArray fakeImage(quint8 sizeByte, bool withHash)
  {
    QByteArray d(24, '\0');
    d[0] = char(0xE9);
    d[3] = char(sizeByte);
    d[23] = withHash ? 1 : 0;
    d.append(QByteArray(100, 'x'));
    if (withHash)
      d.append(QCryptographicHash::hash(d, QCryptographicHash::Sha256));
    return d;
  }

  static QByteArray readFile(const QString& path)
  {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  }

private Q_SLOTS:
  void sizeCodes()
  {
    QCOMPARE(EspImage::sizeCodeForBytes(0x400000), 2);
    QCOMPARE(EspImage::sizeCodeForBytes(0x800000), 3);
    QCOMPARE(EspImage::sizeCodeForBytes(0x1000000), 4);
    QCOMPARE(EspImage::sizeCodeForBytes(3 * 1024 * 1024), -1);
  }

  void patchKeepsFrequencyAndFixesHash()
  {
    QByteArray img = fakeImage(0x2F, true); // 4 MB, frequency nibble F
    QVERIFY(EspImage::hashValid(img));
    QCOMPARE(EspImage::flashSizeCode(img), 2);
    QVERIFY(EspImage::setFlashSize(img, 3));
    QCOMPARE(uchar(img.at(3)), uchar(0x3F));
    QVERIFY(EspImage::hashValid(img));
    QCOMPARE(img.size(), 24 + 100 + 32);
  }

  void imageWithoutHash()
  {
    QByteArray img = fakeImage(0x20, false);
    QVERIFY(EspImage::setFlashSize(img, 4));
    QCOMPARE(EspImage::flashSizeCode(img), 4);
    QCOMPARE(img.size(), 124);
  }

  void rejectsBadInput()
  {
    QByteArray junk("not an image at all, definitely not");
    QString err;
    QVERIFY(!EspImage::setFlashSize(junk, 3, &err));
    QByteArray damaged = fakeImage(0x20, true);
    damaged[50] = 'y';
    QVERIFY(!EspImage::setFlashSize(damaged, 3, &err));
    QVERIFY(err.contains(QStringLiteral("damaged")));
    QByteArray ok = fakeImage(0x20, true);
    QVERIFY(!EspImage::setFlashSize(ok, 7, &err));
  }

  // The real boot loader and app from the firmware bundle, checked with esptool if available.
  void realFirmware()
  {
    const QString dir = QStringLiteral(REPO_ROOT "/dist/firmware/EleksTube");
    const QByteArray boot = readFile(dir + QStringLiteral("/bootloader.bin"));
    const QByteArray app = readFile(dir + QStringLiteral("/app.bin"));
    if (boot.isEmpty() || app.isEmpty())
      QSKIP("run scripts/collect_firmware.py first");
    QVERIFY(EspImage::hasAppendedHash(boot));
    QVERIFY(EspImage::hashValid(boot));
    QVERIFY(EspImage::hashValid(app));
    QCOMPARE(EspImage::flashSizeCode(app), 2);

    QByteArray patched = app;
    QVERIFY(EspImage::setFlashSize(patched, 3));
    QVERIFY(EspImage::hashValid(patched));

    const QString esptool = QStandardPaths::findExecutable(QStringLiteral("esptool"));
    if (esptool.isEmpty())
      QSKIP("esptool not installed");
    QTemporaryDir tmp;
    QFile out(tmp.filePath(QStringLiteral("app8.bin")));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(patched);
    out.close();
    QProcess p;
    p.start(esptool,
            { QStringLiteral("--chip"), QStringLiteral("esp32"), QStringLiteral("image-info"), out.fileName() });
    QVERIFY(p.waitForFinished(20000));
    const QString text = QString::fromLocal8Bit(p.readAllStandardOutput() + p.readAllStandardError());
    QVERIFY2(p.exitCode() == 0, qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("8MB")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("(valid)")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("invalid")), qPrintable(text));
  }
};

QTEST_GUILESS_MAIN(TestEspImage)
#include "test_espimage.moc"
