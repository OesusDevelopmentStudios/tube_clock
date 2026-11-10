// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimeZones.h"

#include <QDir>
#include <QFile>
#include <QTimeZone>

#include "clockcore/Validation.h"

namespace TimeZones {

namespace {

// Zones whose POSIX rule cannot express "no summer time" by dropping the DST part
// (negative DST). They are mapped to a zone with identical wall clock times.
QString
ruleAlias(const QString& zoneId)
{
  if (zoneId == QLatin1String("Europe/Dublin"))
    return QStringLiteral("Europe/London");
  return zoneId;
}

QList<QPair<QString, QString>>
readZoneTab(const QString& dir)
{
  QList<QPair<QString, QString>> out; // (zone, country)
  QFile f(QDir(dir).filePath(QStringLiteral("zone.tab")));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return out;
  while (!f.atEnd()) {
    const QString line = QString::fromUtf8(f.readLine()).trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
      continue;
    const QStringList cols = line.split(QLatin1Char('\t'));
    if (cols.size() >= 3)
      out.append({ cols.at(2), cols.at(0) });
  }
  return out;
}

} // namespace

QString
defaultZoneInfoDir()
{
  const QByteArray env = qgetenv("TZDIR");
  return env.isEmpty() ? QStringLiteral("/usr/share/zoneinfo") : QString::fromLocal8Bit(env);
}

QStringList
zoneIds(const QString& zoneInfoDir)
{
  QStringList ids;
  for (const auto& z : readZoneTab(zoneInfoDir))
    ids.append(z.first);
  if (ids.isEmpty()) {
    // No zone.tab: fall back to Qt's list (includes aliases).
    for (const QByteArray& id : QTimeZone::availableTimeZoneIds()) {
      const QString s = QString::fromLatin1(id);
      if (s.contains(QLatin1Char('/')) && !s.startsWith(QLatin1String("Etc/")))
        ids.append(s);
    }
  }
  ids.sort();
  ids.removeDuplicates();
  ids.prepend(QStringLiteral("Etc/UTC"));
  return ids;
}

QString
posixRuleFromTzif(const QByteArray& data)
{
  if (data.size() < 44 || !data.startsWith("TZif") || data.at(4) < '2')
    return QString();
  // Version 2+ files end with "\n<POSIX rule>\n".
  if (!data.endsWith('\n'))
    return QString();
  const qsizetype start = data.lastIndexOf('\n', data.size() - 2);
  if (start < 0)
    return QString();
  const QByteArray footer = data.mid(start + 1, data.size() - start - 2);
  for (char ch : footer)
    if (static_cast<unsigned char>(ch) < 0x20 || static_cast<unsigned char>(ch) > 0x7e)
      return QString();
  return QString::fromLatin1(footer);
}

QString
posixRule(const QString& zoneId, const QString& zoneInfoDir, QString* error)
{
  if (zoneId == QLatin1String("Etc/UTC") || zoneId == QLatin1String("UTC"))
    return QStringLiteral("UTC0");
  const QString id = ruleAlias(zoneId);
  if (id.contains(QLatin1String("..")) || id.startsWith(QLatin1Char('/'))) {
    if (error)
      *error = QStringLiteral("Invalid time zone name.");
    return QString();
  }
  QFile f(QDir(zoneInfoDir).filePath(id));
  if (!f.open(QIODevice::ReadOnly)) {
    if (error)
      *error = QStringLiteral("Time zone %1 not found in %2.").arg(zoneId, zoneInfoDir);
    return QString();
  }
  const QString rule = posixRuleFromTzif(f.readAll());
  if (rule.isEmpty() || !clockcore::isValidPosixTz(rule.toStdString())) {
    if (error)
      *error = QStringLiteral("Time zone %1 has no usable rule.").arg(zoneId);
    return QString();
  }
  return rule;
}

QString
countryCode(const QString& zoneId, const QString& zoneInfoDir)
{
  for (const auto& z : readZoneTab(zoneInfoDir))
    if (z.first == zoneId)
      return z.second;
  return QString();
}

QString
suggestedNtpServer(const QString& zoneId, const QString& zoneInfoDir)
{
  const QString cc = countryCode(zoneId, zoneInfoDir).toLower();
  // The NTP Pool serves <cc>.pool.ntp.org for almost every country; "uk" is the
  // historical name for GB.
  if (cc.isEmpty())
    return QStringLiteral("pool.ntp.org");
  return (cc == QLatin1String("gb") ? QStringLiteral("uk") : cc) + QStringLiteral(".pool.ntp.org");
}

QString
systemZoneId()
{
  return QString::fromLatin1(QTimeZone::systemTimeZoneId());
}

} // namespace TimeZones
