// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "ClockFilesystem.hpp"
#include "LittleFsImage.hpp"

class TestLittleFs : public QObject
{
    Q_OBJECT
    static constexpr quint32 kPartition = 0x2b0000; // partition_4MB.csv

private Q_SLOTS:
    void roundTrip()
    {
        LittleFsImage fs(kPartition);
        QVERIFY(fs.format());
        QByteArray big(70000, '\0');
        for (int i = 0; i < big.size(); ++i)
        {
            big[i] = char(i * 7);
        }

        QVERIFY(fs.addFile(QStringLiteral("config.json"), "{\"version\":1}"));
        QVERIFY(fs.addFile(QStringLiteral("nixie_0_0.png"), big));
        QVERIFY(fs.addFile(QStringLiteral("empty.txt"), QByteArray()));
        QVERIFY(fs.usedBytes() >= quint64(big.size()));
        const QByteArray image = fs.finish();
        QCOMPARE(quint32(image.size()), kPartition);
        QCOMPARE(image.right(4096), QByteArray(4096, char(0xFF))); // unused flash stays erased

        QString err;
        const auto files = LittleFsImage::readAll(image, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(files.size(), 3);
        QCOMPARE(files.value(QStringLiteral("nixie_0_0.png")), big);
        QCOMPARE(files.value(QStringLiteral("config.json")), QByteArray("{\"version\":1}"));
    }

    void rejectsBadNames()
    {
        LittleFsImage fs(kPartition);
        QVERIFY(fs.format());
        QString err;
        QVERIFY(!fs.addFile(QStringLiteral("dir/file"), "x", &err));
        QVERIFY(!fs.addFile(QString(33, QLatin1Char('a')), "x", &err));
    }

    void reportsFullFilesystem()
    {
        LittleFsImage fs(16 * 4096);
        QVERIFY(fs.format());
        QString err;
        QVERIFY(!fs.addFile(QStringLiteral("big.bin"), QByteArray(20 * 4096, 'x'), &err));
        QCOMPARE(err, QStringLiteral("not enough space on the clock"));
    }

    void buildsClockFilesystemWithDefaultSets()
    {
        ClockConfig cfg;
        cfg.wifiMode = ClockConfig::WifiMode::Dhcp;
        cfg.ssid = QStringLiteral("home");
        cfg.password = QStringLiteral("password123");
        cfg.apiEnabled = true;
        cfg.token = ClockConfig::generateToken();
        cfg.imageSet = QStringLiteral("nixie-tube");
        const QList<ImageSet> sets = ImageSet::fromClockFolder(QStringLiteral(REPO_ROOT "/data"));
        QString err;
        quint64 used = 0;
        const QByteArray image = ClockFilesystem::build(cfg, sets, kPartition, &err, &used);
        QVERIFY2(!image.isEmpty(), qPrintable(err));
        QVERIFY(used > 0 && used < kPartition);

        const auto files = LittleFsImage::readAll(image, &err);
        QCOMPARE(files.size(), 1 + 70);
        const QJsonObject json = QJsonDocument::fromJson(files.value(QStringLiteral("config.json"))).object();
        QCOMPARE(ClockConfig::fromJson(json).token, cfg.token);
        QCOMPARE(files.value(QStringLiteral("nixie-tube_0_5.png")), sets.at(4).png(0, 5));
    }

    void refusesInvalidConfigOrTooManySets()
    {
        ClockConfig bad;
        bad.apiEnabled = true; // no token
        QString err;
        QVERIFY(ClockFilesystem::build(bad, {}, kPartition, &err).isEmpty());
        QVERIFY(!err.isEmpty());

        ImageSet huge(QStringLiteral("huge"));
        for (int s = 0; s < 6; ++s)
        {
            for (int d = 0; d < 10; ++d)
            {
                huge.setPng(s, d, QByteArray(60000, 'x'));
            }
        }

        QVERIFY(ClockFilesystem::build(ClockConfig(), { huge }, kPartition, &err).isEmpty());
        QVERIFY(err.contains(QStringLiteral("KiB")));
    }

    // Cross-check with PlatformIO's mklittlefs (the tool the original firmware used), if installed.
    void readableByMklittlefs()
    {
        const QString tool = QDir::home().filePath(QStringLiteral(".platformio/packages/tool-mklittlefs/mklittlefs"));
        if (!QFile::exists(tool))
        {
            QSKIP("mklittlefs not installed");
        }

        LittleFsImage fs(kPartition);
        QVERIFY(fs.format());
        QVERIFY(fs.addFile(QStringLiteral("hello.txt"), "hello clock"));
        QTemporaryDir dir;
        QFile f(dir.filePath(QStringLiteral("fs.bin")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(fs.finish());
        f.close();
        QProcess p;
        p.start(tool,
            { QStringLiteral("-l"),
                QStringLiteral("-b"),
                QStringLiteral("4096"),
                QStringLiteral("-p"),
                QStringLiteral("256"),
                QStringLiteral("-s"),
                QString::number(kPartition),
                f.fileName() });
        QVERIFY(p.waitForFinished(10000));
        const QString out = QString::fromLocal8Bit(p.readAllStandardOutput() + p.readAllStandardError());
        QVERIFY2(p.exitCode() == 0, qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("hello.txt")), qPrintable(out));
    }
};

QTEST_GUILESS_MAIN(TestLittleFs)
#include "test_littlefs.moc"
