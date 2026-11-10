// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Step 4: choose the image sets that go onto the clock.
Kirigami.ScrollablePage {
    id: page
    required property FlashController controller
    readonly property ImageSetModel sets: controller.imageSets
    readonly property bool tooBig: sets.includedBytes > controller.capacityBytes

    title: i18n("Set Up a Clock: Images")

    actions: [
        Kirigami.Action {
            text: i18n("Back")
            icon.name: "go-previous"
            enabled: !page.controller.busy
            onTriggered: applicationWindow().pageStack.pop()
        },
        Kirigami.Action {
            text: i18n("Import Image Set…")
            icon.name: "document-import"
            onTriggered: importDialog.open()
        },
        Kirigami.Action {
            text: i18n("Next")
            icon.name: "go-next"
            enabled: sets.includedNames.length > 0 && !page.tooBig
            onTriggered: applicationWindow().pageStack.push(Qt.resolvedUrl("SetupFlashPage.qml"), { controller: page.controller })
        }
    ]

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        QQC2.Label {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            wrapMode: Text.Wrap
            text: i18n("Space used: %1 of %2 KiB. The image set shown first is chosen with the radio button; "
                       + "on the clock you can switch sets in the menu.",
                       Math.round(sets.includedBytes / 1024), Math.round(controller.capacityBytes / 1024))
        }
        QQC2.ProgressBar {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            from: 0
            to: controller.capacityBytes
            value: Math.min(sets.includedBytes, controller.capacityBytes)
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: page.tooBig
            text: i18n("The selected sets do not fit on the clock. Unselect some.")
        }
    }

    ListView {
        id: list
        model: page.sets
        spacing: Kirigami.Units.smallSpacing
        delegate: QQC2.ItemDelegate {
            id: setDelegate
            required property int index
            required property string name
            required property bool included
            required property var bytes
            required property bool builtin
            required property bool perTube
            width: ListView.view.width
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                QQC2.CheckBox {
                    checked: setDelegate.included
                    onToggled: page.sets.setIncluded(setDelegate.index, checked)
                }
                QQC2.RadioButton {
                    enabled: setDelegate.included
                    checked: controller.config.imageSet === setDelegate.name
                    onToggled: controller.config.imageSet = setDelegate.name
                    QQC2.ToolTip.text: i18n("Show this set first")
                    QQC2.ToolTip.visible: hovered
                }
                TubePreview {
                    modelId: page.sets.modelId
                    setName: setDelegate.name
                    tubeHeight: Kirigami.Units.gridUnit * 3
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    QQC2.Label {
                        text: setDelegate.name
                        font.bold: true
                    }
                    QQC2.Label {
                        opacity: 0.7
                        text: i18n("%1 KiB", Math.round(setDelegate.bytes / 1024))
                              + (setDelegate.perTube ? " · " + i18n("different picture per tube") : "")
                              + (setDelegate.builtin ? " · " + i18n("included with the app") : "")
                    }
                }
                QQC2.ToolButton {
                    visible: !setDelegate.builtin
                    icon.name: "edit-delete"
                    text: i18n("Remove")
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: page.sets.remove(setDelegate.index)
                }
            }
        }
    }

    ImportSetDialog {
        id: importDialog
        target: page.sets
    }
}
