// SPDX-License-Identifier: GPL-3.0-or-later
//
// Drives the setup wizard: detect the clock, back up its flash, build the
// filesystem image (config + image sets) and flash firmware + filesystem.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QVariantList>

#include <memory>

#include "ConfigForm.hpp"
#include "Esptool.hpp"
#include "FirmwareBundle.hpp"
#include "ImageSetModel.hpp"

class FlashController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList models READ models CONSTANT)
    Q_PROPERTY(QString bundleError READ bundleError CONSTANT)
    Q_PROPERTY(QString firmwareVersion READ firmwareVersion CONSTANT)
    Q_PROPERTY(QString modelId MEMBER modelId_ NOTIFY settingsChanged)
    Q_PROPERTY(QString port MEMBER port_ NOTIFY settingsChanged)
    Q_PROPERTY(int baud MEMBER baud_ NOTIFY settingsChanged)
    Q_PROPERTY(ConfigForm* config READ config CONSTANT)
    Q_PROPERTY(ImageSetModel* imageSets READ imageSets CONSTANT)
    Q_PROPERTY(qint64 capacityBytes READ capacityBytes NOTIFY detected)
    Q_PROPERTY(QString flashSize READ flashSize NOTIFY detected)

    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString task READ task NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)
    Q_PROPERTY(QString chip READ chip NOTIFY detected)
    Q_PROPERTY(QString mac READ mac NOTIFY detected)
    Q_PROPERTY(QString backupFile READ backupFile NOTIFY stateChanged)
    Q_PROPERTY(bool flashed READ flashed NOTIFY stateChanged)

public:
    explicit FlashController(QObject* parent = nullptr);

    QVariantList models() const;
    QString bundleError() const { return bundleError_; }
    QString firmwareVersion() const { return bundle_.version(); }
    ConfigForm* config() { return &config_; }
    ImageSetModel* imageSets() { return &sets_; }
    qint64 capacityBytes() const;
    QString flashSize() const { return chip_.flashSize; }

    bool busy() const { return runner_.isRunning() || !task_.isEmpty(); }
    QString task() const { return task_; }
    double progress() const { return progress_; }
    QString log() const { return log_; }
    QString errorString() const { return error_; }
    QString chip() const { return chip_.chip; }
    QString mac() const { return chip_.mac; }
    QString backupFile() const { return backupFile_; }
    bool flashed() const { return flashed_; }

    // Problems that block flashing (config, sets, missing detection...).
    Q_INVOKABLE QStringList problems() const;

    Q_INVOKABLE void detect();
    Q_INVOKABLE void backup();
    Q_INVOKABLE void flash();
    Q_INVOKABLE void restore(const QString& backupFile);
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void settingsChanged();
    void stateChanged();
    void progressChanged();
    void logChanged();
    void detected();
    void finished(bool ok, const QString& message);

private:
    enum class Step
    {
        None,
        Detect,
        Backup,
        Flash,
        Restore
    };
    void run(Step step, const QString& task, const QStringList& args);
    void onRunnerFinished(bool ok, const QString& error);
    void appendLog(const QString& line);
    void fail(const QString& message);
    QString defaultName() const;
    // Layout of the connected clock; before detection the smallest one (for capacity display).
    const FlashLayout* currentLayout() const;
    bool prepareImage(const FlashImage& source, int sizeCode, const QString& name, QString* outPath, QString* error);

    FirmwareBundle bundle_;
    QString bundleError_;
    QString modelId_ = QStringLiteral("EleksTube");
    QString port_;
    int baud_ = Esptool::kDefaultBaud;
    ConfigForm config_;
    ImageSetModel sets_;

    EsptoolRunner runner_;
    Step step_ = Step::None;
    QString task_;
    double progress_ = 0;
    QString log_;
    QString error_;
    Esptool::ChipInfo chip_;
    QString backupFile_;
    bool flashed_ = false;
    std::unique_ptr<QTemporaryDir> tmp_;
};
