// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import TubeClock

// Pattern, colour and brightness of the LEDs behind the tubes.
ColumnLayout {
    id: root
    required property ConfigForm config

    readonly property var patternNames: ({
        "off": i18n("Off"),
        "constant": i18n("Constant colour"),
        "rainbow": i18n("Rainbow"),
        "pulse": i18n("Pulse"),
        "breath": i18n("Breathing")
    })

    QQC2.ComboBox {
        model: root.config.patterns.map(p => ({ id: p, title: root.patternNames[p] ?? p }))
        textRole: "title"
        valueRole: "id"
        currentIndex: root.config.patterns.indexOf(root.config.pattern)
        onActivated: root.config.pattern = currentValue
    }
    RowLayout {
        enabled: root.config.pattern !== "off" && root.config.pattern !== "rainbow"
        QQC2.Label {
            text: i18n("Colour:")
        }
        QQC2.Slider {
            from: 0
            to: 767
            stepSize: 1
            value: root.config.colorPhase
            onMoved: root.config.colorPhase = value
        }
        Rectangle {
            Layout.preferredWidth: Kirigami.Units.gridUnit
            Layout.preferredHeight: Kirigami.Units.gridUnit
            radius: Kirigami.Units.gridUnit / 2
            color: root.config.phaseColor(root.config.colorPhase)
        }
    }
    RowLayout {
        QQC2.Label {
            text: i18n("Brightness:")
        }
        QQC2.Slider {
            from: 0
            to: 7
            stepSize: 1
            value: root.config.intensity
            onMoved: root.config.intensity = value
        }
        QQC2.Label {
            text: root.config.intensity
        }
    }
}
