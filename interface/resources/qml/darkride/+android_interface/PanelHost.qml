//
//  PanelHost.qml
//  qml/darkride/+android_interface
//
//  A Darkride panel on the phone (interface/src/android/PhonePanels.h): full-screen over the world, with a close box
//  beside the page's large title. The panels were laid out for desktop windows about 560 px wide, so here they're
//  scaled up to a little under the phone's own dp for fingers, and reflow to the width that leaves. While it shows it
//  takes every touch, so none reaches the world under it.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import "../panel"

Item {
    id: host
    objectName: "PhonePanelHost"
    anchors.fill: parent
    // over the bar, the world's windows and the loading screen; under modal dialogs (Desktop's zLevels.modal)
    z: 3000
    visible: false

    // the panel showing, by its name in Panels
    property string panelName: ""
    readonly property Item page: loader.item
    // the screen's pixels per Android dp (PhonePanels sets it)
    property real density: 2.5
    // desktop pixels to the phone's: a little under its dp, so a desktop window's height of rows fits
    readonly property real zoom: Math.max(1, Math.min(density * 0.85, height / 360))
    // room at the sides for a camera cutout
    readonly property real sideInset: 12

    // a page closing itself (Page.closePanel: Explore, for a hop) looks for these
    property bool shown: false
    signal windowClosed()
    onWindowClosed: Panels.close(panelName)

    function showPanel(name, source) {
        if (panelName !== name) {
            loader.source = "";   // the last panel goes first: one at a time
            panelName = name;
            loader.source = source;
        }
        shown = true;
        visible = true;
    }

    function hidePanel() {
        shown = false;
        visible = false;
    }

    // every touch while it shows
    MouseArea {
        anchors.fill: parent
        onWheel: wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    Item {
        id: frame
        width: host.width / host.zoom
        height: host.height / host.zoom
        scale: host.zoom
        transformOrigin: Item.TopLeft

        Loader {
            id: loader
            anchors.fill: parent
            anchors.leftMargin: host.sideInset
            anchors.rightMargin: host.sideInset
            onStatusChanged: {
                if (status === Loader.Error) {
                    console.warn("PanelHost: couldn't make", host.panelName, source);
                }
            }
        }

        Rectangle {
            id: closeBox
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 12
            anchors.rightMargin: host.sideInset + 14
            width: 36
            height: 36
            radius: width / 2
            color: closeMouse.pressed ? Theme.cardPressed : Theme.fill
            z: 10

            Repeater {
                model: [45, -45]
                Rectangle {
                    anchors.centerIn: parent
                    width: 16
                    height: 2
                    radius: 1
                    rotation: modelData
                    color: Theme.text
                }
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                anchors.margins: -8   // a finger's worth around it
                onClicked: Panels.close(host.panelName)
            }
        }
    }
}
