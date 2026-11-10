// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

Kirigami.ScrollablePage {
    id: page
    title: i18n("Clocks")

    actions: [
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: clocks.refresh()
        },
        Kirigami.Action {
            text: i18n("Add by Address…")
            icon.name: "list-add"
            onTriggered: addDialog.open()
        },
        Kirigami.Action {
            text: i18n("Set Up a Clock…")
            icon.name: "run-build-install"
            onTriggered: applicationWindow().startSetup()
        }
    ]

    ClockListModel {
        id: clocks
    }

    function openClock(name, host, port) {
        applicationWindow().pageStack.push(Qt.resolvedUrl("DevicePage.qml"), { clockName: name, clockHost: host, clockPort: port })
    }

    ListView {
        id: list
        model: clocks
        delegate: QQC2.ItemDelegate {
            id: clockDelegate
            required property string name
            required property string host
            required property int port
            required property string model
            required property string firmware
            required property bool online
            required property string address
            required property bool checking
            width: ListView.view.width
            onClicked: page.openClock(clockDelegate.name, clockDelegate.host, clockDelegate.port)
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon {
                    source: "clock"
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                    opacity: clockDelegate.online ? 1 : 0.5
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    QQC2.Label {
                        text: clockDelegate.name
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        text: [clockDelegate.model,
                               clockDelegate.firmware ? i18n("firmware %1", clockDelegate.firmware) : ""]
                              .filter(s => s).join(" · ")
                        visible: text !== ""
                        opacity: 0.7
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
                QQC2.BusyIndicator {
                    visible: clockDelegate.checking
                    running: visible
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                }
                QQC2.Label {
                    text: clockDelegate.checking ? i18n("Checking…")
                        : clockDelegate.online ? (clockDelegate.address || i18n("Connected"))
                        : i18n("Not connected")
                    color: clockDelegate.online ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "clock"
            text: i18n("No clocks found")
            explanation: i18n("Clocks with Tube Clock firmware and WiFi turned on appear here automatically. "
                              + "A clock with a static IP address that does not show up can be added by its address.")
            helpfulAction: Kirigami.Action {
                text: i18n("Set Up a Clock…")
                icon.name: "run-build-install"
                onTriggered: applicationWindow().startSetup()
            }
        }
    }

    Kirigami.PromptDialog {
        id: addDialog
        title: i18n("Add Clock by Address")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: {
            const host = hostField.text.trim()
            if (host.length > 0)
                page.openClock(nameField.text.trim() || host, host, 80)
        }
        Kirigami.FormLayout {
            QQC2.TextField {
                id: hostField
                Kirigami.FormData.label: i18n("IP address or host name:")
                placeholderText: "192.168.1.50"
            }
            QQC2.TextField {
                id: nameField
                Kirigami.FormData.label: i18n("Clock name:")
                placeholderText: "tubeclock-a1b2c3"
            }
        }
    }
}
