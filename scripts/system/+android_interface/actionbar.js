"use strict";
//
//  backbutton.js
//  scripts/system/+android
//
//  Created by Gabriel Calero & Cristian Duarte on Apr 06, 2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
(function() { // BEGIN LOCAL_SCOPE

var actionbar;
var backButton;
var settingsButton;

var logEnabled = true;

function printd(str) {
    if (logEnabled)
        print("[actionbar.js] " + str);
}

// A theme's own icon for a slot (the app's Settings > Theme), or the default
function themedIcon(slot, fallback) {
    var icons = (Settings.getValue("Android/themeIcons", "") || "").split(",");
    return icons.indexOf(slot) >= 0 ? Settings.getValue("Android/themeIconFolder", "") + "/" + slot + ".png" : fallback;
}

// The theme's colour for a default icon ("" for a theme's own icon, which is coloured already)
function themedIconColor(slot, colorKey) {
    var icons = (Settings.getValue("Android/themeIcons", "") || "").split(",");
    return icons.indexOf(slot) >= 0 ? "" : Settings.getValue(colorKey, "");
}

function init() {
    actionbar = new QmlFragment({
        qml: "hifi/ActionBar.qml"
    });
    backButton = actionbar.addButton({
        icon: themedIcon("back", "icons/+android_interface/backward.svg"),
        activeIcon: themedIcon("back", "icons/+android_interface/backward.svg"),
        text: "",
        iconColor: themedIconColor("back", "Android/themeIconColor"),
        bgOpacity: 0.0,
        hoverBgOpacity: 0.0,
        activeBgOpacity: 0.0
    });
    
    backButton.entered.connect(onBackPressed);
    backButton.clicked.connect(onBackClicked);

    // Settings, over the world (InterfaceActivity's settings panel)
    settingsButton = actionbar.addButton({
        icon: themedIcon("settings", "icons/+android_interface/settings.svg"),
        activeIcon: themedIcon("settings", "icons/+android_interface/settings.svg"),
        text: "",
        iconColor: themedIconColor("settings", "Android/themeIconColor"),
        bgOpacity: 0.0,
        hoverBgOpacity: 0.0,
        activeBgOpacity: 0.0
    });
    settingsButton.entered.connect(onBackPressed);
    settingsButton.clicked.connect(onSettingsClicked);
}

function onSettingsClicked() {
   Window.openAndroidActivity("Settings", true);
}

function onBackPressed() {
   Controller.triggerHapticPulseOnDevice(Controller.findDevice("TouchscreenVirtualPad"), 0.1, 40.0, 0);
}

function onBackClicked() {
   Window.openAndroidActivity("Home", false);
}


Script.scriptEnding.connect(function() {
    if(backButton) {
        backButton.entered.disconnect(onBackPressed);
        backButton.clicked.disconnect(onBackClicked);
    }
    if (settingsButton) {
        settingsButton.entered.disconnect(onBackPressed);
        settingsButton.clicked.disconnect(onSettingsClicked);
    }
});

init();

}()); // END LOCAL_SCOPE
