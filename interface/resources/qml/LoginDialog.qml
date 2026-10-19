//
//  LoginDialog.qml
//
//  Created by David Rowe on 3 Jun 2015
//  Copyright 2015 High Fidelity, Inc.
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import Hifi 1.0
import QtQuick 2.4

import controlsUit 1.0 as HifiControlsUit
import stylesUit 1.0 as HifiStylesUit

FocusScope {
    id: root
    objectName: "LoginDialog"
    property bool shown: true
    visible: shown

    HifiStylesUit.HifiConstants { id: hifi }

    // laid out as if the window were at least 760 tall and 640 wide, then scaled down to fit it: in a small window the
    // banner and the form shrink together instead of the form running off the bottom (clicks and typing are scaled
    // with it)
    readonly property real fitScale: parent && parent.width > 0 && parent.height > 0 ?
        Math.min(1, parent.height / 760, parent.width / 640) : 1
    width: parent ? parent.width / fitScale : 0
    height: parent ? parent.height / fitScale : 0
    scale: fitScale
    transformOrigin: Item.TopLeft

    readonly property bool isTablet: false
    readonly property bool isOverlay: false

    property string iconText: ""
    property int iconSize: 50
    property bool keyboardEnabled: false
    property bool keyboardRaised: false
    property bool punctuationMode: false
    property bool isPassword: false

    property alias bannerWidth: banner.width
    property alias bannerHeight: banner.height

    property string title: ""
    property string text: ""

    property int titleWidth: 0

    property bool isHMD: HMD.active

    function tryDestroy() {
        root.destroy()
    }

    LoginDialog {
        id: loginDialog

        Loader {
            id: bodyLoader
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
            width: 500
            height: 150
            source: "../images/darkride-banner.png"
            horizontalAlignment: Image.AlignHCenter
        }
        // the dR mark above the wordmark, in the space above the banner
        Image {
            id: mark
            anchors {
                bottom: banner.top
                horizontalCenter: parent.horizontalCenter
            }
            height: Math.max(0, Math.min(140, bannerContainer.anchors.topMargin - 16))
            width: height * 390 / 330
            fillMode: Image.PreserveAspectFit
            source: "../images/darkride-mark.png"
            mipmap: true
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
            case Qt.Key_Escape:
            case Qt.Key_Back:
                event.accepted = true
                break

            case Qt.Key_Enter:
            case Qt.Key_Return:
                event.accepted = true
                break
        }
    }

    Component.onCompleted: {
        // a remembered account (Remember me) gets "Continue as <name>", anyone else the form
        bodyLoader.setSource("LoginDialog/" + (Account.loggedIn ? "ContinueBody.qml" : "LinkAccountBody.qml"), { "loginDialog": loginDialog, "root": root, "bodyLoader": bodyLoader, "linkSteam": false, "linkOculus": false });
    }
}
