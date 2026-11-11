// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "TimeZones.hpp"
#include "clockcore/Validation.hpp"

class TestTimeZones : public QObject
{
    Q_OBJECT
private:
    // Minimal TZif v2 file: header, no data, footer.
    static QByteArray fakeTzif(const QByteArray& footer, char version = '2')
    {
        QByteArray d("TZif");
        d.append(version);
        d.append(QByteArray(15 + 24, '\0'));
        d.append("\n" + footer + "\n");
        return d;
    }

private Q_SLOTS:
    void footerParsing()
    {
        QCOMPARE(TimeZones::posixRuleFromTzif(fakeTzif("CET-1CEST,M3.5.0,M10.5.0/3")),
            QStringLiteral("CET-1CEST,M3.5.0,M10.5.0/3"));
        QCOMPARE(TimeZones::posixRuleFromTzif(fakeTzif("<+0545>-5:45", '3')), QStringLiteral("<+0545>-5:45"));
        QVERIFY(TimeZones::posixRuleFromTzif(fakeTzif("CET-1", '\0')).isEmpty()); // version 1: no footer
        QVERIFY(TimeZones::posixRuleFromTzif("garbage").isEmpty());
        QVERIFY(TimeZones::posixRuleFromTzif(fakeTzif("bad\x01rule")).isEmpty());
    }

    void readsFromDirectory()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("Europe")));
        QFile f(dir.filePath(QStringLiteral("Europe/Prague")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(fakeTzif("CET-1CEST,M3.5.0,M10.5.0/3"));
        f.close();
        QFile tab(dir.filePath(QStringLiteral("zone.tab")));
        QVERIFY(tab.open(QIODevice::WriteOnly));
        tab.write("# comment\nCZ\t+5005+01426\tEurope/Prague\nGB\t+513030-0000731\tEurope/London\n");
        tab.close();

        QString err;
        QCOMPARE(TimeZones::posixRule(QStringLiteral("Europe/Prague"), dir.path(), &err),
            QStringLiteral("CET-1CEST,M3.5.0,M10.5.0/3"));
        QVERIFY(TimeZones::posixRule(QStringLiteral("Europe/Nowhere"), dir.path(), &err).isEmpty());
        QVERIFY(!err.isEmpty());
        QVERIFY(TimeZones::posixRule(QStringLiteral("../etc/passwd"), dir.path(), &err).isEmpty());
        QCOMPARE(TimeZones::posixRule(QStringLiteral("Etc/UTC"), dir.path()), QStringLiteral("UTC0"));

        const QStringList ids = TimeZones::zoneIds(dir.path());
        QCOMPARE(ids,
            QStringList(
                { QStringLiteral("Etc/UTC"), QStringLiteral("Europe/London"), QStringLiteral("Europe/Prague") }));
        QCOMPARE(TimeZones::countryCode(QStringLiteral("Europe/Prague"), dir.path()), QStringLiteral("CZ"));
        QCOMPARE(TimeZones::suggestedNtpServer(QStringLiteral("Europe/Prague"), dir.path()),
            QStringLiteral("cz.pool.ntp.org"));
        QCOMPARE(TimeZones::suggestedNtpServer(QStringLiteral("Europe/London"), dir.path()),
            QStringLiteral("uk.pool.ntp.org"));
        QCOMPARE(TimeZones::suggestedNtpServer(QStringLiteral("Etc/UTC"), dir.path()), QStringLiteral("pool.ntp.org"));
    }

    void systemDatabase()
    {
        const QString dir = TimeZones::defaultZoneInfoDir();
        if (!QFile::exists(QDir(dir).filePath(QStringLiteral("zone.tab"))))
        {
            QSKIP("no system tz database");
        }

        const QStringList ids = TimeZones::zoneIds(dir);
        QVERIFY(ids.size() > 300);
        // Every zone must produce a rule the firmware accepts, with and without DST.
        int checked = 0;
        for (const QString& id : ids)
        {
            QString err;
            const QString rule = TimeZones::posixRule(id, dir, &err);
            QVERIFY2(!rule.isEmpty(), qPrintable(id + QStringLiteral(": ") + err));
            QVERIFY2(clockcore::isValidPosixTz(rule.toStdString()), qPrintable(id));
            QVERIFY2(!clockcore::stripDst(rule.toStdString()).empty(), qPrintable(id));
            ++checked;
        }

        QVERIFY(checked > 300);
        // Dublin's negative DST is mapped to London's equivalent rule.
        QCOMPARE(TimeZones::posixRule(QStringLiteral("Europe/Dublin"), dir),
            TimeZones::posixRule(QStringLiteral("Europe/London"), dir));
    }
};

QTEST_GUILESS_MAIN(TestTimeZones)
#include "test_timezones.moc"
