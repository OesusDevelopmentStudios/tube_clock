// SPDX-License-Identifier: GPL-3.0-or-later
#include "ConfigForm.hpp"

#include <QColor>

#include "TimeZones.hpp"

void ConfigForm::setWifiMode(const QString& m)
{
    ClockConfig::WifiMode mode = ClockConfig::WifiMode::Off;
    if (m == QLatin1String("dhcp"))
    {
        mode = ClockConfig::WifiMode::Dhcp;
    }
    else if (m == QLatin1String("static"))
    {
        mode = ClockConfig::WifiMode::Static;
    }

    if (mode == c_.wifiMode)
    {
        return;
    }

    c_.wifiMode = mode;
    Q_EMIT changed();
}

bool ConfigForm::setZone(const QString& zoneId)
{
    const QString rule = TimeZones::posixRule(zoneId);
    if (rule.isEmpty())
    {
        return false;
    }

    c_.tzName = zoneId;
    c_.tzPosix = rule;
    Q_EMIT changed();
    return true;
}

QColor ConfigForm::phaseColor(int phase) const
{
    auto intensity = [](int p) { return p <= 255 ? p : (p <= 511 ? 511 - p : 0); };
    const int p = ((phase % 768) + 768) % 768;
    return QColor(intensity(p), intensity((p + 256) % 768), intensity((p + 512) % 768));
}
