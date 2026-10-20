//
//  WorldLoadingScreen.qml
//
//  Darkride's loading screen, over the whole window while a hop or an arrival is under way: the world's picture,
//  blurred, behind everything; the picture itself on a rounded card; the world's name and its author; a thin bar
//  that glides; one short line saying what's happening. It fades in at once and fades out into the world.
//
//  It draws only; scripts/system/worldLoading.js decides, calling Window.setLoadingScreen({up: true|false, world,
//  author, picture: "<url>", status}), any field left out keeping its last value. (QML may not listen to the
//  Messages client, which lives on the network thread; Window lives on the main thread with this.) In a headset
//  the script shows its own card instead, and this stays hidden.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.15
import QtGraphicalEffects 1.0

Item {
    id: root
    anchors.fill: parent
    // over every window and dialog (the desktop's modal level is 4000), under its menus (8000)
    z: 7000

    // the sign-in screen's navy and darkride.ai's page colour (--dr-bg), so every hand-off between them is unseen
    readonly property color night: "#1c2136"

    property bool up: false
    property string world: ""
    property string author: ""
    property string picture: ""
    property string status: ""

    readonly property bool shown: up && !HMD.active
    opacity: shown ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity {
        NumberAnimation {
            duration: root.shown ? 250 : 500
            easing.type: Easing.InOutQuad
        }
    }

    // while it's up, the world behind it takes no clicks
    MouseArea {
        anchors.fill: parent
        enabled: root.shown
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: root.night
    }

    // the world's own colours: its picture, blurred and darkened, filling the window
    Image {
        id: backdrop
        anchors.fill: parent
        source: root.picture
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: 256
        asynchronous: true
        visible: false
    }
    FastBlur {
        anchors.fill: parent
        source: backdrop
        radius: 64
        visible: backdrop.status === Image.Ready
    }
    Rectangle {
        anchors.fill: parent
        color: root.night
        opacity: 0.55
    }
    RadialGradient {
        anchors.fill: parent
        horizontalRadius: width * 0.75
        verticalRadius: height * 0.85
        gradient: Gradient {
            GradientStop { position: 0.45; color: "transparent" }
            GradientStop { position: 1.0; color: "#8c000000" }
        }
    }

    Column {
        anchors.centerIn: parent
        width: card.width

        Item {
            id: card
            width: Math.min(root.width * 0.42, 560)
            height: Math.round(width * 2 / 3)
            visible: root.picture !== "" && cardImage.status !== Image.Error

            Image {
                id: cardImage
                anchors.fill: parent
                source: root.picture
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                mipmap: true
                visible: false
            }
            Rectangle {
                id: cardShape
                anchors.fill: parent
                radius: 22
                visible: false
            }
            OpacityMask {
                id: cardRounded
                anchors.fill: parent
                source: cardImage
                maskSource: cardShape
                visible: false
            }
            DropShadow {
                anchors.fill: cardRounded
                source: cardRounded
                verticalOffset: 18
                radius: 40
                samples: 81
                color: "#8c000000"
            }
            // a hairline, so a dark picture still has an edge
            Rectangle {
                anchors.fill: parent
                radius: 22
                color: "transparent"
                border.color: "#24ffffff"
                border.width: 1
            }
        }

        Item {
            width: 1
            height: card.visible ? 38 : 0
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: root.world
            visible: text !== ""
            color: "white"
            font.pixelSize: 40
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            topPadding: 6
            text: root.author !== "" ? "by " + root.author : ""
            visible: text !== ""
            color: "#99ffffff"
            font.pixelSize: 19
        }

        Item {
            width: 1
            height: 44
        }

        // a bar with a short light gliding along it, back and forth, while anything is under way
        Item {
            id: bar
            anchors.horizontalCenter: parent.horizontalCenter
            width: 300
            height: 5

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: "#2effffff"
            }
            Rectangle {
                id: glider
                width: 90
                height: parent.height
                radius: height / 2
                color: "#e6ffffff"

                SequentialAnimation on x {
                    running: root.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 0; to: bar.width - glider.width; duration: 1100; easing.type: Easing.InOutSine }
                    NumberAnimation { from: bar.width - glider.width; to: 0; duration: 1100; easing.type: Easing.InOutSine }
                }
            }
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            topPadding: 14
            text: root.status
            color: "#8cffffff"
            font.pixelSize: 15
        }
    }

    // Window.setLoadingScreen() from scripts/system/worldLoading.js: a field left out keeps its last value
    Connections {
        target: Window
        function onLoadingScreenChanged(state) {
            if (state.world !== undefined) {
                root.world = state.world || "";
            }
            if (state.author !== undefined) {
                root.author = state.author || "";
            }
            if (state.picture !== undefined) {
                root.picture = state.picture || "";
            }
            if (state.status !== undefined) {
                root.status = state.status || "";
            }
            if (state.up !== undefined) {
                root.up = !!state.up;
            }
        }
    }
}
