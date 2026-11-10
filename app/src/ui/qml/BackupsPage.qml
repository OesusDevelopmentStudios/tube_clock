// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Backups of clocks and restoring them over USB.
Kirigami.ScrollablePage {
    id: page
    required property FlashController controller
    property var backups: AppSettings.backups()
    property string selected: ""

    title: i18n("Backups")

    actions: [
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: page.backups = AppSettings.backups()
        }
    ]

    SerialPorts {
        id: ports
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Backups are stored in %1. A backup only works on the clock it was made from "
                       + "(the file name starts with the clock's MAC address) and must cover the clock's whole "
                       + "flash memory.", AppSettings.backupDir)
        }

        Repeater {
            model: page.backups
            delegate: QQC2.RadioDelegate {
                required property var modelData
                Layout.fillWidth: true
                checked: page.selected === modelData.path
                onToggled: page.selected = modelData.path
                text: modelData.name
                contentItem: RowLayout {
                    QQC2.Label {
                        text: modelData.name
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: modelData.modified.toLocaleString(Qt.locale(), Locale.ShortFormat)
                        opacity: 0.7
                    }
                    QQC2.Label {
                        text: modelData.verified ? i18n("checksum OK") : i18n("not verified")
                        color: modelData.verified ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
                    }
                }
            }
        }
        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            visible: page.backups.length === 0
            icon.name: "document-save"
            text: i18n("No backups yet")
            explanation: i18n("A backup is made when you set up a clock.")
        }

        Kirigami.Heading {
            level: 2
            text: i18n("Restore")
        }
        DamageWarning {
            Layout.fillWidth: true
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("USB port:")
                model: ports
                textRole: "display"
                valueRole: "port"
                onCurrentValueChanged: controller.port = currentValue ?? ""
                Component.onCompleted: controller.port = currentValue ?? ""
            }
            RowLayout {
                Kirigami.FormData.label: i18n("Clock:")
                QQC2.Button {
                    text: i18n("Connect to Clock")
                    icon.name: "network-connect"
                    enabled: controller.port !== "" && !controller.busy
                    onClicked: controller.detect()
                }
                QQC2.Label {
                    visible: controller.mac !== ""
                    text: i18n("MAC %1, %2 flash", controller.mac, controller.flashSize)
                }
            }
            QQC2.TextField {
                id: confirmField
                Kirigami.FormData.label: i18n("Type RESTORE to confirm:")
            }
            QQC2.Button {
                text: i18n("Restore Selected Backup")
                icon.name: "document-revert"
                enabled: page.selected !== "" && controller.mac !== "" && confirmField.text === "RESTORE" && !controller.busy
                onClicked: {
                    confirmField.text = ""
                    controller.restore(page.selected)
                }
            }
        }
        QQC2.ProgressBar {
            Layout.fillWidth: true
            visible: controller.busy
            from: 0
            to: 100
            value: controller.progress
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: controller.errorString !== "" && !controller.busy
            text: controller.errorString
        }
    }

    Connections {
        target: controller
        function onFinished(ok, message) {
            if (ok)
                applicationWindow().showPassiveNotification(message)
        }
    }
}
