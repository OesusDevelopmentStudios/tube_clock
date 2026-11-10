// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Converts pictures into an image set for the clock.
Kirigami.Dialog {
    id: root
    required property ImageSetModel target
    property var files: []
    property color borderColor: "black"
    property string error: ""

    title: i18n("Import Image Set")
    preferredWidth: Kirigami.Units.gridUnit * 26
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: i18n("Import")
            icon.name: "document-import"
            enabled: root.files.length > 0 && nameField.text.trim() !== ""
            onTriggered: {
                const before = root.target.count
                root.error = root.target.importFiles(root.files, nameField.text, fitBox.currentIndex, root.borderColor)
                // Success adds a row; remaining text is a warning about skipped files.
                if (root.target.count > before && root.error === "")
                    root.close()
            }
        },
        Kirigami.Action {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            onTriggered: root.close()
        }
    ]
    onOpened: {
        files = []
        error = ""
        nameField.text = ""
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Select 10 pictures, one per digit, whose names end with the digit (for example "
                       + "0.png … 9.png or digit-7.jpg); they are shown on every tube. "
                       + "For a different picture on each tube, name them <set>_<tube>_<digit> "
                       + "(tube 0 is the leftmost). Any common picture format works; the app converts them.")
        }
        Kirigami.FormLayout {
            Layout.fillWidth: true
            QQC2.TextField {
                id: nameField
                Kirigami.FormData.label: i18n("Name:")
                placeholderText: i18n("e.g. retro-nixie")
                onTextEdited: if (text !== root.target.sanitizeName(text) && text.length > 0)
                                  hint.text = i18n("Will be saved as \"%1\"", root.target.sanitizeName(text))
                              else
                                  hint.text = ""
            }
            QQC2.Label {
                id: hint
                visible: text !== ""
                opacity: 0.7
            }
            RowLayout {
                Kirigami.FormData.label: i18n("Pictures:")
                QQC2.Button {
                    text: i18n("Choose…")
                    icon.name: "document-open"
                    onClicked: fileDialog.open()
                }
                QQC2.Label {
                    text: i18np("%1 picture selected", "%1 pictures selected", root.files.length)
                }
            }
            QQC2.ComboBox {
                id: fitBox
                Kirigami.FormData.label: i18n("Fit:")
                model: [i18n("Show whole picture"), i18n("Fill the tube (crop)"), i18n("Stretch")]
            }
            RowLayout {
                Kirigami.FormData.label: i18n("Border colour:")
                Rectangle {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 1.5
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 1.5
                    color: root.borderColor
                    border.color: Kirigami.Theme.textColor
                }
                QQC2.Button {
                    text: i18n("Change…")
                    onClicked: colorDialog.open()
                }
            }
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.error !== ""
            type: Kirigami.MessageType.Error
            text: root.error
        }
    }

    FileDialog {
        id: fileDialog
        title: i18n("Choose Pictures")
        fileMode: FileDialog.OpenFiles
        nameFilters: [i18n("Pictures (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff)"), i18n("All files (*)")]
        onAccepted: {
            root.files = selectedFiles
            if (nameField.text === "" && selectedFiles.length > 0) {
                const parts = selectedFiles[0].toString().split("/")
                nameField.text = root.target.sanitizeName(parts[parts.length - 2] ?? "")
            }
        }
    }
    ColorDialog {
        id: colorDialog
        selectedColor: root.borderColor
        onAccepted: root.borderColor = selectedColor
    }
}
