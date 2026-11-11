// SPDX-License-Identifier: GPL-3.0-or-later
#include "FirmwareBundle.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace
{

FlashImage readImage(const QString& dir, const QJsonObject& o)
{
    FlashImage fi;
    fi.path = QDir(dir).absoluteFilePath(o.value(QStringLiteral("file")).toString());
    fi.offset = quint32(o.value(QStringLiteral("offset")).toDouble());
    fi.sha256 = o.value(QStringLiteral("sha256")).toString();
    return fi;
}

bool checkFile(const FlashImage& img, QString* error)
{
    QFile f(img.path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QStringLiteral("Missing firmware file %1").arg(img.path);
        }

        return false;
    }

    const QString sum = QString::fromLatin1(QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex());
    if (sum != img.sha256)
    {
        if (error)
        {
            *error = QStringLiteral("Firmware file %1 is damaged (checksum mismatch)").arg(img.path);
        }

        return false;
    }

    return true;
}

} // namespace

bool FirmwareBundle::load(const QString& dir, QString* error)
{
    auto fail = [error](const QString& msg) {
        if (error)
        {
            *error = msg;
        }

        return false;
    };
    QFile f(QDir(dir).filePath(QStringLiteral("manifest.json")));
    if (!f.open(QIODevice::ReadOnly))
    {
        return fail(QStringLiteral("Firmware manifest not found in %1").arg(dir));
    }

    QJsonParseError pe;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll(), &pe).object();
    if (pe.error != QJsonParseError::NoError)
    {
        return fail(QStringLiteral("Firmware manifest is damaged: %1").arg(pe.errorString()));
    }

    version_ = root.value(QStringLiteral("firmware_version")).toString();

    // ---- flash layouts --------------------------------------------------------------
    layouts_.clear();
    quint32 lowestFilesystem = 0xFFFFFFFF;
    const QJsonObject layouts = root.value(QStringLiteral("layouts")).toObject();
    for (auto it = layouts.begin(); it != layouts.end(); ++it)
    {
        const QJsonObject l = it.value().toObject();
        const QJsonObject fs = l.value(QStringLiteral("filesystem")).toObject();
        FlashLayout layout;
        layout.id = it.key();
        layout.flashSize = quint64(l.value(QStringLiteral("flash_size")).toDouble());
        layout.headerSizeCode = l.value(QStringLiteral("header_size_code")).toInt(-1);
        layout.partitions = readImage(dir, l.value(QStringLiteral("partitions")).toObject());
        layout.filesystemOffset = quint32(fs.value(QStringLiteral("offset")).toDouble());
        layout.filesystemSize = quint32(fs.value(QStringLiteral("size")).toDouble());
        if (fs.value(QStringLiteral("type")).toString() != QLatin1String("littlefs")
            || fs.value(QStringLiteral("block_size")).toInt() != 4096 || layout.filesystemSize == 0
            || quint64(layout.filesystemOffset) + layout.filesystemSize > layout.flashSize || layout.headerSizeCode < 0
            || layout.headerSizeCode > 4)
        {
            return fail(QStringLiteral("Unsupported flash layout %1 in the firmware manifest").arg(layout.id));
        }

        lowestFilesystem = std::min(lowestFilesystem, layout.filesystemOffset);
        layouts_.append(layout);
    }

    if (layouts_.isEmpty())
    {
        return fail(QStringLiteral("The firmware bundle contains no flash layouts"));
    }

    std::sort(layouts_.begin(), layouts_.end(), [](const FlashLayout& a, const FlashLayout& b) {
        return a.flashSize < b.flashSize;
    });

    // ---- models ------------------------------------------------------------------------
    models_.clear();
    const QJsonObject models = root.value(QStringLiteral("models")).toObject();
    for (auto it = models.begin(); it != models.end(); ++it)
    {
        const QJsonObject m = it.value().toObject();
        ClockModel model;
        model.id = it.key();
        model.title = m.value(QStringLiteral("title")).toString(model.id);
        for (const QJsonValue& v : m.value(QStringLiteral("images")).toArray())
        {
            const FlashImage fi = readImage(dir, v.toObject());
            // Must not overlap the partition table or any file system.
            if (fi.offset < 0x1000 || fi.offset >= lowestFilesystem || fi.offset == 0x8000)
            {
                return fail(QStringLiteral("Firmware image %1 has an unexpected offset").arg(fi.path));
            }

            if (fi.path.endsWith(QLatin1String("/bootloader.bin")))
            {
                model.bootloader = fi;
            }
            else if (fi.path.endsWith(QLatin1String("/app.bin")))
            {
                model.app = fi;
            }
        }

        if (model.bootloader.path.isEmpty() || model.app.path.isEmpty())
        {
            return fail(QStringLiteral("Firmware for %1 is incomplete").arg(model.id));
        }

        models_.append(model);
    }

    if (models_.isEmpty())
    {
        return fail(QStringLiteral("The firmware bundle contains no clock models"));
    }

    return true;
}

QString FirmwareBundle::locate()
{
    const QByteArray env = qgetenv("TUBECLOCK_FIRMWARE_DIR");
    QStringList candidates;
    if (!env.isEmpty())
    {
        candidates << QString::fromLocal8Bit(env);
    }

    for (const QString& d : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
    {
        candidates << QDir(d).filePath(QStringLiteral("tubeclock/firmware"));
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath(QStringLiteral("../share/tubeclock/firmware"));
    for (const QString& c : candidates)
    {
        if (QFile::exists(QDir(c).filePath(QStringLiteral("manifest.json"))))
        {
            return QDir(c).absolutePath();
        }
    }

    return QString();
}

const ClockModel* FirmwareBundle::model(const QString& id) const
{
    for (const ClockModel& m : models_)
    {
        if (m.id == id)
        {
            return &m;
        }
    }

    return nullptr;
}

const FlashLayout* FirmwareBundle::layout(const QString& flashSize) const
{
    for (const FlashLayout& l : layouts_)
    {
        if (l.id == flashSize)
        {
            return &l;
        }
    }

    return nullptr;
}

QStringList FirmwareBundle::supportedFlashSizes() const
{
    QStringList ids;
    for (const FlashLayout& l : layouts_)
    {
        ids << l.id;
    }

    return ids;
}

bool FirmwareBundle::verify(const QString& modelId, const QString& layoutId, QString* error) const
{
    const ClockModel* m = model(modelId);
    if (m == nullptr)
    {
        if (error)
        {
            *error = QStringLiteral("Unknown clock model %1").arg(modelId);
        }

        return false;
    }

    const FlashLayout* l = layout(layoutId);
    if (l == nullptr)
    {
        if (error)
        {
            *error = QStringLiteral("Flash size %1 is not supported").arg(layoutId);
        }

        return false;
    }

    return checkFile(m->bootloader, error) && checkFile(m->app, error) && checkFile(l->partitions, error);
}
