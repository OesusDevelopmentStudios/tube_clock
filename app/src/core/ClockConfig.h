// SPDX-License-Identifier: GPL-3.0-or-later
//
// The clock configuration (/config.json on the clock). Same schema as the firmware
// (lib/clockcore/include/clockcore/Config.h, documented in docs/config-format.md).
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct ClockConfig
{
  enum class WifiMode
  {
    Off,
    Dhcp,
    Static
  };

  int version = 1;
  QString name; // host name; empty = firmware default (tubeclock-xxxxxx)

  WifiMode wifiMode = WifiMode::Off;
  QString ssid;
  QString password;
  QString ip;
  QString gateway;
  QString netmask = QStringLiteral("255.255.255.0");
  QString dns;

  bool apiEnabled = false;
  QString token;

  QString tzName = QStringLiteral("Etc/UTC");
  QString tzPosix = QStringLiteral("UTC0");
  bool dst = true;
  QString ntpServer = QStringLiteral("pool.ntp.org");
  bool ntpEnabled = true;
  bool twelveHour = false;

  QString imageSet;
  int lcdBrightness = 10; // 1..10

  QString pattern = QStringLiteral("rainbow"); // off, constant, rainbow, pulse, breath
  int colorPhase = 0;                          // 0..767
  int intensity = 7;                           // 0..7

  QJsonObject toJson(bool includeSecrets = true) const;
  // Missing keys keep their defaults. Unknown values are reported in `errors`.
  static ClockConfig fromJson(const QJsonObject& obj, QStringList* errors = nullptr);
  // Human readable problems, empty if the clock firmware will accept the config.
  QStringList validate() const;

  static QString generateToken(); // 32 random hex characters
  static QStringList patterns();
  static QString wifiModeName(WifiMode mode);
};
