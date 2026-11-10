// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.coreaddons as KCoreAddons
import TubeClock

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Tube Clock")
    minimumWidth: Kirigami.Units.gridUnit * 24
    minimumHeight: Kirigami.Units.gridUnit * 28
    width: Kirigami.Units.gridUnit * 46
    height: Kirigami.Units.gridUnit * 36

    // One page at a time: "Next" replaces the view instead of showing pages side by side.
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn
    pageStack.globalToolBar.showNavigationButtons: Kirigami.ApplicationHeaderStyle.ShowBackButton

    // One controller for the whole setup wizard, so going back keeps the entries.
    FlashController {
        id: flashController
    }

    function showClocks() {
        pageStack.clear()
        pageStack.push(Qt.resolvedUrl("ClocksPage.qml"))
    }

    function startSetup() {
        pageStack.clear()
        pageStack.push(Qt.resolvedUrl("SetupConnectPage.qml"), { controller: flashController })
    }

    function showBackups() {
        pageStack.clear()
        pageStack.push(Qt.resolvedUrl("BackupsPage.qml"), { controller: flashController })
    }

    globalDrawer: Kirigami.GlobalDrawer {
        modal: !root.wideScreen
        collapsible: true
        collapsed: !root.wideScreen
        enabled: AppSettings.disclaimerAccepted
        actions: [
            Kirigami.Action {
                text: i18n("Clocks")
                icon.name: "clock"
                onTriggered: root.showClocks()
            },
            Kirigami.Action {
                text: i18n("Set Up a Clock…")
                icon.name: "run-build-install"
                onTriggered: root.startSetup()
            },
            Kirigami.Action {
                text: i18n("Backups")
                icon.name: "document-save-all"
                onTriggered: root.showBackups()
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: i18n("About Tube Clock")
                icon.name: "help-about"
                onTriggered: {
                    pageStack.clear()
                    pageStack.push(aboutPage)
                }
            }
        ]
    }

    Component {
        id: aboutPage
        Kirigami.AboutPage {
            aboutData: KCoreAddons.AboutData
        }
    }

    // --smoke-test: visit every page once, then quit.
    Timer {
        id: smokeTimer
        property int step: 0
        interval: 300
        repeat: true
        running: typeof smokeTest !== "undefined" && smokeTest
        onTriggered: {
            const fc = flashController
            switch (step++) {
            case 0: AppSettings.disclaimerAccepted = true; break
            case 1: root.startSetup(); break
            case 2: pageStack.push(Qt.resolvedUrl("SetupNetworkPage.qml"), { controller: fc }); break
            case 3: fc.config.wifiMode = "static"; break
            case 4: pageStack.push(Qt.resolvedUrl("SetupTimePage.qml"), { controller: fc }); break
            case 5: pageStack.push(Qt.resolvedUrl("SetupImagesPage.qml"), { controller: fc }); break
            case 6: pageStack.push(Qt.resolvedUrl("SetupFlashPage.qml"), { controller: fc }); break
            case 7: root.showBackups(); break
            case 8: pageStack.clear(); pageStack.push(aboutPage); break
            case 9: root.showClocks(); break
            case 10: pageStack.push(Qt.resolvedUrl("DevicePage.qml"), { clockName: "smoke", clockHost: "127.0.0.1", clockPort: 9 }); break
            default: Qt.quit()
            }
        }
    }

    Component.onCompleted: {
        if (AppSettings.disclaimerAccepted)
            showClocks()
        else
            pageStack.push(Qt.resolvedUrl("DisclaimerPage.qml"))
    }

    Connections {
        target: AppSettings
        function onDisclaimerAcceptedChanged() {
            if (AppSettings.disclaimerAccepted)
                root.showClocks()
        }
    }
}
