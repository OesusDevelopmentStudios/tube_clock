// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Step 5: review and flash.
Kirigami.ScrollablePage {
    id: page
    required property FlashController controller
    readonly property ConfigForm cfg: controller.config
    property var problems: controller.problems()

    title: i18n("Set Up a Clock: Write")

    actions: [
        Kirigami.Action {
            text: i18n("Back")
            icon.name: "go-previous"
            enabled: !page.controller.busy
            onTriggered: applicationWindow().pageStack.pop()
        }
    ]

    Connections {
        target: controller
        function onStateChanged() { page.problems = controller.problems() }
    }
    Connections {
        target: cfg
        function onChanged() { page.problems = controller.problems() }
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        DamageWarning {
            Layout.fillWidth: true
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            QQC2.Label {
                Kirigami.FormData.label: i18n("Clock:")
                text: controller.models.find(m => m.id === controller.modelId)?.title ?? controller.modelId
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Firmware:")
                text: i18n("Tube Clock %1", controller.firmwareVersion)
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Name:")
                text: cfg.clockName
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("WiFi:")
                text: cfg.wifiMode === "off" ? i18n("Off")
                    : cfg.wifiMode === "dhcp" ? i18n("%1, automatic address", cfg.ssid)
                    : i18n("%1, %2", cfg.ssid, cfg.ip)
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Time zone:")
                text: cfg.tzName + (cfg.dst ? "" : " " + i18n("(no summer time)"))
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Image sets:")
                text: controller.imageSets.includedNames.join(", ")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: page.problems.length > 0 && !controller.busy && !controller.flashed
            text: page.problems.join("\n")
        }

        QQC2.Button {
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Write to Clock")
            icon.name: "document-send"
            enabled: page.problems.length === 0 && !controller.busy
            onClicked: confirmDialog.open()
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
            type: Kirigami.MessageType.Positive
            visible: controller.flashed && !controller.busy
            text: cfg.wifiMode === "off"
                ? i18n("Done. The clock restarts with the new firmware. Set the time in the clock's menu (middle button).")
                : i18n("Done. The clock restarts and joins your WiFi; it then appears under Clocks.")
            actions: [
                Kirigami.Action {
                    visible: cfg.wifiMode !== "off"
                    text: i18n("Show Clocks")
                    icon.name: "clock"
                    onTriggered: applicationWindow().showClocks()
                }
            ]
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: controller.errorString !== "" && !controller.busy
            text: controller.errorString
        }

        QQC2.TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 10
            readOnly: true
            font.family: "monospace"
            text: controller.log
            visible: text.length > 0
        }
    }

    Kirigami.PromptDialog {
        id: confirmDialog
        title: i18n("Write to the Clock?")
        subtitle: i18n("This replaces the firmware and all files on the clock. Keep the clock connected "
                       + "until the app reports that it is done. This may damage the clock.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Write")
                icon.name: "document-send"
                onTriggered: {
                    confirmDialog.close()
                    controller.flash()
                }
            },
            Kirigami.Action {
                text: i18n("Cancel")
                icon.name: "dialog-cancel"
                onTriggered: confirmDialog.close()
            }
        ]
    }
}
