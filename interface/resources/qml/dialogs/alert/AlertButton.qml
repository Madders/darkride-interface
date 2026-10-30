//
//  AlertButton.qml
//  interface/resources/qml/dialogs/alert
//
//  Created on 2026-10-19 for Darkride's popups.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2

// One of an alert's buttons: blue when it is the default, red words when it deletes or replaces something for good.
// It shows a focus ring only once the keyboard moves between the buttons (Tab), as on a Mac.
QQC2.AbstractButton {
    id: button

    property real unit: 1
    readonly property AlertLook look: AlertLook {}
    // "default", "destructive" or "plain"
    property string role: "plain"

    implicitHeight: 28 * unit
    padding: 0
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: 7 * button.unit
        color: button.role === "default" ? (button.down ? button.look.tintPressed : button.look.tint)
                                         : (button.down ? button.look.buttonPressed : button.look.button)
        border.width: button.visualFocus ? 2 : 0
        border.color: button.look.focus
    }

    contentItem: Text {
        text: button.text
        textFormat: Text.PlainText
        leftPadding: 10 * button.unit
        rightPadding: 10 * button.unit
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: button.role === "destructive" ? button.look.destructive
             : button.role === "default" ? "#ffffff" : button.look.buttonText
        font.pixelSize: 13 * button.unit
        font.weight: button.role === "default" ? Font.Medium : Font.Normal
    }
}
