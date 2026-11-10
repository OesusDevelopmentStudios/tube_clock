// SPDX-License-Identifier: GPL-3.0-or-later
#include "FlashController.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "AppSettings.h"
#include "ClockFilesystem.h"
#include "EspImage.h"
#include "TokenStore.h"

FlashController::FlashController(QObject* parent)
  : QObject(parent)
{
  const QString dir = FirmwareBundle::locate();
  if (dir.isEmpty())
    bundleError_ = QStringLiteral("The firmware files are missing from this installation.");
  else
    bundle_.load(dir, &bundleError_); // sets bundleError_ on failure
  sets_.loadBundled(true);
  // Sensible defaults: the computer's time zone and the matching NTP pool.
  const QString zone = AppSettings::instance()->systemZoneId();
  if (config_.setZone(zone))
    config_.setNtpServer(AppSettings::instance()->suggestedNtpServer(zone));
  config_.setRemoteAccess(true);

  connect(&runner_, &EsptoolRunner::progress, this, [this](double p) {
    progress_ = p;
    Q_EMIT progressChanged();
  });
  connect(&runner_, &EsptoolRunner::lineReceived, this, &FlashController::appendLog);
  connect(&runner_, &EsptoolRunner::finished, this, &FlashController::onRunnerFinished);
}

QVariantList
FlashController::models() const
{
  QVariantList out;
  for (const ClockModel& m : bundle_.models())
    out.append(QVariantMap{ { QStringLiteral("id"), m.id }, { QStringLiteral("title"), m.title } });
  return out;
}

const FlashLayout*
FlashController::currentLayout() const
{
  if (chip_.valid())
    return bundle_.layout(chip_.flashSize);
  const QList<FlashLayout> layouts = bundle_.layouts();
  return layouts.isEmpty() ? nullptr : bundle_.layout(layouts.first().id);
}

qint64
FlashController::capacityBytes() const
{
  const FlashLayout* l = currentLayout();
  return l ? ClockFilesystem::capacityBytes(l->filesystemSize) : 0;
}

QString
FlashController::defaultName() const
{
  // Same rule as the firmware: tubeclock-<last three MAC bytes>.
  QString m = chip_.mac;
  m.remove(QLatin1Char(':'));
  return m.size() == 12 ? QStringLiteral("tubeclock-") + m.right(6) : QString();
}

QStringList
FlashController::problems() const
{
  QStringList p;
  if (!bundleError_.isEmpty())
    p << bundleError_;
  if (port_.isEmpty())
    p << QStringLiteral("Select the serial port of the clock.");
  if (!chip_.valid())
    p << QStringLiteral("Connect to the clock first (Detect).");
  else if (currentLayout() == nullptr)
    p << QStringLiteral("Clocks with %1 of flash are not supported.").arg(chip_.flashSize);
  ClockConfig c = config_.config();
  if (c.apiEnabled && c.token.isEmpty())
    c.token = QStringLiteral("0000000000000000"); // generated when flashing
  p << c.validate();
  if (sets_.includedNames().isEmpty())
    p << QStringLiteral("Select at least one image set.");
  if (sets_.includedBytes() > capacityBytes())
    p << QStringLiteral("The selected image sets do not fit on the clock.");
  return p;
}

void
FlashController::appendLog(const QString& line)
{
  log_ += line + QLatin1Char('\n');
  Q_EMIT logChanged();
}

void
FlashController::fail(const QString& message)
{
  error_ = message;
  task_.clear();
  step_ = Step::None;
  appendLog(QStringLiteral("Error: ") + message);
  Q_EMIT stateChanged();
  Q_EMIT finished(false, message);
}

void
FlashController::run(Step step, const QString& task, const QStringList& args)
{
  if (busy())
    return;
  step_ = step;
  task_ = task;
  error_.clear();
  progress_ = 0;
  appendLog(QStringLiteral("== %1 ==").arg(task));
  Q_EMIT stateChanged();
  Q_EMIT progressChanged();
  runner_.start(args);
}

void
FlashController::detect()
{
  if (port_.isEmpty())
    return fail(QStringLiteral("Select the serial port of the clock."));
  chip_ = {};
  run(Step::Detect, QStringLiteral("Connecting to the clock"), Esptool::chipInfoArgs(port_, baud_));
}

void
FlashController::backup()
{
  if (!chip_.valid() || currentLayout() == nullptr)
    return fail(QStringLiteral("Connect to the clock first."));
  QString mac = chip_.mac;
  mac.remove(QLatin1Char(':'));
  backupFile_ =
    QDir(AppSettings::instance()->backupDir())
      .filePath(
        QStringLiteral("%1_%2.bin").arg(mac, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
  run(Step::Backup,
      QStringLiteral("Backing up the clock"),
      Esptool::backupArgs(port_, baud_, backupFile_, currentLayout()->flashSize));
}

void
FlashController::flash()
{
  const QStringList p = problems();
  if (!p.isEmpty())
    return fail(p.join(QLatin1Char('\n')));
  QString err;
  const FlashLayout* layout = currentLayout();
  if (!bundle_.verify(modelId_, layout->id, &err))
    return fail(err);

  ClockConfig c = config_.config();
  if (c.name.isEmpty())
    c.name = defaultName();
  if (c.apiEnabled && c.token.isEmpty())
    c.token = ClockConfig::generateToken();
  const QStringList names = sets_.includedNames();
  if (!names.contains(c.imageSet))
    c.imageSet = names.first();
  config_.setConfig(c);

  appendLog(QStringLiteral("== Preparing the clock's files =="));
  quint64 used = 0;
  const QByteArray fsImage = ClockFilesystem::build(c, sets_.includedSets(), layout->filesystemSize, &err, &used);
  if (fsImage.isEmpty())
    return fail(err);
  appendLog(QStringLiteral("Flash layout %1, filesystem: %2 KiB used of %3 KiB")
              .arg(layout->id)
              .arg(used / 1024)
              .arg(layout->filesystemSize / 1024));

  tmp_ = std::make_unique<QTemporaryDir>();
  const QString fsPath = tmp_->filePath(QStringLiteral("littlefs.bin"));
  QFile f(fsPath);
  if (!f.open(QIODevice::WriteOnly) || f.write(fsImage) != fsImage.size())
    return fail(QStringLiteral("Cannot write a temporary file."));
  f.close();

  // The firmware is built for 4 MB; record the real flash size in the image headers.
  const ClockModel* model = bundle_.model(modelId_);
  QString bootPath, appPath;
  if (!prepareImage(model->bootloader, layout->headerSizeCode, QStringLiteral("bootloader.bin"), &bootPath, &err) ||
      !prepareImage(model->app, layout->headerSizeCode, QStringLiteral("app.bin"), &appPath, &err))
    return fail(err);

  const QList<QPair<quint32, QString>> images = { { model->bootloader.offset, bootPath },
                                                  { layout->partitions.offset, layout->partitions.path },
                                                  { model->app.offset, appPath },
                                                  { layout->filesystemOffset, fsPath } };
  flashed_ = false;
  run(Step::Flash, QStringLiteral("Writing firmware and files"), Esptool::flashArgs(port_, baud_, images));
}

void
FlashController::restore(const QString& file)
{
  const QFileInfo fi(file);
  if (!chip_.valid() || currentLayout() == nullptr)
    return fail(QStringLiteral("Connect to the clock first, so the app can check its flash size."));
  if (!fi.exists() || quint64(fi.size()) != currentLayout()->flashSize)
    return fail(
      QStringLiteral("%1 is not a full backup of a clock with %2 of flash.").arg(fi.fileName(), chip_.flashSize));
  if (port_.isEmpty())
    return fail(QStringLiteral("Select the serial port of the clock."));
  run(Step::Restore, QStringLiteral("Restoring backup"), Esptool::restoreArgs(port_, baud_, file));
}

bool
FlashController::prepareImage(const FlashImage& source,
                              int sizeCode,
                              const QString& name,
                              QString* outPath,
                              QString* error)
{
  QFile in(source.path);
  if (!in.open(QIODevice::ReadOnly)) {
    *error = QStringLiteral("Cannot read %1").arg(source.path);
    return false;
  }
  QByteArray data = in.readAll();
  if (EspImage::flashSizeCode(data) != sizeCode && !EspImage::setFlashSize(data, sizeCode, error))
    return false;
  *outPath = tmp_->filePath(name);
  QFile out(*outPath);
  if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size()) {
    *error = QStringLiteral("Cannot write a temporary file.");
    return false;
  }
  return true;
}

void
FlashController::cancel()
{
  runner_.cancel();
}

void
FlashController::onRunnerFinished(bool ok, const QString& error)
{
  const Step step = step_;
  step_ = Step::None;
  task_.clear();
  if (!ok) {
    if (step == Step::Backup)
      QFile::remove(backupFile_);
    QString hint;
    if (error.contains(QLatin1String("Corrupt data")) || error.contains(QLatin1String("timed out")))
      hint = QStringLiteral(" Try a lower speed (115200 baud).");
    else if (error.contains(QLatin1String("Permission denied")))
      hint = QStringLiteral(" Your user may need to be in the 'dialout' group.");
    else if (error.contains(QLatin1String("Failed to connect")))
      hint = QStringLiteral(" Check the cable and port; some clocks need a button held while connecting.");
    return fail(error + hint);
  }

  QString message;
  switch (step) {
    case Step::Detect:
      chip_ = Esptool::parseChipInfo(runner_.output());
      if (!chip_.valid())
        return fail(QStringLiteral("The clock answered, but its chip could not be identified."));
      if (chip_.flashSize.isEmpty())
        return fail(QStringLiteral("The size of the clock's flash memory could not be detected."));
      if (bundle_.layout(chip_.flashSize) == nullptr) {
        const QString size = chip_.flashSize;
        chip_ = {};
        return fail(QStringLiteral("This clock has %1 of flash; supported sizes are %2.")
                      .arg(size, bundle_.supportedFlashSizes().join(QStringLiteral(", "))));
      }
      if (config_.clockName().isEmpty())
        config_.setClockName(defaultName());
      message = QStringLiteral("Found %1 (%2), %3 flash").arg(chip_.chip, chip_.mac, chip_.flashSize);
      Q_EMIT detected();
      break;
    case Step::Backup: {
      QFile f(backupFile_);
      if (!f.open(QIODevice::ReadOnly) || quint64(f.size()) != currentLayout()->flashSize)
        return fail(QStringLiteral("The backup is incomplete."));
      const QByteArray sum = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
      QFile s(backupFile_ + QStringLiteral(".sha256"));
      if (s.open(QIODevice::WriteOnly))
        s.write(sum + "  " + QFileInfo(backupFile_).fileName().toUtf8() + "\n");
      message = QStringLiteral("Backup saved: %1").arg(backupFile_);
      break;
    }
    case Step::Flash: {
      flashed_ = true;
      const ClockConfig c = config_.config();
      const QString host = c.wifiMode == ClockConfig::WifiMode::Static ? c.ip : c.name + QStringLiteral(".local");
      if (c.wifiMode != ClockConfig::WifiMode::Off)
        AppSettings::instance()->rememberClock(c.name, host, modelId_);
      if (c.apiEnabled) {
        TokenStore::write(c.name, c.token, [this](bool stored, const QString& err) {
          if (!stored)
            appendLog(QStringLiteral("Warning: the pairing token could not be stored in the keyring: ") + err);
        });
      }
      message = QStringLiteral("The clock has been flashed and restarts now.");
      break;
    }
    case Step::Restore:
      message = QStringLiteral("The backup has been written to the clock.");
      break;
    case Step::None:
      break;
  }
  appendLog(message);
  Q_EMIT stateChanged();
  Q_EMIT finished(true, message);
}
