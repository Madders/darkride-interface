//
//  Toggle.qml
//  qml/darkride/panel
//
//  iOS's switch: green when on, the knob sliding across.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import "."

QQC2.Switch {
    id: toggle

    implicitWidth: 44 * Theme.unit
    implicitHeight: Theme.controlHeight
    padding: 0
    opacity: enabled ? 1 : 0.4

    indicator: Rectangle {
        width: toggle.width
        height: toggle.height
        radius: height / 2
        color: toggle.checked ? Theme.on : Theme.fill
        Behavior on color { ColorAnimation { duration: 150 } }

        Rectangle {
            x: toggle.checked ? parent.width - width - 2 * Theme.unit : 2 * Theme.unit
            y: 2 * Theme.unit
            width: parent.height - 4 * Theme.unit
            height: width
            radius: width / 2
            color: "#ffffff"
            border.color: Qt.rgba(0, 0, 0, 0.08)
            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Item {}
}
