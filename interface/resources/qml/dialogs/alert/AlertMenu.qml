//
//  AlertMenu.qml
//  interface/resources/qml/dialogs/alert
//
//  Created on 2026-10-19 for Darkride's popups.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2

// An alert's pop-up list, drawn like its text field, with the up-and-down arrows of a Mac pop-up button.
QQC2.ComboBox {
    id: menu

    property real unit: 1

    width: parent ? parent.width : implicitWidth
    implicitHeight: 24 * unit
    font.pixelSize: 12 * unit

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: 9 * menu.unit
            color: "transparent"
            border.width: 3
            border.color: Qt.rgba(10 / 255, 132 / 255, 1, 0.45)
            visible: menu.visualFocus
        }
        Rectangle {
            anchors.fill: parent
            radius: 6 * menu.unit
            color: menu.down ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.28)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.14)
        }
    }

    contentItem: Text {
        leftPadding: 8 * menu.unit
        rightPadding: 22 * menu.unit
        text: menu.displayText
        textFormat: Text.PlainText
        font: menu.font
        color: "#ffffff"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        x: menu.width - width - 7 * menu.unit
        y: (menu.height - height) / 2
        width: 9 * menu.unit
        height: 12 * menu.unit
        onPaint: {
            var context = getContext("2d");
            context.reset();
            context.scale(width / 9, height / 12);
            context.strokeStyle = Qt.rgba(1, 1, 1, 0.74);
            context.lineWidth = 1.3;
            context.lineCap = "round";
            context.lineJoin = "round";
            context.beginPath();
            context.moveTo(1.5, 4.5);
            context.lineTo(4.5, 1.5);
            context.lineTo(7.5, 4.5);
            context.moveTo(1.5, 7.5);
            context.lineTo(4.5, 10.5);
            context.lineTo(7.5, 7.5);
            context.stroke();
        }
    }

    delegate: QQC2.ItemDelegate {
        width: menu.width - 8 * menu.unit
        height: 22 * menu.unit
        padding: 0
        highlighted: menu.highlightedIndex === index
        contentItem: Text {
            leftPadding: 8 * menu.unit
            rightPadding: 8 * menu.unit
            text: modelData
            textFormat: Text.PlainText
            font: menu.font
            color: "#ffffff"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 4 * menu.unit
            color: highlighted ? "#0b6bd8" : "transparent"
        }
    }

    popup: QQC2.Popup {
        y: menu.height + 3 * menu.unit
        width: menu.width
        padding: 4 * menu.unit
        implicitHeight: Math.min(list.contentHeight, 220 * menu.unit) + 2 * padding

        contentItem: ListView {
            id: list
            clip: true
            model: menu.popup.visible ? menu.delegateModel : null
            currentIndex: menu.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollIndicator.vertical: QQC2.ScrollIndicator { }
        }

        background: Rectangle {
            radius: 8 * menu.unit
            color: "#323236"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.12)
        }
    }
}
