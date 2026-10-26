//
//  ValueSlider.qml
//  qml/darkride/panel
//
//  iOS's slider (a thin track, the part up to the knob in blue, a white knob) with the value beside it.
//  moved(value) while it is dragged; the owner sets value back from the engine.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import "."

RowLayout {
    id: valueSlider

    property real from: 0
    property real to: 1
    property real stepSize: 0.1
    property real value: 0
    property int decimals: 1
    property string units: ""
    signal moved(real value)

    spacing: 10 * Theme.unit
    Layout.fillWidth: true

    QQC2.Slider {
        id: slider
        Layout.fillWidth: true
        implicitWidth: 120 * Theme.unit
        implicitHeight: Theme.controlHeight
        padding: 0
        from: valueSlider.from
        to: valueSlider.to
        stepSize: valueSlider.stepSize
        value: valueSlider.value
        onMoved: valueSlider.moved(value)

        background: Item {
            x: slider.leftPadding
            width: slider.availableWidth
            height: slider.height

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 4 * Theme.unit
                radius: height / 2
                color: Theme.sliderTrack

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: Theme.tint
                }
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2
            width: 22 * Theme.unit
            height: width
            radius: width / 2
            color: slider.pressed ? "#e5e5ea" : "#ffffff"
            border.color: Qt.rgba(0, 0, 0, 0.12)
        }
    }

    Text {
        text: valueSlider.value.toFixed(valueSlider.decimals) + valueSlider.units
        color: Theme.secondaryText
        font.pixelSize: Theme.bodySize
        horizontalAlignment: Text.AlignRight
        Layout.preferredWidth: 46 * Theme.unit
    }
}
