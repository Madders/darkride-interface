//
//  ValueSlider.qml
//  qml/darkride/panel
//
//  iOS's slider (a thin track, the part up to the knob in blue, a white knob) with the value beside it.
//  moved(value) while it is dragged or a value is typed; the owner sets value back from the engine.
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
    property var stops: []   // the values the knob lands on while dragged, in order (empty: every stepSize)
    signal moved(real value)

    function nearestStop(value) {
        var nearest = stops[0];
        for (var i = 1; i < stops.length; i++) {
            if (Math.abs(stops[i] - value) < Math.abs(nearest - value)) {
                nearest = stops[i];
            }
        }
        return nearest;
    }

    function stopAlong(direction) {
        var i = stops.indexOf(nearestStop(value));
        return stops[Math.max(0, Math.min(stops.length - 1, i + direction))];
    }

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
        stepSize: valueSlider.stops.length > 0 ? 0 : valueSlider.stepSize
        value: valueSlider.value
        onMoved: {
            if (valueSlider.stops.length > 0) {
                valueSlider.moved(valueSlider.nearestStop(value));
                // the knob sits on the stop, not under the pointer, even when the stop hasn't changed
                slider.value = Qt.binding(function () { return valueSlider.value; });
            } else {
                valueSlider.moved(value);
            }
        }
        // with stops, an arrow key moves one stop along (the slider's own step could round back to the same stop)
        Keys.onPressed: {
            var direction = event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 1 :
                event.key === Qt.Key_Left || event.key === Qt.Key_Down ? -1 : 0;
            if (valueSlider.stops.length > 0 && direction !== 0) {
                valueSlider.moved(valueSlider.stopAlong(direction));
                event.accepted = true;
            }
        }

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

    // the value; click it to type another (Return, or going on to another field, sets it; Escape keeps the old one)
    Item {
        Layout.preferredWidth: 46 * Theme.unit
        implicitHeight: Theme.controlHeight

        HoverHandler {
            id: numberHover
            cursorShape: Qt.IBeamCursor
        }
        Rectangle {
            anchors.fill: parent
            visible: numberHover.hovered || number.activeFocus
            radius: 7 * Theme.unit
            color: Theme.fill
            border.width: number.activeFocus ? 1 : 0
            border.color: Theme.tint
        }
        TextInput {
            id: number

            property bool cancelled: false
            property string startText: ""
            readonly property string shown: valueSlider.value.toFixed(valueSlider.decimals) + valueSlider.units

            anchors.fill: parent
            rightPadding: 4 * Theme.unit
            text: shown
            color: activeFocus ? Theme.text : Theme.secondaryText
            selectionColor: Theme.tint
            selectedTextColor: "#ffffff"
            font.pixelSize: Theme.bodySize
            horizontalAlignment: TextInput.AlignRight
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true
            clip: true

            // what was typed, set: a decimal comma too (a Spanish keyboard's), held to the range and the decimals
            // shown; a click in and out changes nothing, nor does Escape
            function commit() {
                var typed = parseFloat(text.replace(",", "."));
                if (!cancelled && text !== startText && !isNaN(typed)) {
                    var scale = Math.pow(10, valueSlider.decimals);
                    valueSlider.moved(Math.min(valueSlider.to, Math.max(valueSlider.from, Math.round(typed * scale) / scale)));
                }
                cancelled = false;
                text = Qt.binding(function () { return number.shown; });
                startText = text;
            }

            onActiveFocusChanged: {
                if (activeFocus) {
                    text = valueSlider.value.toFixed(valueSlider.decimals);
                    startText = text;
                    selectAll();
                } else {
                    commit();
                }
            }
            // Return sets it at once (also for a field given its text without the keyboard's focus, as a test does)
            onAccepted: {
                commit();
                focus = false;
            }
            Keys.onEscapePressed: {
                cancelled = true;
                focus = false;
            }
        }
    }
}
