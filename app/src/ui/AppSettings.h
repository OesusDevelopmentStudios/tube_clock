// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>

// Application wide settings and helpers for QML (singleton "AppSettings").
class AppSettings : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(
    bool disclaimerAccepted READ disclaimerAccepted WRITE setDisclaimerAccepted NOTIFY disclaimerAcceptedChanged)
  Q_PROPERTY(QString backupDir READ backupDir CONSTANT)
  Q_PROPERTY(QString version READ version CONSTANT)
  Q_PROPERTY(QVariantList knownClocks READ knownClocks NOTIFY knownClocksChanged)

public:
  explicit AppSettings(QObject* parent = nullptr);
  static AppSettings* instance();
  static AppSettings* create(QQmlEngine*, QJSEngine*);

  bool disclaimerAccepted() const;
  void setDisclaimerAccepted(bool accepted);
  QString backupDir() const;
  QString version() const;

  // Clocks set up with this app: [{name, host, model, ip}]
  QVariantList knownClocks() const;
  // `ip` is the last address the clock answered at (kept if empty).
  Q_INVOKABLE void rememberClock(const QString& name,
                                 const QString& host,
                                 const QString& model,
                                 const QString& ip = QString());
  QVariantMap knownClock(const QString& name) const;
  Q_INVOKABLE void forgetClock(const QString& name);

  // Time zones
  Q_INVOKABLE QStringList zoneIds() const;
  Q_INVOKABLE QString posixRule(const QString& zoneId) const;
  Q_INVOKABLE QString suggestedNtpServer(const QString& zoneId) const;
  Q_INVOKABLE QString systemZoneId() const;

  // Backups in backupDir: [{path, name, size, modified, verified}]
  Q_INVOKABLE QVariantList backups() const;
  Q_INVOKABLE QString localPath(const QUrl& url) const { return url.toLocalFile(); }

Q_SIGNALS:
  void disclaimerAcceptedChanged();
  void knownClocksChanged();

private:
  mutable QStringList zoneCache_;
};
