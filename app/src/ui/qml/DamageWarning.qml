// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.kde.kirigami as Kirigami

// Shown on every page that writes to the clock.
Kirigami.InlineMessage {
    type: Kirigami.MessageType.Warning
    visible: true
    showCloseButton: false
    text: i18n("Writing to the clock can damage it. Do not unplug the clock or close the app while it is being written.")
}
