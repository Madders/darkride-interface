//
//  Field.qml
//  qml/darkride/panel
//
//  A text field in a row: a rounded fill, the text at the left as on the Mac (Caity 10-28: not right-justified as iOS
//  keeps a cell's value), blue edge while typing.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import "."

QQC2.TextField {
    id: field

    implicitWidth: 80 * Theme.unit
    implicitHeight: Theme.controlHeight
    leftPadding: 8 * Theme.unit
    rightPadding: 8 * Theme.unit
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    horizontalAlignment: TextInput.AlignLeft
    color: Theme.text
    placeholderTextColor: Theme.tertiaryText
    selectionColor: Theme.tint
    selectedTextColor: "#ffffff"
    font.pixelSize: Theme.bodySize
    selectByMouse: true
    opacity: enabled ? 1 : 0.4

    background: Item {
        Rectangle {
            anchors.fill: parent
            visible: !fieldFrame.shown
            radius: 7 * Theme.unit
            color: Theme.fill
            border.width: field.activeFocus ? 1 : 0
            border.color: Theme.tint
        }
        // a skin's field frame, when it gives one ("checked" while typing)
        SkinFrame {
            id: fieldFrame
            part: "field"
            look: field.activeFocus ? "checked" : "image"
            anchors.fill: parent
        }
    }
}
