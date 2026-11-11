// SPDX-License-Identifier: GPL-3.0-or-later
//
// Remote control of one clock over WiFi (HTTP API): status, settings, image sets,
// time sync with the PC.
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

#include "ClockClient.hpp"
#include "ConfigForm.hpp"
#include "ImageSetModel.hpp"

class DeviceController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name NOTIFY openedChanged)
    Q_PROPERTY(QString host READ host NOTIFY openedChanged)
    // IP address actually used (resolved host name or last known address).
    Q_PROPERTY(QString address READ address NOTIFY openedChanged)
    Q_PROPERTY(bool paired READ paired NOTIFY openedChanged)
    Q_PROPERTY(bool online READ online NOTIFY infoChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
    Q_PROPERTY(QVariantList sets READ sets NOTIFY setsChanged)
    Q_PROPERTY(ConfigForm* config READ config CONSTANT)
    Q_PROPERTY(ImageSetModel* imports READ imports CONSTANT)
    Q_PROPERTY(int uploadDone READ uploadDone NOTIFY uploadProgressChanged)
    Q_PROPERTY(int uploadTotal READ uploadTotal NOTIFY uploadProgressChanged)

public:
    explicit DeviceController(QObject* parent = nullptr);

    QString name() const { return name_; }
    QString host() const { return host_; }
    QString address() const { return address_; }
    bool paired() const { return !token_.isEmpty(); }
    bool online() const { return !info_.isEmpty(); }
    bool busy() const { return pending_ > 0; }
    QString message() const { return message_; }
    bool messageIsError() const { return messageIsError_; }
    QVariantMap info() const { return info_; }
    QVariantList sets() const { return sets_; }
    ConfigForm* config() { return &config_; }
    ImageSetModel* imports() { return &imports_; }
    int uploadDone() const { return uploadDone_; }
    int uploadTotal() const { return uploadTotal_; }

    // Opens a clock; the pairing token is read from the keyring.
    Q_INVOKABLE void open(const QString& name, const QString& host, int port = 80);
    // Manual pairing (token shown in the app that flashed the clock / from a backup).
    Q_INVOKABLE void pair(const QString& token);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void syncTime();
    Q_INVOKABLE void applySettings(); // time, display and backlight settings
    Q_INVOKABLE void applyNetwork();  // WiFi settings, the clock restarts
    Q_INVOKABLE void disableRemoteAccess();
    Q_INVOKABLE void selectSet(const QString& name);
    Q_INVOKABLE void deleteSet(const QString& name);
    Q_INVOKABLE void uploadSet(int importRow);
    Q_INVOKABLE void setDisplay(bool on);
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void forget();

Q_SIGNALS:
    void openedChanged();
    void infoChanged();
    void busyChanged();
    void messageChanged();
    void setsChanged();
    void uploadProgressChanged();

private:
    ClockClient::Callback track(std::function<void(const ClockClient::Result&)> onOk, const QString& okMessage = {});
    void setMessage(const QString& text, bool error);

    ClockClient client_;
    QString name_, host_, address_, token_;
    QVariantMap info_;
    QVariantList sets_;
    ConfigForm config_;
    ImageSetModel imports_;
    int pending_ = 0;
    QString message_;
    bool messageIsError_ = false;
    int uploadDone_ = 0, uploadTotal_ = 0;
};
