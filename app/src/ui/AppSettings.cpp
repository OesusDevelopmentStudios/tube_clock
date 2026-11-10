// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppSettings.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include "TimeZones.h"

namespace {
AppSettings* gInstance = nullptr;
}

AppSettings::AppSettings(QObject* parent)
  : QObject(parent)
{
  gInstance = this;
}

AppSettings*
AppSettings::instance()
{
  if (gInstance == nullptr)
    gInstance = new AppSettings(QCoreApplication::instance());
  return gInstance;
}

AppSettings*
AppSettings::create(QQmlEngine*, QJSEngine*)
{
  AppSettings* s = instance();
  QJSEngine::setObjectOwnership(s, QJSEngine::CppOwnership);
  return s;
}

bool
AppSettings::disclaimerAccepted() const
{
  return QSettings().value(QStringLiteral("disclaimerAccepted"), false).toBool();
}

void
AppSettings::setDisclaimerAccepted(bool accepted)
{
  if (accepted == disclaimerAccepted())
    return;
  QSettings().setValue(QStringLiteral("disclaimerAccepted"), accepted);
  Q_EMIT disclaimerAcceptedChanged();
}

QString
AppSettings::backupDir() const
{
  const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const QString dir = QDir(docs).filePath(QStringLiteral("TubeClock/backups"));
  QDir().mkpath(dir);
  return dir;
}

QString
AppSettings::version() const
{
  return QCoreApplication::applicationVersion();
}

namespace {
const QStringList kClockKeys = { QStringLiteral("name"),
                                 QStringLiteral("host"),
                                 QStringLiteral("model"),
                                 QStringLiteral("ip") };

void
writeClocks(const QVariantList& list)
{
  QSettings s;
  s.remove(QStringLiteral("clocks"));
  s.beginWriteArray(QStringLiteral("clocks"), int(list.size()));
  for (int i = 0; i < list.size(); ++i) {
    s.setArrayIndex(i);
    const QVariantMap m = list.at(i).toMap();
    for (const QString& key : kClockKeys)
      s.setValue(key, m.value(key));
  }
  s.endArray();
}
} // namespace

QVariantList
AppSettings::knownClocks() const
{
  QVariantList out;
  QSettings s;
  const int n = s.beginReadArray(QStringLiteral("clocks"));
  for (int i = 0; i < n; ++i) {
    s.setArrayIndex(i);
    QVariantMap m;
    for (const QString& key : kClockKeys)
      m.insert(key, s.value(key).toString());
    out.append(m);
  }
  s.endArray();
  return out;
}

QVariantMap
AppSettings::knownClock(const QString& name) const
{
  for (const QVariant& v : knownClocks())
    if (v.toMap().value(QStringLiteral("name")) == name)
      return v.toMap();
  return {};
}

void
AppSettings::rememberClock(const QString& name, const QString& host, const QString& model, const QString& ip)
{
  QVariantList list = knownClocks();
  QString lastIp = ip;
  for (int i = int(list.size()) - 1; i >= 0; --i) {
    const QVariantMap m = list.at(i).toMap();
    if (m.value(QStringLiteral("name")) == name) {
      if (lastIp.isEmpty())
        lastIp = m.value(QStringLiteral("ip")).toString(); // keep the last known address
      list.removeAt(i);
    }
  }
  list.append(QVariantMap{ { QStringLiteral("name"), name },
                           { QStringLiteral("host"), host },
                           { QStringLiteral("model"), model },
                           { QStringLiteral("ip"), lastIp } });
  writeClocks(list);
  Q_EMIT knownClocksChanged();
}

void
AppSettings::forgetClock(const QString& name)
{
  QVariantList list = knownClocks();
  for (int i = int(list.size()) - 1; i >= 0; --i)
    if (list.at(i).toMap().value(QStringLiteral("name")) == name)
      list.removeAt(i);
  writeClocks(list);
  Q_EMIT knownClocksChanged();
}

QStringList
AppSettings::zoneIds() const
{
  if (zoneCache_.isEmpty())
    zoneCache_ = TimeZones::zoneIds();
  return zoneCache_;
}

QString
AppSettings::posixRule(const QString& zoneId) const
{
  return TimeZones::posixRule(zoneId);
}

QString
AppSettings::suggestedNtpServer(const QString& zoneId) const
{
  return TimeZones::suggestedNtpServer(zoneId);
}

QString
AppSettings::systemZoneId() const
{
  return TimeZones::systemZoneId();
}

QVariantList
AppSettings::backups() const
{
  QVariantList out;
  const QDir dir(backupDir());
  for (const QFileInfo& fi : dir.entryInfoList({ QStringLiteral("*.bin") }, QDir::Files, QDir::Time)) {
    bool verified = false;
    QFile sumFile(fi.absoluteFilePath() + QStringLiteral(".sha256"));
    if (sumFile.open(QIODevice::ReadOnly)) {
      const QByteArray expected = sumFile.readAll().left(64);
      QFile bin(fi.absoluteFilePath());
      if (bin.open(QIODevice::ReadOnly))
        verified = QCryptographicHash::hash(bin.readAll(), QCryptographicHash::Sha256).toHex() == expected;
    }
    out.append(QVariantMap{ { QStringLiteral("path"), fi.absoluteFilePath() },
                            { QStringLiteral("name"), fi.fileName() },
                            { QStringLiteral("size"), fi.size() },
                            { QStringLiteral("modified"), fi.lastModified() },
                            { QStringLiteral("verified"), verified } });
  }
  return out;
}
