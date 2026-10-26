//
//  ResetButton.qml
//  qml/darkride/panel
//
//  A small round button that puts a value back to its default; it shows only while the value differs from it, and
//  keeps its room while hidden, so the values of neighbouring rows stay lined up.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import "."

QQC2.AbstractButton {
    id: reset

    property bool needed: true

    opacity: needed ? 1 : 0
    enabled: needed
    implicitWidth: 22 * Theme.unit
    implicitHeight: 22 * Theme.unit
    hoverEnabled: true

    background: Rectangle {
        radius: width / 2
        color: reset.down ? Theme.segmentThumb : (reset.hovered ? Theme.fill : "transparent")
    }

    contentItem: Text {
        text: "↺"   // ↺
        color: Theme.secondaryText
        font.pixelSize: 14 * Theme.unit
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.delay: 600
    QQC2.ToolTip.text: "Default"
}
