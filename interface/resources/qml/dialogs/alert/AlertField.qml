//
//  AlertField.qml
//  interface/resources/qml/dialogs/alert
//
//  Created on 2026-10-19 for Darkride's popups.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2

// An alert's text field: dark, with a blue ring while it has the focus. Paths and addresses show in a fixed-width face.
QQC2.TextField {
    id: field

    property real unit: 1
    property bool monospace: false
    readonly property AlertLook look: AlertLook {}

    width: parent ? parent.width : implicitWidth
    implicitHeight: 24 * unit
    leftPadding: 8 * unit
    rightPadding: 8 * unit
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    color: look.title
    placeholderTextColor: look.placeholder
    selectionColor: look.tint
    selectedTextColor: "#ffffff"
    font.pixelSize: (monospace ? 11 : 12) * unit
    font.family: monospace ? (Qt.platform.os === "osx" ? "Menlo" : (Qt.platform.os === "windows" ? "Consolas" : "monospace"))
                           : Qt.application.font.family

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: 9 * field.unit
            color: "transparent"
            border.width: 3
            border.color: field.look.focus
            visible: field.activeFocus
        }
        Rectangle {
            anchors.fill: parent
            radius: 6 * field.unit
            color: field.look.well
            border.width: 1
            border.color: field.look.wellBorder
        }
    }
}
