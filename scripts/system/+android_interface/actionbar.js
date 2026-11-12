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
var panelsButton;
var panelChooser;

// the buttons' size in the screen's pixels, and the icon centred in it (the label is empty)
var BUTTON_SIZE = 170;
var ICON_SIZE = 110;
var ICON_BOTTOM = 22;

var logEnabled = true;

function printd(str) {
    if (logEnabled)
        print("[actionbar.js] " + str);
}

// A theme's own icon for a slot (the app's Settings > Theme), or the default
function themedIcon(slot, fallback) {
    var icons = (Settings.getValue("Android/themeIcons", "") || "").split(",");
    if (icons.indexOf(slot) < 0) {
        return fallback;
    }
    // (an installed theme's folder is a path in the app's files)
    var folder = Settings.getValue("Android/themeIconFolder", "");
    return (folder.charAt(0) === "/" ? "file://" : "") + folder + "/" + slot + ".png";
}

// The theme's colour for a default icon ("" for a theme's own icon, which is coloured already)
function themedIconColor(slot, colorKey) {
    var icons = (Settings.getValue("Android/themeIcons", "") || "").split(",");
    return icons.indexOf(slot) >= 0 ? "" : Settings.getValue(colorKey, "");
}

function init() {
    actionbar = QmlFragment.create({
        qml: "hifi/ActionBar.qml"
    });
    backButton = actionbar.addButton({
        text: "",
        width: BUTTON_SIZE,
        height: BUTTON_SIZE,
        iconSize: ICON_SIZE,
        textSize: 1,
        bottomMargin: ICON_BOTTOM,
        bgOpacity: 0.0,
        hoverBgOpacity: 0.0,
        activeBgOpacity: 0.0
    });
    
    backButton.entered.connect(onBackPressed);
    backButton.clicked.connect(onBackClicked);

    // Settings, over the world (InterfaceActivity's settings panel)
    settingsButton = actionbar.addButton({
        text: "",
        width: BUTTON_SIZE,
        height: BUTTON_SIZE,
        iconSize: ICON_SIZE,
        textSize: 1,
        bottomMargin: ICON_BOTTOM,
        bgOpacity: 0.0,
        hoverBgOpacity: 0.0,
        activeBgOpacity: 0.0
    });
    settingsButton.entered.connect(onBackPressed);
    settingsButton.clicked.connect(onSettingsClicked);

    // Darkride's panels (World settings, Avatar, People, Explore, Radio, Chat), chosen from a card over the world
    panelsButton = actionbar.addButton({
        text: "",
        width: BUTTON_SIZE,
        height: BUTTON_SIZE,
        iconSize: ICON_SIZE,
        textSize: 1,
        bottomMargin: ICON_BOTTOM,
        bgOpacity: 0.0,
        hoverBgOpacity: 0.0,
        activeBgOpacity: 0.0
    });
    panelsButton.entered.connect(onBackPressed);
    panelsButton.clicked.connect(onPanelsClicked);
    panelChooser = QmlFragment.create({
        qml: "hifi/PanelChooser.qml"
    });

    applyTheme();
    themeTimer = Script.setInterval(checkTheme, THEME_CHECK_MSECS);
}

// The app theme's icons and colours on the buttons, and again when the theme changes in the app's menus
var THEME_CHECK_MSECS = 2000;
var themeTimer;
var appliedTheme;

function currentTheme() {
    return Settings.getValue("Android/themeIconFolder", "") + "|" + Settings.getValue("Android/themeIcons", "") + "|" +
        Settings.getValue("Android/themeIconColor", "");
}

function themedButton(slot, icon) {
    var image = themedIcon(slot, icon);
    return { icon: image, activeIcon: image, iconColor: themedIconColor(slot, "Android/themeIconColor") };
}

// (through the bar's QML, on its own thread: the buttons are QML objects, which can't be changed from this one)
function applyTheme() {
    appliedTheme = currentTheme();
    actionbar.sendToQml({ buttons: [
        themedButton("back", "icons/+android_interface/backward.svg"),
        themedButton("settings", "icons/+android_interface/settings.svg"),
        themedButton("panels", "icons/+android_interface/panels.svg")
    ] });
}

function checkTheme() {
    if (currentTheme() !== appliedTheme) {
        applyTheme();
    }
}

function onSettingsClicked() {
   Window.openAndroidActivity("Settings", true);
}

function onPanelsClicked() {
    panelChooser.sendToQml({ show: true, iconColor: Settings.getValue("Android/themeIconColor", "") });
}

function onBackPressed() {
   Controller.triggerHapticPulseOnDevice(Controller.findDevice("TouchscreenVirtualPad"), 0.1, 40.0, 0);
}

function onBackClicked() {
   Window.openAndroidActivity("Home", false);
}


Script.scriptEnding.connect(function() {
    if (themeTimer) {
        Script.clearInterval(themeTimer);
    }
    if(backButton) {
        backButton.entered.disconnect(onBackPressed);
        backButton.clicked.disconnect(onBackClicked);
    }
    if (settingsButton) {
        settingsButton.entered.disconnect(onBackPressed);
        settingsButton.clicked.disconnect(onSettingsClicked);
    }
    if (panelsButton) {
        panelsButton.entered.disconnect(onBackPressed);
        panelsButton.clicked.disconnect(onPanelsClicked);
    }
});

init();

}()); // END LOCAL_SCOPE
