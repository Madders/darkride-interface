//
//  PanelChooser.qml
//  qml/hifi/+android_interface
//
//  What the bar's Panels button opens (scripts/system/+android_interface/actionbar.js): Darkride's panels, as the
//  desktop's bar has them, each of which opens full-screen over the world (Panels, interface/src/android/PhonePanels.h).
//  The Settings panel is "World settings" here, so it isn't taken for the app's own Settings (the gear). In the
//  panels' colours, with the app theme's icon colour when it has one; a tap outside closes it.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.5
import QtGraphicalEffects 1.0
import "../../darkride/panel"

Item {
    id: chooser
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    // over the bar and the world's windows; under a panel (PanelHost)
    z: 2500
    visible: false

    signal sendToScript(var message);
    signal windowClosed();
    property bool shown: false   // (set as the bar's script makes it; it shows when the button asks)

    // the phone's dp, near enough: phones are about 360-430 dp tall in landscape
    readonly property real dp: Math.max(1, height / 400)
    // the app theme's icon colour, "" for the panels' own tint
    property string iconColor: ""

    readonly property var panels: [
        { name: "Settings", label: "World settings", icon: "settings" },
        { name: "Avatar", label: "Avatar", icon: "avatar" },
        { name: "People", label: "People", icon: "people" },
        { name: "Explore", label: "Explore", icon: "explore" },
        { name: "Radio", label: "Radio", icon: "radio" },
        { name: "Chat", label: "Chat", icon: "chat" }
    ]

    // Messages from the bar's script (sendToQml): {show, iconColor}
    function fromScript(message) {
        if (!message || message.show === undefined) {
            return;
        }
        if (message.iconColor !== undefined) {
            iconColor = message.iconColor;
        }
        chooser.visible = message.show;
    }

    function choose(name) {
        chooser.visible = false;
        Panels.open(name);
    }

    MouseArea {
        anchors.fill: parent
        onClicked: chooser.visible = false
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.45
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: grid.width + 32 * chooser.dp
        height: heading.height + grid.height + 44 * chooser.dp
        radius: 14 * chooser.dp
        color: Theme.background
        border.color: Theme.separator
        border.width: 1

        // a tap on the card isn't one outside it
        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: heading
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 14 * chooser.dp
            anchors.leftMargin: 18 * chooser.dp
            text: "Panels"
            color: Theme.text
            font.pixelSize: 20 * chooser.dp
            font.bold: true
        }

        Grid {
            id: grid
            anchors.top: heading.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 14 * chooser.dp
            columns: 3
            spacing: 8 * chooser.dp

            Repeater {
                model: chooser.panels

                Rectangle {
                    width: 108 * chooser.dp
                    height: 88 * chooser.dp
                    radius: 10 * chooser.dp
                    color: tileMouse.pressed ? Theme.cardPressed : Theme.card

                    Image {
                        id: icon
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 14 * chooser.dp
                        width: 32 * chooser.dp
                        height: width
                        sourceSize.width: width
                        sourceSize.height: height
                        source: "../../../icons/bar/" + modelData.icon + ".svg"
                        visible: false
                    }
                    ColorOverlay {
                        anchors.fill: icon
                        source: icon
                        color: chooser.iconColor !== "" ? chooser.iconColor : Theme.tint
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12 * chooser.dp
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 8 * chooser.dp
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: modelData.label
                        color: Theme.text
                        font.pixelSize: 13 * chooser.dp
                    }

                    MouseArea {
                        id: tileMouse
                        anchors.fill: parent
                        onClicked: chooser.choose(modelData.name)
                    }
                }
            }
        }
    }
}
