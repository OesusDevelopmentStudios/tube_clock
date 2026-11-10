// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.kde.kirigami as Kirigami

// Six tubes showing "12:34:56" with the pictures of an image set.
Row {
    id: root
    required property int modelId
    required property string setName
    property real tubeHeight: Kirigami.Units.gridUnit * 4
    property var digits: [1, 2, 3, 4, 5, 6]

    spacing: Math.round(tubeHeight / 24)

    Repeater {
        model: 6
        delegate: Rectangle {
            required property int index
            width: Math.round(root.tubeHeight * 135 / 240)
            height: root.tubeHeight
            color: "black"
            radius: width / 6
            clip: true
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                sourceSize.height: root.tubeHeight * 2
                source: "image://tubeclock/%1/%2/%3/%4".arg(root.modelId).arg(root.setName).arg(index).arg(root.digits[index])
            }
        }
    }
}
