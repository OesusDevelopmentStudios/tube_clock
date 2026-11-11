// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockConfig.hpp"

#include <QRandomGenerator>

#include "clockcore/ImageSet.hpp"
#include "clockcore/Validation.hpp"

namespace
{
bool validIp(const QString& s)
{
    return clockcore::isValidIPv4(s.toStdString());
}
} // namespace

QStringList ClockConfig::patterns()
{
    return { QStringLiteral("off"),
        QStringLiteral("constant"),
        QStringLiteral("rainbow"),
        QStringLiteral("pulse"),
        QStringLiteral("breath") };
}

QString ClockConfig::wifiModeName(WifiMode mode)
{
    switch (mode)
    {
        case WifiMode::Dhcp:
            return QStringLiteral("dhcp");
        case WifiMode::Static:
            return QStringLiteral("static");
        case WifiMode::Off:
            break;
    }

    return QStringLiteral("off");
}

QJsonObject ClockConfig::toJson(bool includeSecrets) const
{
    QJsonObject wifi{ { QStringLiteral("mode"), wifiModeName(wifiMode) },
        { QStringLiteral("ssid"), ssid },
        { QStringLiteral("ip"), ip },
        { QStringLiteral("gateway"), gateway },
        { QStringLiteral("netmask"), netmask },
        { QStringLiteral("dns"), dns } };
    QJsonObject api{ { QStringLiteral("enabled"), apiEnabled } };
    if (includeSecrets)
    {
        wifi.insert(QStringLiteral("password"), password);
        api.insert(QStringLiteral("token"), token);
    }

    return QJsonObject{
        { QStringLiteral("version"), version },
        { QStringLiteral("name"), name },
        { QStringLiteral("wifi"), wifi },
        { QStringLiteral("api"), api },
        { QStringLiteral("time"),
            QJsonObject{ { QStringLiteral("tz_name"), tzName },
                { QStringLiteral("tz_posix"), tzPosix },
                { QStringLiteral("dst"), dst },
                { QStringLiteral("ntp_server"), ntpServer },
                { QStringLiteral("ntp_enabled"), ntpEnabled },
                { QStringLiteral("twelve_hour"), twelveHour } } },
        { QStringLiteral("display"),
            QJsonObject{
                { QStringLiteral("image_set"), imageSet }, { QStringLiteral("lcd_brightness"), lcdBrightness } } },
        { QStringLiteral("backlight"),
            QJsonObject{ { QStringLiteral("pattern"), pattern },
                { QStringLiteral("color_phase"), colorPhase },
                { QStringLiteral("intensity"), intensity } } },
    };
}

ClockConfig ClockConfig::fromJson(const QJsonObject& o, QStringList* errors)
{
    ClockConfig c;
    auto str = [](const QJsonObject& obj, const char* key, QString& dst) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isString())
        {
            dst = v.toString();
        }
    };
    auto boolean = [](const QJsonObject& obj, const char* key, bool& dst) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isBool())
        {
            dst = v.toBool();
        }
    };
    auto integer = [](const QJsonObject& obj, const char* key, int& dst) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isDouble())
        {
            dst = v.toInt();
        }
    };

    integer(o, "version", c.version);
    str(o, "name", c.name);

    const QJsonObject wifi = o.value(QStringLiteral("wifi")).toObject();
    const QString mode = wifi.value(QStringLiteral("mode")).toString(QStringLiteral("off"));
    if (mode == QLatin1String("dhcp"))
    {
        c.wifiMode = WifiMode::Dhcp;
    }
    else if (mode == QLatin1String("static"))
    {
        c.wifiMode = WifiMode::Static;
    }
    else if (mode != QLatin1String("off") && errors)
    {
        errors->append(QStringLiteral("unknown wifi.mode '%1'").arg(mode));
    }

    str(wifi, "ssid", c.ssid);
    str(wifi, "password", c.password);
    str(wifi, "ip", c.ip);
    str(wifi, "gateway", c.gateway);
    str(wifi, "netmask", c.netmask);
    str(wifi, "dns", c.dns);

    const QJsonObject api = o.value(QStringLiteral("api")).toObject();
    boolean(api, "enabled", c.apiEnabled);
    str(api, "token", c.token);

    const QJsonObject time = o.value(QStringLiteral("time")).toObject();
    str(time, "tz_name", c.tzName);
    str(time, "tz_posix", c.tzPosix);
    boolean(time, "dst", c.dst);
    str(time, "ntp_server", c.ntpServer);
    boolean(time, "ntp_enabled", c.ntpEnabled);
    boolean(time, "twelve_hour", c.twelveHour);

    const QJsonObject display = o.value(QStringLiteral("display")).toObject();
    str(display, "image_set", c.imageSet);
    integer(display, "lcd_brightness", c.lcdBrightness);

    const QJsonObject bl = o.value(QStringLiteral("backlight")).toObject();
    str(bl, "pattern", c.pattern);
    integer(bl, "color_phase", c.colorPhase);
    integer(bl, "intensity", c.intensity);
    if (!patterns().contains(c.pattern) && errors)
    {
        errors->append(QStringLiteral("unknown backlight.pattern '%1'").arg(c.pattern));
    }

    return c;
}

QStringList ClockConfig::validate() const
{
    // Mirrors clockcore::validateConfig() so the clock never rejects what the app writes.
    QStringList p;
    if (version != 1)
    {
        p << QStringLiteral("Unsupported configuration version.");
    }

    if (!name.isEmpty() && !clockcore::isValidHostName(name.toStdString()))
    {
        p << QStringLiteral("The clock name may only contain letters, digits and '-'.");
    }

    if (wifiMode != WifiMode::Off)
    {
        if (ssid.isEmpty() || ssid.toUtf8().size() > 32)
        {
            p << QStringLiteral("The WiFi network name (SSID) must be 1-32 characters.");
        }

        const auto pwLen = password.toUtf8().size();
        if (!password.isEmpty() && (pwLen < 8 || pwLen > 63))
        {
            p << QStringLiteral("The WiFi password must be 8-63 characters (or empty for an open network).");
        }
    }

    if (wifiMode == WifiMode::Static)
    {
        if (!validIp(ip))
        {
            p << QStringLiteral("The IP address is not valid.");
        }

        if (!validIp(gateway))
        {
            p << QStringLiteral("The gateway address is not valid.");
        }

        if (!validIp(netmask))
        {
            p << QStringLiteral("The subnet mask is not valid.");
        }

        if (!dns.isEmpty() && !validIp(dns))
        {
            p << QStringLiteral("The DNS server address is not valid.");
        }
    }

    if (apiEnabled && !clockcore::isValidToken(token.toStdString()))
    {
        p << QStringLiteral("Remote access needs a pairing token.");
    }

    if (!clockcore::isValidPosixTz(tzPosix.toStdString()))
    {
        p << QStringLiteral("The time zone rule is not valid.");
    }

    if (tzName.isEmpty() || tzName.size() > 48)
    {
        p << QStringLiteral("The time zone name is not valid.");
    }

    if (ntpEnabled && !clockcore::isValidHostName(ntpServer.toStdString()) && !validIp(ntpServer))
    {
        p << QStringLiteral("The time server must be a host name or an IPv4 address.");
    }

    if (!imageSet.isEmpty() && !clockcore::isValidSetName(imageSet.toStdString()))
    {
        p << QStringLiteral("The image set name is not valid.");
    }

    if (lcdBrightness < 1 || lcdBrightness > 10)
    {
        p << QStringLiteral("LCD brightness must be 1-10.");
    }

    if (intensity < 0 || intensity > 7)
    {
        p << QStringLiteral("Pattern brightness must be 0-7.");
    }

    if (colorPhase < 0 || colorPhase > 767)
    {
        p << QStringLiteral("Pattern colour must be 0-767.");
    }

    if (!patterns().contains(pattern))
    {
        p << QStringLiteral("Unknown backlight pattern.");
    }

    return p;
}

QString ClockConfig::generateToken()
{
    QString t;
    auto* rng = QRandomGenerator::system();
    for (int i = 0; i < 4; ++i)
    {
        t += QStringLiteral("%1").arg(rng->generate(), 8, 16, QLatin1Char('0'));
    }

    return t;
}
