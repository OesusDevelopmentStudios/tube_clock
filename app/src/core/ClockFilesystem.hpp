// SPDX-License-Identifier: GPL-3.0-or-later
//
// Builds the complete LittleFS partition for a clock: /config.json + image sets.
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include "ClockConfig.hpp"
#include "ImageSet.hpp"

namespace ClockFilesystem
{

// Rough space a set needs on the clock (file data rounded up to blocks + metadata).
qint64 estimateBytes(const ImageSet& set);
// Usable bytes of a partition of the given size (littlefs keeps some blocks for itself).
qint64 capacityBytes(quint32 partitionSize);

// Returns the image or an empty array with *error set (e.g. sets do not fit).
QByteArray build(const ClockConfig& config,
    const QList<ImageSet>& sets,
    quint32 partitionSize,
    QString* error = nullptr,
    quint64* usedBytes = nullptr);

} // namespace ClockFilesystem
