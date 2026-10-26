//
//  AlertCheck.qml
//  interface/resources/qml/dialogs/alert
//
//  Created on 2026-10-19 for Darkride's popups.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2

// An alert's checkbox, at the left under its fields.
QQC2.CheckBox {
    id: check

    property real unit: 1

    padding: 0
    spacing: 6 * unit
    font.pixelSize: 12 * unit
    focusPolicy: Qt.TabFocus

    indicator: Rectangle {
        x: check.leftPadding
        y: (check.height - height) / 2
        width: 12 * check.unit
        height: 12 * check.unit
        radius: 3 * check.unit
        color: check.checked ? (check.enabled ? "#0b6bd8" : Qt.rgba(1, 1, 1, 0.14)) : Qt.rgba(0, 0, 0, 0.28)
        border.width: check.checked ? 0 : 1
        border.color: Qt.rgba(1, 1, 1, 0.3)

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: 5 * check.unit
            color: "transparent"
            border.width: 2
            border.color: Qt.rgba(10 / 255, 132 / 255, 1, 0.59)
            visible: check.visualFocus
        }

        Canvas {
            anchors.fill: parent
            visible: check.checked
            onPaint: {
                var context = getContext("2d");
                context.reset();
                context.scale(width / 12, height / 12);
                context.strokeStyle = "#ffffff";
                context.lineWidth = 1.6;
                context.lineCap = "round";
                context.lineJoin = "round";
                context.beginPath();
                context.moveTo(3, 6.2);
                context.lineTo(5.2, 8.4);
                context.lineTo(9, 3.8);
                context.stroke();
            }
        }
    }

    contentItem: Text {
        leftPadding: check.indicator.width + check.spacing
        text: check.text
        textFormat: Text.PlainText
        font: check.font
        color: check.enabled ? Qt.rgba(1, 1, 1, 0.74) : Qt.rgba(1, 1, 1, 0.35)
        verticalAlignment: Text.AlignVCenter
    }
}
