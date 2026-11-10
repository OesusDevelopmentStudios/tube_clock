// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// One clock on the network.
Kirigami.ScrollablePage {
    id: page
    required property string clockName
    required property string clockHost
    property int clockPort: 80
    readonly property ConfigForm cfg: device.config

    title: clockName

    DeviceController {
        id: device
    }
    Component.onCompleted: device.open(clockName, clockHost, clockPort)

    actions: [
        Kirigami.Action {
            text: i18n("Sync with Computer Time")
            icon.name: "chronometer-reset"
            enabled: device.paired && device.online && !device.busy
            onTriggered: device.syncTime()
        },
        Kirigami.Action {
            text: device.info.display_on === false ? i18n("Turn Tubes On") : i18n("Turn Tubes Off")
            icon.name: "system-shutdown"
            enabled: device.paired && device.online
            onTriggered: device.setDisplay(device.info.display_on === false)
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: device.refresh()
        },
        Kirigami.Action {
            text: i18n("Restart Clock")
            icon.name: "system-reboot"
            displayHint: Kirigami.DisplayHint.AlwaysHide
            enabled: device.paired && device.online
            onTriggered: device.reboot()
        },
        Kirigami.Action {
            text: i18n("Forget This Clock")
            icon.name: "edit-delete"
            displayHint: Kirigami.DisplayHint.AlwaysHide
            onTriggered: {
                device.forget()
                applicationWindow().showClocks()
            }
        }
    ]

    // First contact: nothing to show yet, so a large spinner fills the page.
    Kirigami.LoadingPlaceholder {
        anchors.centerIn: parent
        visible: device.busy && !device.online
        text: i18n("Syncing with clock…")
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        // Later actions (saving, uploading, refreshing): a small spinner above the content.
        RowLayout {
            Layout.fillWidth: true
            visible: device.busy && device.online
            QQC2.BusyIndicator {
                running: visible
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            }
            QQC2.Label {
                text: device.uploadDone < device.uploadTotal ? i18n("Uploading %1 of %2…", device.uploadDone, device.uploadTotal)
                                             : i18n("Syncing with clock…")
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: device.message !== ""
            type: device.messageIsError ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            text: device.message
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !device.online && !device.busy && device.address !== ""
            type: Kirigami.MessageType.Warning
            text: i18n("The clock does not answer at %1. Is it powered, connected to WiFi and is remote access on?",
                       device.address + (page.clockHost && page.clockHost !== device.address ? " (" + page.clockHost + ")" : ""))
        }

        // ---- pairing -------------------------------------------------------------
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !device.paired && device.online
            type: Kirigami.MessageType.Information
            text: i18n("This computer is not paired with the clock. Pairing happens automatically when the "
                       + "clock is set up with this app. If it was set up on another computer, enter its pairing key.")
        }
        RowLayout {
            visible: !device.paired && device.online
            Kirigami.PasswordField {
                id: tokenField
                Layout.fillWidth: true
                placeholderText: i18n("Pairing key (32 characters)")
            }
            QQC2.Button {
                text: i18n("Pair")
                onClicked: device.pair(tokenField.text)
            }
        }

        // ---- status --------------------------------------------------------------
        Kirigami.FormLayout {
            Layout.fillWidth: true
            visible: device.online
            QQC2.Label {
                Kirigami.FormData.label: i18n("Model:")
                text: device.info.model_name ?? ""
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Firmware:")
                text: device.info.firmware ?? ""
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Address:")
                text: (device.info.ip ?? "") + "  (" + page.clockHost + ")"
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Clock time (UTC):")
                text: device.info.time_valid
                      ? new Date((device.info.utc ?? 0) * 1000).toISOString().replace("T", " ").substring(0, 19)
                      : i18n("not set")
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Last internet time sync:")
                text: (device.info.last_ntp_utc ?? 0) > 0
                      ? new Date(device.info.last_ntp_utc * 1000).toLocaleString()
                      : i18n("never")
            }
            QQC2.Label {
                Kirigami.FormData.label: i18n("Free space:")
                text: i18n("%1 of %2 KiB",
                           Math.round(((device.info.fs_total ?? 0) - (device.info.fs_used ?? 0)) / 1024),
                           Math.round((device.info.fs_total ?? 0) / 1024))
            }
        }

        // ---- settings ------------------------------------------------------------
        Kirigami.Heading {
            level: 2
            text: i18n("Settings")
            visible: device.paired && device.online
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            visible: device.paired && device.online
            TimeZoneField {
                Kirigami.FormData.label: i18n("Time zone:")
                config: cfg
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
            QQC2.Switch {
                Kirigami.FormData.label: i18n("Network time:")
                text: i18n("Synchronize once a day")
                checked: cfg.ntpEnabled
                onToggled: cfg.ntpEnabled = checked
            }
            QQC2.TextField {
                Kirigami.FormData.label: i18n("Time server:")
                enabled: cfg.ntpEnabled
                text: cfg.ntpServer
                onTextEdited: cfg.ntpServer = text.trim()
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
            QQC2.Button {
                text: i18n("Apply Settings")
                icon.name: "dialog-ok-apply"
                enabled: cfg.problems.length === 0 && !device.busy
                onClicked: device.applySettings()
            }
        }

        // ---- image sets ----------------------------------------------------------
        Kirigami.Heading {
            level: 2
            text: i18n("Image Sets on the Clock")
            visible: device.paired && device.online
        }
        Repeater {
            model: device.paired ? device.sets : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                QQC2.RadioButton {
                    text: modelData.name
                    checked: device.info.image_set === modelData.name
                    enabled: modelData.complete
                    onToggled: device.selectSet(modelData.name)
                }
                QQC2.Label {
                    visible: !modelData.complete
                    text: i18n("(incomplete)")
                    color: Kirigami.Theme.negativeTextColor
                }
                Item {
                    Layout.fillWidth: true
                }
                QQC2.ToolButton {
                    icon.name: "edit-delete"
                    text: i18n("Delete")
                    onClicked: device.deleteSet(modelData.name)
                }
            }
        }
        RowLayout {
            visible: device.paired && device.online
            QQC2.Button {
                text: i18n("Import Image Set…")
                icon.name: "document-import"
                onClicked: importDialog.open()
            }
        }
        Repeater {
            model: device.imports
            delegate: RowLayout {
                required property int index
                required property string name
                required property var bytes
                TubePreview {
                    modelId: device.imports.modelId
                    setName: name
                    tubeHeight: Kirigami.Units.gridUnit * 2.5
                }
                QQC2.Label {
                    text: i18n("%1 (%2 KiB)", name, Math.round(bytes / 1024))
                }
                QQC2.Button {
                    text: i18n("Upload to Clock")
                    icon.name: "cloud-upload"
                    enabled: !device.busy
                    onClicked: device.uploadSet(index)
                }
                QQC2.ToolButton {
                    icon.name: "edit-delete"
                    onClicked: device.imports.remove(index)
                }
            }
        }

        // ---- network -------------------------------------------------------------
        Kirigami.Heading {
            level: 2
            text: i18n("Network")
            visible: device.paired && device.online
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            visible: device.paired && device.online
            QQC2.ComboBox {
                Kirigami.FormData.label: i18n("Address:")
                model: [i18n("Automatic (DHCP)"), i18n("Static IP")]
                currentIndex: cfg.wifiMode === "static" ? 1 : 0
                onActivated: cfg.wifiMode = currentIndex === 1 ? "static" : "dhcp"
            }
            QQC2.TextField {
                Kirigami.FormData.label: i18n("Network name (SSID):")
                text: cfg.ssid
                onTextEdited: cfg.ssid = text
            }
            Kirigami.PasswordField {
                Kirigami.FormData.label: i18n("New password:")
                placeholderText: i18n("Leave empty to keep the current one")
                onTextEdited: cfg.password = text
            }
            QQC2.TextField {
                Kirigami.FormData.label: i18n("IP address:")
                visible: cfg.wifiMode === "static"
                text: cfg.ip
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
                onTextEdited: cfg.gateway = text
            }
            QQC2.Button {
                text: i18n("Save Network Settings and Restart")
                icon.name: "network-wireless"
                onClicked: networkDialog.open()
            }
            QQC2.Button {
                Kirigami.FormData.label: i18n("Remote access:")
                text: i18n("Turn Off Remote Access…")
                icon.name: "network-disconnect"
                onClicked: remoteDialog.open()
            }
        }
    }

    ImportSetDialog {
        id: importDialog
        target: device.imports
    }

    Kirigami.PromptDialog {
        id: networkDialog
        title: i18n("Change Network Settings?")
        subtitle: i18n("The clock restarts with the new settings. If they are wrong, the clock cannot be reached "
                       + "any more and has to be set up again over USB.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: device.applyNetwork()
    }
    Kirigami.PromptDialog {
        id: remoteDialog
        title: i18n("Turn Off Remote Access?")
        subtitle: i18n("This app will no longer be able to reach the clock. To turn it on again, open the "
                       + "clock's menu with the middle button and set \"Remote access\" to On.")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: device.disableRemoteAccess()
    }
}
