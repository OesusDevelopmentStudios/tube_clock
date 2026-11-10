// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Step 2: WiFi and remote access.
Kirigami.ScrollablePage {
    id: page
    required property FlashController controller
    readonly property ConfigForm cfg: controller.config

    title: i18n("Set Up a Clock: Network")

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
            onTriggered: applicationWindow().pageStack.push(Qt.resolvedUrl("SetupTimePage.qml"), { controller: page.controller })
        }
    ]

    Kirigami.FormLayout {
        width: parent.width

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Clock name:")
            text: cfg.clockName
            placeholderText: "tubeclock-a1b2c3"
            onTextEdited: cfg.clockName = text.toLowerCase()
        }

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("WiFi")
        }
        QQC2.RadioButton {
            Kirigami.FormData.label: i18n("Connection:")
            text: i18n("Off (no network, set the time on the clock)")
            checked: cfg.wifiMode === "off"
            onToggled: cfg.wifiMode = "off"
        }
        QQC2.RadioButton {
            text: i18n("Automatic address (DHCP)")
            checked: cfg.wifiMode === "dhcp"
            onToggled: cfg.wifiMode = "dhcp"
        }
        QQC2.RadioButton {
            text: i18n("Static IP address")
            checked: cfg.wifiMode === "static"
            onToggled: cfg.wifiMode = "static"
        }
        QQC2.TextField {
            Kirigami.FormData.label: i18n("Network name (SSID):")
            visible: cfg.wifiMode !== "off"
            text: cfg.ssid
            onTextEdited: cfg.ssid = text
        }
        Kirigami.PasswordField {
            Kirigami.FormData.label: i18n("Password:")
            visible: cfg.wifiMode !== "off"
            text: cfg.password
            onTextEdited: cfg.password = text
        }
        QQC2.Label {
            visible: cfg.wifiMode !== "off"
            opacity: 0.7
            text: i18n("The clock supports 2.4 GHz networks only.")
        }
        QQC2.TextField {
            Kirigami.FormData.label: i18n("IP address:")
            visible: cfg.wifiMode === "static"
            text: cfg.ip
            placeholderText: "192.168.1.50"
            onTextEdited: cfg.ip = text
        }
        QQC2.TextField {
            Kirigami.FormData.label: i18n("Subnet mask:")
            visible: cfg.wifiMode === "static"
            text: cfg.netmask
            onTextEdited: cfg.netmask = text
        }
        QQC2.TextField {
            Kirigami.FormData.label: i18n("Gateway:")
            visible: cfg.wifiMode === "static"
            text: cfg.gateway
            placeholderText: "192.168.1.1"
            onTextEdited: cfg.gateway = text
        }
        QQC2.TextField {
            Kirigami.FormData.label: i18n("DNS server:")
            visible: cfg.wifiMode === "static"
            text: cfg.dns
            placeholderText: i18n("Same as gateway")
            onTextEdited: cfg.dns = text
        }

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Remote Access")
        }
        QQC2.Switch {
            Kirigami.FormData.label: i18n("Control from this app:")
            enabled: cfg.wifiMode !== "off"
            checked: cfg.remoteAccess && cfg.wifiMode !== "off"
            onToggled: cfg.remoteAccess = checked
        }
        QQC2.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.Wrap
            opacity: 0.7
            text: i18n("Lets this computer change settings, upload image sets and set the time over WiFi. "
                       + "Access is protected by a random pairing key that is stored in your system wallet. "
                       + "It can be switched off later in the app or in the clock's menu.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: cfg.problems.length > 0
            text: cfg.problems.join("\n")
        }
    }
}
