// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QtTest>

#include "ClockConfig.hpp"

class TestClockConfig : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsAreValid()
    {
        ClockConfig c;
        QVERIFY2(c.validate().isEmpty(), qPrintable(c.validate().join(QLatin1Char('\n'))));
        QVERIFY(!c.apiEnabled);
    }

    void readsSharedFixture()
    {
        // The same file is parsed by the firmware tests (test/test_config).
        QFile f(QStringLiteral(TEST_FIXTURES "/config_full.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QStringList errors;
        const ClockConfig c = ClockConfig::fromJson(QJsonDocument::fromJson(f.readAll()).object(), &errors);
        QVERIFY(errors.isEmpty());
        QVERIFY(c.validate().isEmpty());
        QCOMPARE(c.name, QStringLiteral("tubeclock-test"));
        QCOMPARE(c.wifiMode, ClockConfig::WifiMode::Static);
        QCOMPARE(c.ip, QStringLiteral("192.168.1.50"));
        QCOMPARE(c.password, QStringLiteral("correct horse"));
        QCOMPARE(c.token, QStringLiteral("00112233445566778899aabbccddeeff"));
        QCOMPARE(c.tzPosix, QStringLiteral("CET-1CEST,M3.5.0,M10.5.0/3"));
        QCOMPARE(c.ntpServer, QStringLiteral("cz.pool.ntp.org"));
        QCOMPARE(c.lcdBrightness, 8);
        QCOMPARE(c.pattern, QStringLiteral("pulse"));
        QCOMPARE(c.colorPhase, 300);
        QCOMPARE(c.intensity, 5);
    }

    void roundTrip()
    {
        QFile f(QStringLiteral(TEST_FIXTURES "/config_full.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject original = QJsonDocument::fromJson(f.readAll()).object();
        const ClockConfig c = ClockConfig::fromJson(original);
        QCOMPARE(c.toJson(true), original);
    }

    void secretsOmitted()
    {
        ClockConfig c;
        c.password = QStringLiteral("secret-password");
        c.token = ClockConfig::generateToken();
        const QByteArray json = QJsonDocument(c.toJson(false)).toJson();
        QVERIFY(!json.contains("secret-password"));
        QVERIFY(!json.contains(c.token.toUtf8()));
    }

    void validation_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("value");
        QTest::newRow("bad ip") << "ip" << "192.168.1.300";
        QTest::newRow("bad gateway") << "gateway" << "gw";
        QTest::newRow("short password") << "password" << "short";
        QTest::newRow("empty ssid") << "ssid" << "";
        QTest::newRow("bad name") << "name" << "my clock";
        QTest::newRow("bad tz") << "tzPosix" << "Europe/Prague";
        QTest::newRow("bad ntp") << "ntpServer" << "pool ntp org";
        QTest::newRow("bad set") << "imageSet" << "Nixie Tubes";
    }

    void validation()
    {
        QFETCH(QString, field);
        QFETCH(QString, value);
        ClockConfig c;
        c.wifiMode = ClockConfig::WifiMode::Static;
        c.ssid = QStringLiteral("home");
        c.ip = QStringLiteral("192.168.1.50");
        c.gateway = QStringLiteral("192.168.1.1");
        QVERIFY(c.validate().isEmpty());
        if (field == QLatin1String("ip"))
        {
            c.ip = value;
        }
        else if (field == QLatin1String("gateway"))
        {
            c.gateway = value;
        }
        else if (field == QLatin1String("password"))
        {
            c.password = value;
        }
        else if (field == QLatin1String("ssid"))
        {
            c.ssid = value;
        }
        else if (field == QLatin1String("name"))
        {
            c.name = value;
        }
        else if (field == QLatin1String("tzPosix"))
        {
            c.tzPosix = value;
        }
        else if (field == QLatin1String("ntpServer"))
        {
            c.ntpServer = value;
        }
        else if (field == QLatin1String("imageSet"))
        {
            c.imageSet = value;
        }

        QCOMPARE(c.validate().size(), 1);
    }

    void apiNeedsToken()
    {
        ClockConfig c;
        c.apiEnabled = true;
        QCOMPARE(c.validate().size(), 1);
        c.token = ClockConfig::generateToken();
        QVERIFY(c.validate().isEmpty());
    }

    void tokensAreRandomHex()
    {
        const QString a = ClockConfig::generateToken();
        const QString b = ClockConfig::generateToken();
        QCOMPARE(a.size(), 32);
        QVERIFY(a != b);
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{32}$")).match(a).hasMatch());
    }
};

QTEST_GUILESS_MAIN(TestClockConfig)
#include "test_clockconfig.moc"
