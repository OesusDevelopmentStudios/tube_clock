// SPDX-License-Identifier: GPL-3.0-or-later
//
// IANA time zones -> POSIX TZ rules for the clock.
// The rule is the footer line of the compiled TZif file (/usr/share/zoneinfo/<zone>),
// which contains the current DST transition rule.
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace TimeZones
{

QString defaultZoneInfoDir();

// Region/City zones from zone.tab, sorted (no legacy aliases, no Etc/ zones).
QStringList zoneIds(const QString& zoneInfoDir = defaultZoneInfoDir());

// POSIX rule for an IANA zone, or an empty string with *error set.
QString posixRule(const QString& zoneId, const QString& zoneInfoDir = defaultZoneInfoDir(), QString* error = nullptr);

// Extracts the POSIX footer of TZif version 2+ data. Empty if absent.
QString posixRuleFromTzif(const QByteArray& data);

// Two-letter country code of a zone from zone.tab (e.g. "CZ"), or empty.
QString countryCode(const QString& zoneId, const QString& zoneInfoDir = defaultZoneInfoDir());

// Regional NTP Pool server for the zone ("cz.pool.ntp.org"), falling back to pool.ntp.org.
QString suggestedNtpServer(const QString& zoneId, const QString& zoneInfoDir = defaultZoneInfoDir());

// Best guess for the computer's own zone (QTimeZone::systemTimeZoneId()).
QString systemZoneId();

} // namespace TimeZones
