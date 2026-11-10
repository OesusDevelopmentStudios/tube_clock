// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Step 3: time zone, summer time, time server, display options.
Kirigami.ScrollablePage {
    id: page
    required property FlashController controller
    readonly property ConfigForm cfg: controller.config

    title: i18n("Set Up a Clock: Time")

    actions: [
        Kirigami.Action {
            text: i18n("Back")
            icon.name: "go-previous"
            enabled: !page.controller.busy
            onTriggered: applicationWindow().pageStack.pop()
        },
        Kirigami.Action {
            text: i18n("Next")
            icon.name: "go-next"
            onTriggered: applicationWindow().pageStack.push(Qt.resolvedUrl("SetupImagesPage.qml"), { controller: page.controller })
        }
    ]

    Kirigami.FormLayout {
        width: parent.width

        TimeZoneField {
            Kirigami.FormData.label: i18n("Time zone:")
            config: cfg
            onZoneChosen: zone => cfg.ntpServer = AppSettings.suggestedNtpServer(zone)
        }
        QQC2.Switch {
            Kirigami.FormData.label: i18n("Summer time:")
            text: i18n("Switch to summer time (DST) automatically")
            checked: cfg.dst
            onToggled: cfg.dst = checked
        }
        QQC2.Switch {
            Kirigami.FormData.label: i18n("Format:")
            text: i18n("12-hour clock")
            checked: cfg.twelveHour
            onToggled: cfg.twelveHour = checked
        }

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Network Time")
        }
        QQC2.Switch {
            Kirigami.FormData.label: i18n("Synchronize:")
            text: i18n("Get the time from the internet once a day")
            enabled: cfg.wifiMode !== "off"
            checked: cfg.ntpEnabled
            onToggled: cfg.ntpEnabled = checked
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Time server:")
            enabled: cfg.ntpEnabled && cfg.wifiMode !== "off"
            QQC2.TextField {
                text: cfg.ntpServer
                onTextEdited: cfg.ntpServer = text.trim()
                Layout.minimumWidth: Kirigami.Units.gridUnit * 12
            }
            QQC2.Button {
                text: i18n("Default")
                onClicked: cfg.ntpServer = AppSettings.suggestedNtpServer(cfg.tzName)
            }
        }
        QQC2.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.Wrap
            opacity: 0.7
            text: i18n("The default is the free NTP Pool (pool.ntp.org) server group for your region. "
                       + "Without WiFi the clock keeps time with its battery backed clock chip.")
        }

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Display")
        }
        RowLayout {
            Kirigami.FormData.label: i18n("Tube brightness:")
            QQC2.Slider {
                from: 1
                to: 10
                stepSize: 1
                value: cfg.lcdBrightness
                onMoved: cfg.lcdBrightness = value
            }
            QQC2.Label {
                text: cfg.lcdBrightness
            }
        }
        BacklightFields {
            Kirigami.FormData.label: i18n("Backlight:")
            config: cfg
        }
    }
}
