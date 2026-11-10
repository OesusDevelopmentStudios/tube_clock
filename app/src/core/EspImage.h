// SPDX-License-Identifier: GPL-3.0-or-later
//
// ESP32 firmware image header handling (format: ESP-IDF esp_image_header_t).
//
// The boot loader and the app both record the flash chip size in their header (high
// nibble of byte 3). The firmware is built for 4 MB; for clocks with more flash the app
// rewrites that field and, when the image carries an appended SHA-256 (byte 23), the
// hash over the image. This is what `esptool write-flash --flash-size` does for the
// boot loader, applied to the app image as well.
#pragma once

#include <QByteArray>
#include <QString>

namespace EspImage {

// Header size code for a flash size in bytes (1 MB = 0 ... 16 MB = 4), or -1.
int
sizeCodeForBytes(quint64 flashBytes);
// Flash size code stored in an image, or -1 if the data is not an ESP32 image.
int
flashSizeCode(const QByteArray& image);
bool
hasAppendedHash(const QByteArray& image);
// True if the image has no appended hash or the hash matches.
bool
hashValid(const QByteArray& image);
// Sets the flash size code and updates the appended hash.
bool
setFlashSize(QByteArray& image, int sizeCode, QString* error = nullptr);

} // namespace EspImage
