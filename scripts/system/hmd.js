"use strict";

//
//  hmd.js
//  scripts/system/
//
//  Created by Howard Stearns on 2 Jun 2016
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
/* globals HMD, Script, Menu, Tablet, Camera */
/* eslint indent: ["error", 4, { "outerIIFEBody": 0 }] */

(function() { // BEGIN LOCAL_SCOPE

var headset; // The preferred headset: the one last used, else the first one found in the following list.
var displayMenuName = "Display";
var desktopMenuItemName = "Desktop";
// OpenXR's display is named "OpenXR"; without it in the list a Quest over Link had no Leave VR button, and no way back
// to the desktop from inside the headset (Caity's Quest 3 test, 10-28). The HMD Simulator (HIFI_DEBUG_HMD) is last.
var HEADSETS = ['OpenXR', 'Oculus Rift', 'HTC Vive', 'WindowMS', 'HMD Simulator'];
HEADSETS.forEach(function (name) {
    if (!headset && Menu.menuItemExists(displayMenuName, name)) {
        headset = name;
    }
});

var controllerDisplay = false;
function updateControllerDisplay() {
    if (HMD.active && Menu.isOptionChecked("Third Person")) {
        if (!controllerDisplay) {
            HMD.requestShowHandControllers();
            controllerDisplay = true;
        }
    } else if (controllerDisplay) {
        HMD.requestHideHandControllers();
        controllerDisplay = false;
    }
}

var button;
var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");

var switchToVR = "Enter VR";
var switchToDesktop = "Leave VR";

function onHmdChanged(isHmd) {
    HMD.closeTablet();
    if (isHmd) {
        // Enter VR goes back to this one
        HEADSETS.forEach(function (name) {
            if (Menu.menuItemExists(displayMenuName, name) && Menu.isOptionChecked(name)) {
                headset = name;
            }
        });
        button.editProperties({
            icon: "icons/tablet-icons/switch-desk-i.svg",
            text: switchToDesktop
        });
    } else {
        button.editProperties({
            icon: "icons/tablet-icons/switch-vr-i.svg",
            text: switchToVR
        });
    }
    updateControllerDisplay();
}

function onClicked() {
    var isDesktop = Menu.isOptionChecked(desktopMenuItemName);
    Menu.setIsOptionChecked(isDesktop ? headset : desktopMenuItemName, true);
}

if (headset) {
    button = tablet.addButton({
        icon: HMD.active ? "icons/tablet-icons/switch-desk-i.svg" : "icons/tablet-icons/switch-vr-i.svg",
        text: HMD.active ? switchToDesktop : switchToVR,
        sortOrder: 2
    });
    onHmdChanged(HMD.active);

    button.clicked.connect(onClicked);
    HMD.displayModeChanged.connect(onHmdChanged);
    Camera.modeUpdated.connect(updateControllerDisplay);

    Script.scriptEnding.connect(function () {
        button.clicked.disconnect(onClicked);
        if (tablet) {
            tablet.removeButton(button);
        }
        HMD.displayModeChanged.disconnect(onHmdChanged);
        Camera.modeUpdated.disconnect(updateControllerDisplay);
    });
}

}()); // END LOCAL_SCOPE
