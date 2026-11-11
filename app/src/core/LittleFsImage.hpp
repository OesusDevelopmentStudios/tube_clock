// SPDX-License-Identifier: GPL-3.0-or-later
//
// Builds the LittleFS partition image for the clock in memory (vendored littlefs).
// Geometry and on-disk version match the ESP32 Arduino core 2.0.x (esp_littlefs):
// 4096 byte blocks, littlefs disk format 2.0, file names up to 31 characters.
#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>

class LittleFsImage
{
public:
    static constexpr quint32 kBlockSize = 4096;

    explicit LittleFsImage(quint32 sizeBytes);
    ~LittleFsImage();
    LittleFsImage(const LittleFsImage&) = delete;
    LittleFsImage& operator=(const LittleFsImage&) = delete;

    // Formats and mounts an empty filesystem. Must be called first.
    bool format(QString* error = nullptr);
    bool addFile(const QString& name, const QByteArray& data, QString* error = nullptr);
    // Unmounts and returns the finished image (unused space is 0xFF like erased flash).
    QByteArray finish();

    quint64 usedBytes() const;
    quint32 sizeBytes() const { return quint32(storage_.size()); }

    // Reads every file of an image (used by tests and to inspect backups).
    static QMap<QString, QByteArray> readAll(const QByteArray& image, QString* error = nullptr);

    struct Impl;

private:
    QByteArray storage_;
    Impl* d_;
    bool mounted_ = false;
};
