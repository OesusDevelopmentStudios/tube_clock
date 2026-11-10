// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Step 1: choose model and port, connect, back up the original firmware.
Kirigami.ScrollablePage {
    id: page
    required property FlashController controller
    property bool backupDone: controller.backupFile !== "" && !controller.busy && controller.errorString === ""
    property bool skipBackup: false

    title: i18n("Set Up a Clock: Connect")

    actions: [
        Kirigami.Action {
            text: i18n("Next")
            icon.name: "go-next"
            enabled: controller.mac !== "" && (page.backupDone || page.skipBackup) && !controller.busy
            onTriggered: applicationWindow().pageStack.push(Qt.resolvedUrl("SetupNetworkPage.qml"), { controller: page.controller })
        }
    ]

    SerialPorts {
        id: ports
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        DamageWarning {
            Layout.fillWidth: true
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: controller.bundleError !== ""
            text: controller.bundleError
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            QQC2.ComboBox {
                id: modelBox
                Kirigami.FormData.label: i18n("Clock model:")
                model: controller.models
                textRole: "title"
                valueRole: "id"
                Component.onCompleted: currentIndex = Math.max(0, indexOfValue(controller.modelId))
                onActivated: controller.modelId = currentValue
            }
            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 22
                wrapMode: Text.Wrap
                opacity: 0.7
                text: i18n("Choose carefully: firmware for another model can leave the tubes dark. "
                           + "PunkCyber clocks show the tubes in reversed order.")
            }

            RowLayout {
                Kirigami.FormData.label: i18n("USB port:")
                QQC2.ComboBox {
                    id: portBox
                    model: ports
                    textRole: "display"
                    valueRole: "port"
                    Layout.minimumWidth: Kirigami.Units.gridUnit * 16
                    onCurrentValueChanged: controller.port = currentValue ?? ""
                    Component.onCompleted: controller.port = currentValue ?? ""
                }
                QQC2.Button {
                    icon.name: "view-refresh"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Refresh ports")
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    onClicked: ports.refresh()
                }
            }
            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Speed:")
                model: [460800, 115200]
                displayText: i18n("%1 baud", currentText)
                onActivated: controller.baud = currentValue
                Component.onCompleted: currentIndex = indexOfValue(controller.baud)
            }

            QQC2.Button {
                text: i18n("Connect to Clock")
                icon.name: "network-connect"
                enabled: controller.port !== "" && !controller.busy
                onClicked: controller.detect()
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Found:")
                visible: controller.mac !== ""
                text: i18n("%1, MAC %2, %3 flash", controller.chip, controller.mac, controller.flashSize)
            }
        }

        Kirigami.Heading {
            level: 2
            text: i18n("Backup")
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("A full copy of the clock's flash memory (%1) is saved to %2. "
                       + "It takes about two minutes per 4 MB.", controller.flashSize || i18n("all of it"), AppSettings.backupDir)
        }
        RowLayout {
            QQC2.Button {
                text: i18n("Back Up Now")
                icon.name: "document-save"
                enabled: controller.mac !== "" && !controller.busy
                onClicked: controller.backup()
            }
            QQC2.CheckBox {
                text: i18n("Skip the backup (not recommended)")
                checked: page.skipBackup
                onToggled: {
                    if (checked)
                        skipDialog.open()
                    else
                        page.skipBackup = false
                }
            }
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Positive
            visible: page.backupDone
            text: i18n("Backup saved: %1", controller.backupFile)
        }

        QQC2.ProgressBar {
            Layout.fillWidth: true
            visible: controller.busy
            from: 0
            to: 100
            value: controller.progress
            indeterminate: controller.progress === 0
        }
        QQC2.Label {
            visible: controller.busy
            text: controller.task
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: controller.errorString !== "" && !controller.busy
            text: controller.errorString
        }
    }

    Kirigami.PromptDialog {
        id: skipDialog
        title: i18n("Skip the Backup?")
        subtitle: i18n("Without a backup the original firmware cannot be restored. Skip only if you already have a backup of this clock.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Skip Backup")
                icon.name: "dialog-warning"
                onTriggered: {
                    page.skipBackup = true
                    skipDialog.close()
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: skipDialog.close()
            }
        ]
        onRejected: page.skipBackup = false
    }
}
