// SPDX-License-Identifier: GPL-3.0-or-later
#include "EspImage.h"

#include <QCryptographicHash>

namespace EspImage {

namespace {
constexpr char kMagic = char(0xE9);
constexpr int kHeaderSize = 24;
constexpr int kSizeByte = 3;  // high nibble: flash size, low nibble: flash frequency
constexpr int kHashFlag = 23; // hash_appended
constexpr int kHashSize = 32; // SHA-256

bool
isImage(const QByteArray& image)
{
  return image.size() >= kHeaderSize && image.at(0) == kMagic;
}
} // namespace

int
sizeCodeForBytes(quint64 flashBytes)
{
  for (int code = 0; code <= 4; ++code)
    if (flashBytes == (quint64(1024 * 1024) << code))
      return code;
  return -1;
}

int
flashSizeCode(const QByteArray& image)
{
  return isImage(image) ? (uchar(image.at(kSizeByte)) >> 4) : -1;
}

bool
hasAppendedHash(const QByteArray& image)
{
  return isImage(image) && image.at(kHashFlag) == 1 && image.size() >= kHeaderSize + kHashSize;
}

bool
hashValid(const QByteArray& image)
{
  if (!hasAppendedHash(image))
    return isImage(image);
  const QByteArray body = image.left(image.size() - kHashSize);
  return QCryptographicHash::hash(body, QCryptographicHash::Sha256) == image.right(kHashSize);
}

bool
setFlashSize(QByteArray& image, int sizeCode, QString* error)
{
  auto fail = [error](const QString& msg) {
    if (error)
      *error = msg;
    return false;
  };
  if (!isImage(image))
    return fail(QStringLiteral("not an ESP32 firmware image"));
  if (sizeCode < 0 || sizeCode > 4)
    return fail(QStringLiteral("unsupported flash size"));
  if (!hashValid(image))
    return fail(QStringLiteral("the firmware image is damaged (hash mismatch)"));
  image[kSizeByte] = char((sizeCode << 4) | (uchar(image.at(kSizeByte)) & 0x0F));
  if (hasAppendedHash(image)) {
    const QByteArray hash = QCryptographicHash::hash(image.left(image.size() - kHashSize), QCryptographicHash::Sha256);
    image.replace(image.size() - kHashSize, kHashSize, hash);
  }
  return true;
}

} // namespace EspImage
