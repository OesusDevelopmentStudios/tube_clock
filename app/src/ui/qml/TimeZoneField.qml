// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Button showing the selected zone; opens a searchable list of all zones.
QQC2.Button {
    id: root
    required property ConfigForm config
    signal zoneChosen(string zone)

    text: config.tzName
    icon.name: "preferences-system-time"
    onClicked: dialog.open()

    Kirigami.Dialog {
        id: dialog
        title: i18n("Choose Time Zone")
        preferredWidth: Kirigami.Units.gridUnit * 22
        preferredHeight: Kirigami.Units.gridUnit * 26
        standardButtons: Kirigami.Dialog.NoButton
        onOpened: {
            search.text = ""
            search.forceActiveFocus()
        }

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Kirigami.SearchField {
                id: search
                Layout.fillWidth: true
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 20
                clip: true
                model: AppSettings.zoneIds().filter(z => z.toLowerCase().replace(/_/g, " ")
                                                         .includes(search.text.toLowerCase().trim()))
                delegate: QQC2.ItemDelegate {
                    required property string modelData
                    width: ListView.view.width
                    text: modelData.replace(/_/g, " ")
                    highlighted: modelData === root.config.tzName
                    onClicked: {
                        if (root.config.setZone(modelData))
                            root.zoneChosen(modelData)
                        dialog.close()
                    }
                }
            }
        }
    }
}
