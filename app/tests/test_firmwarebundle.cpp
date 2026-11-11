// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "FirmwareBundle.hpp"

class TestFirmwareBundle : public QObject
{
    Q_OBJECT
private:
    static QString writeFile(const QString& path, const QByteArray& data)
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
        {
            return QString();
        }

        f.write(data);
        return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    }

    static QJsonObject layout(const QString& dir, const QString& id, int mb, int code, int fsSize)
    {
        const QString file = QStringLiteral("layouts/%1/partitions.bin").arg(id);
        const QString sum = writeFile(QDir(dir).filePath(file), "table " + id.toUtf8());
        return { { "flash_size", mb * 1024 * 1024 },
            { "header_size_code", code },
            { "partitions", QJsonObject{ { "file", file }, { "offset", 0x8000 }, { "sha256", sum } } },
            { "filesystem",
                QJsonObject{
                    { "type", "littlefs" }, { "offset", 0x140000 }, { "size", fsSize }, { "block_size", 4096 } } } };
    }

    static void writeManifest(const QString& dir, const QJsonObject& layouts, const QJsonObject& models)
    {
        const QJsonObject root{ { "firmware_version", "2.0.0" }, { "layouts", layouts }, { "models", models } };
        writeFile(QDir(dir).filePath(QStringLiteral("manifest.json")), QJsonDocument(root).toJson());
    }

    static QJsonObject model(const QString& dir, quint32 appOffset = 0x10000)
    {
        const QString sumBoot = writeFile(QDir(dir).filePath(QStringLiteral("EleksTube/bootloader.bin")), "boot");
        const QString sumApp = writeFile(QDir(dir).filePath(QStringLiteral("EleksTube/app.bin")), "app");
        return { { "title", "EleksTube IPS" },
            { "images",
                QJsonArray{
                    QJsonObject{ { "file", "EleksTube/bootloader.bin" }, { "offset", 0x1000 }, { "sha256", sumBoot } },
                    QJsonObject{
                        { "file", "EleksTube/app.bin" }, { "offset", int(appOffset) }, { "sha256", sumApp } } } } };
    }

private Q_SLOTS:
    void loadAndVerify()
    {
        QTemporaryDir dir;
        writeManifest(dir.path(),
            { { "8MB", layout(dir.path(), "8MB", 8, 3, 0x6b0000) },
                { "4MB", layout(dir.path(), "4MB", 4, 2, 0x2b0000) } },
            { { "EleksTube", model(dir.path()) } });
        FirmwareBundle b;
        QString err;
        QVERIFY2(b.load(dir.path(), &err), qPrintable(err));
        QCOMPARE(b.version(), QStringLiteral("2.0.0"));
        QCOMPARE(b.supportedFlashSizes(), QStringList({ "4MB", "8MB" })); // sorted by size
        const FlashLayout* l8 = b.layout(QStringLiteral("8MB"));
        QVERIFY(l8 != nullptr);
        QCOMPARE(l8->flashSize, quint64(0x800000));
        QCOMPARE(l8->headerSizeCode, 3);
        QCOMPARE(l8->filesystemSize, 0x6b0000u);
        QVERIFY(b.layout(QStringLiteral("2MB")) == nullptr);
        QCOMPARE(b.model(QStringLiteral("EleksTube"))->app.offset, 0x10000u);
        QVERIFY(b.verify(QStringLiteral("EleksTube"), QStringLiteral("8MB"), &err));

        writeFile(dir.filePath(QStringLiteral("layouts/8MB/partitions.bin")), "tampered");
        QVERIFY(!b.verify(QStringLiteral("EleksTube"), QStringLiteral("8MB"), &err));
        QVERIFY(err.contains(QStringLiteral("checksum")));
        QVERIFY(b.verify(QStringLiteral("EleksTube"), QStringLiteral("4MB"), &err));
        QVERIFY(!b.verify(QStringLiteral("Nope"), QStringLiteral("4MB"), &err));
        QVERIFY(!b.verify(QStringLiteral("EleksTube"), QStringLiteral("32MB"), &err));
    }

    void rejectsBadManifests()
    {
        QTemporaryDir dir;
        FirmwareBundle b;
        QString err;
        QVERIFY(!b.load(dir.path(), &err)); // missing
        // An image overlapping the file system would be overwritten - refuse.
        writeManifest(dir.path(),
            { { "4MB", layout(dir.path(), "4MB", 4, 2, 0x2b0000) } },
            { { "X", model(dir.path(), 0x150000) } });
        QVERIFY(!b.load(dir.path(), &err));
        // File system larger than the flash.
        writeManifest(dir.path(),
            { { "4MB", layout(dir.path(), "4MB", 4, 2, 0x400000) } },
            { { "EleksTube", model(dir.path()) } });
        QVERIFY(!b.load(dir.path(), &err));
        writeManifest(dir.path(), {}, { { "EleksTube", model(dir.path()) } });
        QVERIFY(!b.load(dir.path(), &err));
        writeManifest(dir.path(), { { "4MB", layout(dir.path(), "4MB", 4, 2, 0x2b0000) } }, {});
        QVERIFY(!b.load(dir.path(), &err));
    }

    void realBundleIfBuilt()
    {
        const QString dir = QStringLiteral(REPO_ROOT "/dist/firmware");
        if (!QFile::exists(dir + QStringLiteral("/manifest.json")))
        {
            QSKIP("run scripts/collect_firmware.py first");
        }

        FirmwareBundle b;
        QString err;
        QVERIFY2(b.load(dir, &err), qPrintable(err));
        QCOMPARE(b.models().size(), 4);
        QCOMPARE(b.supportedFlashSizes(), QStringList({ "4MB", "8MB", "16MB" }));
        for (const ClockModel& m : b.models())
        {
            for (const FlashLayout& l : b.layouts())
            {
                QVERIFY2(b.verify(m.id, l.id, &err), qPrintable(err));
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestFirmwareBundle)
#include "test_firmwarebundle.moc"
