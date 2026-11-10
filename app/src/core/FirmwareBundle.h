// SPDX-License-Identifier: GPL-3.0-or-later
//
// The prebuilt firmware shipped with the app (created by scripts/collect_firmware.py).
//
// One boot loader + app per clock model, and one flash layout (partition table +
// file system location) per supported flash chip size.
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct FlashImage
{
  QString path; // absolute
  quint32 offset = 0;
  QString sha256;
};

struct ClockModel
{
  QString id;    // "EleksTube", "EleksTube_Gen2", "SI_HAI", "PunkCyber"
  QString title; // human readable
  FlashImage bootloader;
  FlashImage app;
};

struct FlashLayout
{
  QString id;            // "4MB", "8MB", "16MB" (as reported by esptool)
  quint64 flashSize = 0; // bytes
  int headerSizeCode = -1;
  FlashImage partitions;
  quint32 filesystemOffset = 0;
  quint32 filesystemSize = 0;
};

class FirmwareBundle
{
public:
  // Loads <dir>/manifest.json. Returns false with *error on problems.
  bool load(const QString& dir, QString* error = nullptr);
  // Searches TUBECLOCK_FIRMWARE_DIR, the installed data dir and the build tree.
  static QString locate();

  QString version() const { return version_; }
  QList<ClockModel> models() const { return models_; }
  const ClockModel* model(const QString& id) const;
  QList<FlashLayout> layouts() const { return layouts_; }
  // Layout for a flash size as reported by esptool ("8MB"), or nullptr if unsupported.
  const FlashLayout* layout(const QString& flashSize) const;
  QStringList supportedFlashSizes() const;
  // Checks the SHA-256 of the files of a model and of the layout's partition table.
  bool verify(const QString& modelId, const QString& layoutId, QString* error = nullptr) const;

private:
  QString version_;
  QList<ClockModel> models_;
  QList<FlashLayout> layouts_;
};
