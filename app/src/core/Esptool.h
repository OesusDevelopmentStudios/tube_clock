// SPDX-License-Identifier: GPL-3.0-or-later
//
// Talking to the clock's ESP32 bootloader through esptool (https://github.com/espressif/esptool,
// GPL-2.0-or-later), which the Flatpak bundles. The app only builds command lines and
// parses esptool's output; esptool runs as a separate process.
#pragma once

#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <optional>

namespace Esptool {

// The CH340 USB bridges on these clocks often fail at 921600 baud.
constexpr int kDefaultBaud = 460800;
constexpr int kSafeBaud = 115200;

struct ChipInfo
{
  QString chip;      // "ESP32-D0WD (revision v1.0)"
  QString mac;       // "98:cd:ac:57:4b:b8"
  QString flashSize; // "4MB"
  bool valid() const { return !chip.isEmpty() && !mac.isEmpty(); }
};

QStringList
chipInfoArgs(const QString& port, int baud);
QStringList
backupArgs(const QString& port, int baud, const QString& outFile, quint64 flashSize);
// images: (offset, file). Never includes an erase of the whole chip.
QStringList
flashArgs(const QString& port, int baud, const QList<QPair<quint32, QString>>& images);
// Writes a full backup back at offset 0.
QStringList
restoreArgs(const QString& port, int baud, const QString& backupFile);

QString
stripAnsi(const QString& text);
// Percentage from a progress line ("Writing at 0x00010000 [===>  ] 10.4% ..." or "(10 %)").
std::optional<double>
parseProgress(const QString& line);
ChipInfo
parseChipInfo(const QString& output);
// Last line starting with "A fatal error occurred:" (without the prefix), or empty.
QString
parseFatalError(const QString& output);

// The esptool command: $TUBECLOCK_ESPTOOL, "esptool" in PATH, or "python3 -m esptool".
QPair<QString, QStringList>
command();

} // namespace Esptool

// Runs one esptool command and reports progress.
class EsptoolRunner : public QObject
{
  Q_OBJECT
public:
  explicit EsptoolRunner(QObject* parent = nullptr);
  ~EsptoolRunner() override;

  bool isRunning() const;
  QString output() const { return output_; }

public Q_SLOTS:
  void start(const QStringList& args);
  void cancel();

Q_SIGNALS:
  void progress(double percent);
  void lineReceived(const QString& line);
  void finished(bool ok, const QString& error);

private:
  void onReadyRead();
  void onFinished(int exitCode, QProcess::ExitStatus status);

  QProcess* process_ = nullptr;
  QString output_;
  QString pending_;
};
