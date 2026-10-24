//
//  Segmented.qml
//  qml/darkride/panel
//
//  iOS's segmented control: a few mutually exclusive choices on a track, the chosen one on a raised thumb that slides.
//  model: [{ text, value }]; current: the chosen value; chosen(value) when one is clicked.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import "."

Rectangle {
    id: segmented

    property var model: []
    property var current
    signal chosen(var value)

    readonly property int currentIndex: {
        for (var i = 0; i < model.length; i++) {
            if (model[i].value === current) {
                return i;
            }
        }
        return -1;
    }
    readonly property Item currentItem: repeater.count, currentIndex >= 0 ? repeater.itemAt(currentIndex) : null

    implicitWidth: segments.implicitWidth + 4 * Theme.unit
    implicitHeight: Theme.controlHeight
    radius: 8 * Theme.unit
    color: Theme.fill
    opacity: enabled ? 1 : 0.4

    Rectangle {
        id: thumb
        visible: segmented.currentItem !== null
        x: segments.x + (segmented.currentItem ? segmented.currentItem.x : 0)
        y: segments.y
        width: segmented.currentItem ? segmented.currentItem.width : 0
        height: segments.height
        radius: 6.5 * Theme.unit
        color: Theme.segmentThumb
        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    }

    Row {
        id: segments
        x: 2 * Theme.unit
        y: 2 * Theme.unit
        height: parent.height - 4 * Theme.unit

        Repeater {
            id: repeater
            model: segmented.model

            delegate: Item {
                readonly property bool selected: index === segmented.currentIndex
                width: Math.max(36 * Theme.unit, label.implicitWidth + 18 * Theme.unit)
                height: segments.height

                // the thin dividers between unchosen neighbours, as iOS draws them
                Rectangle {
                    visible: index > 0 && !selected && index - 1 !== segmented.currentIndex
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: parent.height * 0.5
                    color: Theme.separator
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData.text
                    color: Theme.text
                    font.pixelSize: Theme.controlTextSize
                    font.weight: selected ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: segmented.chosen(modelData.value)
                }
            }
        }
    }
}
