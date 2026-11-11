// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockFilesystem.hpp"

#include <QJsonDocument>

#include "LittleFsImage.hpp"

namespace ClockFilesystem
{

namespace
{
constexpr qint64 kBlock = LittleFsImage::kBlockSize;
// Room the firmware needs at runtime: config.json rewrites (temp file) and the
// upload temp file of the HTTP API.
constexpr qint64 kRuntimeReserve = 16 * 1024;
} // namespace

qint64 estimateBytes(const ImageSet& set)
{
    qint64 total = 0;
    for (const QByteArray& data : set.files())
    {
        total += ((data.size() + kBlock - 1) / kBlock) * kBlock; // data blocks
    }

    return total;
}

qint64 capacityBytes(quint32 partitionSize)
{
    // Superblock pair + root directory pair + reserve.
    return qint64(partitionSize) - 4 * kBlock - kRuntimeReserve;
}

QByteArray build(const ClockConfig& config,
    const QList<ImageSet>& sets,
    quint32 partitionSize,
    QString* error,
    quint64* usedBytes)
{
    auto fail = [error](const QString& msg) {
        if (error)
        {
            *error = msg;
        }

        return QByteArray();
    };
    const QStringList problems = config.validate();
    if (!problems.isEmpty())
    {
        return fail(problems.join(QLatin1Char('\n')));
    }

    qint64 needed = 0;
    for (const ImageSet& s : sets)
    {
        if (!ImageTools::isValidSetName(s.name()))
        {
            return fail(QStringLiteral("Invalid image set name '%1'").arg(s.name()));
        }

        if (!s.isComplete())
        {
            return fail(QStringLiteral("Image set '%1' is incomplete (%2 missing)")
                    .arg(s.name(), s.missing().join(QStringLiteral(", "))));
        }

        needed += estimateBytes(s);
    }

    if (needed > capacityBytes(partitionSize))
    {
        return fail(QStringLiteral("The image sets need about %1 KiB but the clock has %2 KiB.")
                .arg(needed / 1024)
                .arg(capacityBytes(partitionSize) / 1024));
    }

    LittleFsImage fs(partitionSize);
    QString err;
    if (!fs.format(&err))
    {
        return fail(err);
    }

    const QByteArray json = QJsonDocument(config.toJson(true)).toJson(QJsonDocument::Indented);
    if (!fs.addFile(QStringLiteral("config.json"), json, &err))
    {
        return fail(err);
    }

    for (const ImageSet& s : sets)
    {
        const auto files = s.files();
        for (auto it = files.cbegin(); it != files.cend(); ++it)
        {
            if (!fs.addFile(it.key(), it.value(), &err))
            {
                return fail(QStringLiteral("%1: %2").arg(it.key(), err));
            }
        }
    }

    if (usedBytes)
    {
        *usedBytes = fs.usedBytes();
    }

    return fs.finish();
}

} // namespace ClockFilesystem
