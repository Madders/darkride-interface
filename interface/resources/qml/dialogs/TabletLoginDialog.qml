//
//  TabletLoginDialog.qml
//
//  Created by Vlad Stelmahovsky on 15 Mar 2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import Hifi 1.0
import QtQuick 2.5

import controlsUit 1.0 as HifiControlsUit
import stylesUit 1.0 as HifiStylesUit

import TabletScriptingInterface 1.0

import "../LoginDialog"
import "../LoginDialog" as LoginParts

FocusScope {
    id: root
    objectName: "LoginDialog"
    visible: true

    HifiStylesUit.HifiConstants { id: hifi }

    anchors.fill: parent

    readonly property bool isTablet: true
    readonly property bool isOverlay: false

    property string iconText: hifi.glyphs.avatar
    property int iconSize: 35
    property bool keyboardEnabled: false
    property bool keyboardRaised: false
    property bool punctuationMode: false
    property bool isPassword: false

    property alias bannerWidth: banner.width
    property alias bannerHeight: banner.height

    property int titleWidth: 0

    property bool isHMD: HMD.active

    // TABLET SPECIFIC PROPERTIES START //
    property alias text: loginKeyboard.mirroredText

    width: parent.width
    height: parent.height

    property var tabletProxy: Tablet.getTablet("com.highfidelity.interface.tablet.system")

    property bool gotoPreviousApp: false

    property bool keyboardOverride: true

    property var items;
    property string label: ""

    property alias bodyLoader: bodyLoader
    property alias loginDialog: loginDialog
    property alias hifi: hifi

    property var pane: QtObject {
        property real width: root.width
        property real height: root.height
    }

    MouseArea {
        width: root.width
        height: root.height
    }
    // TABLET SPECIFIC PROPERTIES END //

    function tryDestroy() {
            tabletProxy.gotoHomeScreen();
    }

    Timer {
        id: keyboardTimer
        repeat: false
        interval: 200

        onTriggered: {
            if (MenuInterface.isOptionChecked("Use 3D Keyboard") && root.isHMD) {
                KeyboardScriptingInterface.raised = true;
            }
        }
    }

    LoginDialog {
        id: loginDialog

        anchors.fill: parent
        Loader {
            id: bodyLoader
            anchors.fill: parent
        }
    }

    // Darkride's navy, the website's page colour (--dr-bg): the world doesn't show until someone has signed in
    Rectangle {
        z: -10
        id: loginDialogBackground
        color: "#1c2136"
        anchors.fill: parent
    }

    Rectangle {
        z: -6
        id: opaqueRect
        height: parent.height
        width: parent.width
        // opacity: 0.65
        opacity: 0
        color: "black"
    }

    Item {
        z: -5
        id: bannerContainer
        width: parent.width
        height: banner.height
        anchors {
            top: parent.top
            topMargin: 0.18 * parent.height
        }
        Image {
            id: banner
            anchors.centerIn: parent
            width: 400
            height: 120
            source: "../../images/darkride-banner.png"
            horizontalAlignment: Image.AlignHCenter
        }
        // the dR mark above the wordmark, in the space above the banner
        Image {
            id: mark
            anchors {
                bottom: banner.top
                horizontalCenter: parent.horizontalCenter
            }
            height: Math.max(0, Math.min(110, bannerContainer.anchors.topMargin - 16))
            width: height * 390 / 330
            fillMode: Image.PreserveAspectFit
            source: "../../images/darkride-mark.png"
            mipmap: true
        }
    }

    HifiControlsUit.Keyboard {
        id: loginKeyboard
        raised: root.keyboardEnabled && root.keyboardRaised
        numeric: root.punctuationMode
        password: root.isPassword
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
    }

    Keys.onPressed: {
        if (!visible) {
            return
        }

        if (event.modifiers === Qt.ControlModifier)
            switch (event.key) {
            case Qt.Key_A:
                event.accepted = true
                detailedText.selectAll()
                break
            case Qt.Key_C:
                event.accepted = true
                detailedText.copy()
                break
            case Qt.Key_Period:
                if (Qt.platform.os === "osx") {
                    event.accepted = true
                    content.reject()
                }
                break
        } else switch (event.key) {
            case Qt.Key_Enter:
            case Qt.Key_Return:
                event.accepted = true
                break
        }
    }

    Component.onDestruction: {
        loginKeyboard.raised = false;
        if (root.isHMD) {
            KeyboardScriptingInterface.raised = false;
        }
    }

    // the version and build along the bottom, for bug reports (LoginDialog/BuildLine.qml)
    LoginParts.BuildLine {
        z: 5
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 14
        }
    }

    Component.onCompleted: {
        keyboardTimer.start();
        // a remembered account (Remember me) gets "Continue as <name>", anyone else the form
        bodyLoader.setSource("../LoginDialog/" + (Account.loggedIn ? "ContinueBody.qml" : "LinkAccountBody.qml"), { "loginDialog": loginDialog, "root": root, "bodyLoader": bodyLoader });
    }
}
