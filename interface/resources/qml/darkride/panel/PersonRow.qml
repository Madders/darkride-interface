//
//  PersonRow.qml
//  qml/darkride/panel
//
//  One person in the world, for a NearbyPeople list: their name and distance, a live level of how loud they are, your
//  volume for them (the name scrubs it), its reset, and Mute. With actions, a click on the name opens a line under it:
//  Block (and Unblock), and for whoever may kick in this world, Mute for Everyone and Kick (a first press asks, a
//  second does it).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import "."

Column {
    id: person
    objectName: "personRow"   // a test opens its actions by it

    property var nearby                // the NearbyPeople this row is from
    property int index: -1
    property string name: ""
    property real distance: -1
    property real level: 0
    property real gain: 0
    property bool muted: false
    property bool blocked: false
    property bool actions: false       // the line of actions under the row, opened by a click on it
    property bool open: false
    property string confirming: ""     // the action waiting for its second press

    width: parent ? parent.width : 0

    // a small action button in the line under the row
    component ActionButton: Rectangle {
        id: action
        property string text: ""
        property bool danger: false
        signal pressed()
        implicitWidth: actionLabel.implicitWidth + 18 * Theme.unit
        implicitHeight: 26 * Theme.unit
        radius: 7 * Theme.unit
        color: Theme.fill
        Text {
            id: actionLabel
            anchors.centerIn: parent
            text: action.text
            color: action.danger ? Theme.destructive : Theme.tint
            font.pixelSize: Theme.controlTextSize
            font.bold: action.danger && action.text.indexOf("?") !== -1
        }
        MouseArea {
            anchors.fill: parent
            onClicked: action.pressed()
        }
    }

    ListRow {
        cardItem: person
        label: person.name
        detail: person.blocked ? "blocked" : person.muted ? "muted"
            : (person.distance >= 0 ? Math.round(person.distance) + " m away" : "")
        fill: true
        labelWidth: 120 * Theme.unit
        scrubbable: !person.blocked
        scrubValue: person.gain
        scrubFrom: -60
        scrubTo: 20
        scrubStep: 0.5
        onScrubbed: person.nearby.setGain(person.index, Math.round(value))
        clickable: person.actions
        onClicked: {
            person.open = !person.open;
            person.confirming = "";
        }

        // how loud they are now
        Rectangle {
            implicitWidth: 6 * Theme.unit
            implicitHeight: 18 * Theme.unit
            radius: width / 2
            color: Theme.sliderTrack
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: parent.height * (person.blocked ? 0 : person.level)
                radius: parent.radius
                color: person.muted ? Theme.tertiaryText : Theme.on
            }
        }
        ValueSlider {
            from: -60; to: 20; stepSize: 1; decimals: 0; units: " dB"
            enabled: !person.muted && !person.blocked
            value: person.gain
            onMoved: person.nearby.setGain(person.index, value)
        }
        ResetButton {
            needed: Math.abs(person.gain) > 0.01
            onClicked: person.nearby.setGain(person.index, 0)
        }
        // mute: a small button, red while they're muted
        Rectangle {
            implicitWidth: muteLabel.implicitWidth + 16 * Theme.unit
            implicitHeight: 24 * Theme.unit
            radius: 6 * Theme.unit
            color: person.muted ? Theme.destructive : Theme.fill
            opacity: person.blocked ? 0.4 : 1
            Text {
                id: muteLabel
                anchors.centerIn: parent
                text: person.muted ? "Muted" : "Mute"
                color: person.muted ? "#ffffff" : Theme.text
                font.pixelSize: Theme.controlTextSize
            }
            MouseArea {
                anchors.fill: parent
                enabled: !person.blocked
                onClicked: person.nearby.setMuted(person.index, !person.muted)
            }
        }
    }

    // the actions, under the row
    ListRow {
        visible: person.actions && person.open
        Row {
            spacing: 8 * Theme.unit

            ActionButton {
                text: person.blocked ? "Unblock" : (person.confirming === "block" ? "Block them?" : "Block")
                danger: !person.blocked
                onPressed: {
                    if (person.blocked) {
                        person.nearby.setBlocked(person.index, false);
                    } else if (person.confirming === "block") {
                        person.nearby.setBlocked(person.index, true);
                        person.confirming = "";
                    } else {
                        person.confirming = "block";
                    }
                }
            }
            ActionButton {
                visible: person.nearby && person.nearby.canKick && !person.blocked
                text: person.confirming === "muteAll" ? "Mute for everyone?" : "Mute for Everyone"
                danger: true
                onPressed: {
                    if (person.confirming === "muteAll") {
                        person.nearby.muteForEveryone(person.index);
                        person.confirming = "";
                    } else {
                        person.confirming = "muteAll";
                    }
                }
            }
            ActionButton {
                visible: person.nearby && person.nearby.canKick
                text: person.confirming === "kick" ? "Kick them?" : "Kick"
                danger: true
                onPressed: {
                    if (person.confirming === "kick") {
                        person.nearby.kick(person.index);
                        person.confirming = "";
                        person.open = false;
                    } else {
                        person.confirming = "kick";
                    }
                }
            }
            ActionButton {
                visible: person.confirming !== ""
                text: "Cancel"
                onPressed: person.confirming = ""
            }
        }
    }
}
