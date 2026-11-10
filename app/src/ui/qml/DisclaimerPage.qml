// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

Kirigami.ScrollablePage {
    title: i18n("Before You Start")
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: "dialog-warning"
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
            Layout.alignment: Qt.AlignHCenter
        }

        Kirigami.Heading {
            text: i18n("This app can damage your clock")
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Tube Clock replaces the firmware of your clock with an unofficial one. "
                       + "Flashing firmware always carries a risk: an interrupted transfer, a wrong clock "
                       + "model, a faulty USB cable or a bug can leave the clock unusable (\"bricked\"), "
                       + "and it may void your warranty.")
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("The app makes a full backup of the original firmware before it changes anything. "
                       + "Keep that backup: it is the only way back to the manufacturer's firmware, and it only "
                       + "works for the clock it was made from.")
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("This software comes with ABSOLUTELY NO WARRANTY (GNU GPL version 3). "
                       + "You use it entirely at your own risk.")
        }

        QQC2.CheckBox {
            id: understood
            Layout.fillWidth: true
            text: i18n("I understand that using this app may damage my clock")
        }

        QQC2.Button {
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Continue")
            icon.name: "go-next"
            enabled: understood.checked
            onClicked: AppSettings.disclaimerAccepted = true
        }
    }
}
