// SPDX-License-Identifier: GPL-3.0-or-later
#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>

#include "HostResolver.hpp"
#include "ImageSetModel.hpp"
#include "SerialDiscovery.hpp"

namespace
{
// --smoke-test: open every page once and fail on any QML warning (used by ctest).
int gQmlProblems = 0;
QtMessageHandler gDefaultHandler = nullptr;
void smokeHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    // Only problems in our own QML count. Kirigami's PageRow creates pages before it
    // reparents them, which Qt reports as "not placed in the graphics scene" - harmless.
    if (type != QtDebugMsg && type != QtInfoMsg && msg.contains(QLatin1String("qrc:/qt/qml/TubeClock/"))
        && !msg.contains(QLatin1String("was not placed in the graphics scene")))
    {
        ++gQmlProblems;
    }

    gDefaultHandler(type, ctx, msg);
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption smokeOption(
        QStringLiteral("smoke-test"), QStringLiteral("Open every page once and exit (for automated tests)."));
    parser.addOption(smokeOption);
    const QCommandLineOption listPortsOption(
        QStringLiteral("list-ports"), QStringLiteral("Print the serial ports the app can see and exit."));
    parser.addOption(listPortsOption);
    const QCommandLineOption resolveOption(QStringLiteral("resolve"),
        QStringLiteral("Resolve a clock's host name (e.g. name.local) and exit."),
        QStringLiteral("host"));
    parser.addOption(resolveOption);
    parser.process(app);
    if (parser.isSet(listPortsOption))
    {
        for (const SerialDiscovery::Port& p : SerialDiscovery::scan())
        {
            printf("%s  %04x:%04x  %s%s\n",
                qPrintable(p.path),
                p.vendorId,
                p.productId,
                qPrintable(p.description),
                p.likelyClock ? "  (clock USB bridge)" : "");
        }

        return 0;
    }

    if (parser.isSet(resolveOption))
    {
        const QString host = parser.value(resolveOption);
        HostResolver::resolve(host, [host](const QString& address) {
            printf("%s -> %s\n", qPrintable(host), address.isEmpty() ? "not found" : qPrintable(address));
            QCoreApplication::exit(address.isEmpty() ? 1 : 0);
        });
        return app.exec();
    }

    const bool smoke = parser.isSet(smokeOption);
    if (smoke)
    {
        QStandardPaths::setTestModeEnabled(true); // keep the user's settings untouched
        gDefaultHandler = qInstallMessageHandler(smokeHandler);
    }

    KLocalizedString::setApplicationDomain("tubeclock");
    QApplication::setOrganizationName(QStringLiteral("TubeClock"));
    QApplication::setDesktopFileName(QStringLiteral(TUBECLOCK_APP_ID));
    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral(TUBECLOCK_APP_ID), QIcon::fromTheme(QStringLiteral("clock"))));
    // Breeze look on KDE; fall back to the Fusion style elsewhere.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
    {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    KAboutData about(QStringLiteral("tubeclock"),
        i18n("Tube Clock"),
        QStringLiteral(TUBECLOCK_VERSION),
        i18n("Set up, back up and control EleksTube style IPS tube clocks"),
        KAboutLicense::GPL_V3,
        i18n("© Tube Clock contributors\n"
             "Firmware based on EleksTubeHAX © Mark Smith, Aljaz Ogrin, Martinius79 and contributors"));
    about.setHomepage(QStringLiteral("https://github.com/OesusDevelopmentStudios/tube_clock"));
    about.setBugAddress("https://github.com/OesusDevelopmentStudios/tube_clock/issues");
    about.setDesktopFileName(QStringLiteral(TUBECLOCK_APP_ID));
    about.addCredit(QStringLiteral("Mark Smith (SmittyHalibut)"),
        i18n("Original EleksTubeHAX firmware"),
        QString(),
        QStringLiteral("https://github.com/SmittyHalibut/EleksTubeHAX"));
    about.addCredit(QStringLiteral("Aljaz Ogrin (aly-fly)"),
        i18n("EleksTubeHAX maintainer"),
        QString(),
        QStringLiteral("https://github.com/aly-fly/EleksTubeHAX"));
    about.addCredit(QStringLiteral("Martinius79"), i18n("EleksTubeHAX hardware support and maintenance"));
    about.addCredit(i18n("EleksTubeHAX contributors"), i18n("Clock faces, hardware research and many improvements"));
    about.addComponent(QStringLiteral("esptool"),
        i18n("Talks to the clock's ESP32"),
        QString(),
        QStringLiteral("https://github.com/espressif/esptool"),
        KAboutLicense::GPL_V2);
    about.addComponent(QStringLiteral("littlefs"),
        i18n("Clock filesystem"),
        QStringLiteral("2.9.3"),
        QStringLiteral("https://github.com/littlefs-project/littlefs"),
        KAboutLicense::BSD_3_Clause);
    KAboutData::setApplicationData(about);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("smokeTest"), smoke);
    engine.addImageProvider(QStringLiteral("tubeclock"), new SetPreviewProvider);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("TubeClock", "Main");
    const int rc = app.exec();
    if (smoke && gQmlProblems > 0)
    {
        fprintf(stderr, "smoke test: %d QML problem(s)\n", gQmlProblems);
        return 1;
    }

    return rc;
}
