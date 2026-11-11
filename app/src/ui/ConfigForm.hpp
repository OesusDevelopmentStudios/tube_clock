// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>

#include "ClockConfig.hpp"

// Editable clock configuration for QML forms (wraps ClockConfig field by field).
class ConfigForm : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("owned by controllers")
    Q_PROPERTY(QString clockName READ clockName WRITE setClockName NOTIFY changed)
    Q_PROPERTY(QString ssid READ ssid WRITE setSsid NOTIFY changed)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY changed)
    Q_PROPERTY(QString ip READ ip WRITE setIp NOTIFY changed)
    Q_PROPERTY(QString gateway READ gateway WRITE setGateway NOTIFY changed)
    Q_PROPERTY(QString netmask READ netmask WRITE setNetmask NOTIFY changed)
    Q_PROPERTY(QString dns READ dns WRITE setDns NOTIFY changed)
    Q_PROPERTY(bool remoteAccess READ remoteAccess WRITE setRemoteAccess NOTIFY changed)
    Q_PROPERTY(QString tzName READ tzName WRITE setTzName NOTIFY changed)
    Q_PROPERTY(QString tzPosix READ tzPosix WRITE setTzPosix NOTIFY changed)
    Q_PROPERTY(bool dst READ dst WRITE setDst NOTIFY changed)
    Q_PROPERTY(QString ntpServer READ ntpServer WRITE setNtpServer NOTIFY changed)
    Q_PROPERTY(bool ntpEnabled READ ntpEnabled WRITE setNtpEnabled NOTIFY changed)
    Q_PROPERTY(bool twelveHour READ twelveHour WRITE setTwelveHour NOTIFY changed)
    Q_PROPERTY(QString imageSet READ imageSet WRITE setImageSet NOTIFY changed)
    Q_PROPERTY(int lcdBrightness READ lcdBrightness WRITE setLcdBrightness NOTIFY changed)
    Q_PROPERTY(QString pattern READ pattern WRITE setPattern NOTIFY changed)
    Q_PROPERTY(int colorPhase READ colorPhase WRITE setColorPhase NOTIFY changed)
    Q_PROPERTY(int intensity READ intensity WRITE setIntensity NOTIFY changed)
    // "off" | "dhcp" | "static"
    Q_PROPERTY(QString wifiMode READ wifiMode WRITE setWifiMode NOTIFY changed)
    Q_PROPERTY(QStringList problems READ problems NOTIFY changed)
    Q_PROPERTY(QStringList patterns READ patterns CONSTANT)

public:
    explicit ConfigForm(QObject* parent = nullptr)
      : QObject(parent)
    {
    }

    QString clockName() const { return c_.name; }
    void setClockName(const QString& v) { assign(c_.name, v); }
    QString ssid() const { return c_.ssid; }
    void setSsid(const QString& v) { assign(c_.ssid, v); }
    QString password() const { return c_.password; }
    void setPassword(const QString& v) { assign(c_.password, v); }
    QString ip() const { return c_.ip; }
    void setIp(const QString& v) { assign(c_.ip, v); }
    QString gateway() const { return c_.gateway; }
    void setGateway(const QString& v) { assign(c_.gateway, v); }
    QString netmask() const { return c_.netmask; }
    void setNetmask(const QString& v) { assign(c_.netmask, v); }
    QString dns() const { return c_.dns; }
    void setDns(const QString& v) { assign(c_.dns, v); }
    bool remoteAccess() const { return c_.apiEnabled; }
    void setRemoteAccess(bool v) { assign(c_.apiEnabled, v); }
    QString tzName() const { return c_.tzName; }
    void setTzName(const QString& v) { assign(c_.tzName, v); }
    QString tzPosix() const { return c_.tzPosix; }
    void setTzPosix(const QString& v) { assign(c_.tzPosix, v); }
    bool dst() const { return c_.dst; }
    void setDst(bool v) { assign(c_.dst, v); }
    QString ntpServer() const { return c_.ntpServer; }
    void setNtpServer(const QString& v) { assign(c_.ntpServer, v); }
    bool ntpEnabled() const { return c_.ntpEnabled; }
    void setNtpEnabled(bool v) { assign(c_.ntpEnabled, v); }
    bool twelveHour() const { return c_.twelveHour; }
    void setTwelveHour(bool v) { assign(c_.twelveHour, v); }
    QString imageSet() const { return c_.imageSet; }
    void setImageSet(const QString& v) { assign(c_.imageSet, v); }
    int lcdBrightness() const { return c_.lcdBrightness; }
    void setLcdBrightness(int v) { assign(c_.lcdBrightness, v); }
    QString pattern() const { return c_.pattern; }
    void setPattern(const QString& v) { assign(c_.pattern, v); }
    int colorPhase() const { return c_.colorPhase; }
    void setColorPhase(int v) { assign(c_.colorPhase, v); }
    int intensity() const { return c_.intensity; }
    void setIntensity(int v) { assign(c_.intensity, v); }

    QString wifiMode() const { return ClockConfig::wifiModeName(c_.wifiMode); }
    void setWifiMode(const QString& m);
    QStringList problems() const { return c_.validate(); }
    QStringList patterns() const { return ClockConfig::patterns(); }

    // Selects a zone and looks up its POSIX rule.
    Q_INVOKABLE bool setZone(const QString& zoneId);
    // RGB colour of a backlight colour phase (same formula as the firmware).
    Q_INVOKABLE QColor phaseColor(int phase) const;

    ClockConfig config() const { return c_; }
    void setConfig(const ClockConfig& c)
    {
        c_ = c;
        Q_EMIT changed();
    }

Q_SIGNALS:
    void changed();

private:
    template<typename T>
    void assign(T& field, const T& value)
    {
        if (field == value)
        {
            return;
        }

        field = value;
        Q_EMIT changed();
    }

    ClockConfig c_;
};
