"use strict";
//
//  audio.js
//  scripts/system/
//
//  Created by Gabriel Calero & Cristian Duarte on Jan 16, 2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() { // BEGIN LOCAL_SCOPE

var audiobar;
var audioButton;

var logEnabled = false;

function printd(str) {
    if (logEnabled)
        print("[audio.js] " + str);
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
    audiobar = new QmlFragment({
        qml: "hifi/AudioBar.qml"
    });

    audioButton = audiobar.addButton({
        text: "",
        bgOpacity: 0.0,
        hoverBgOpacity: 0.0,
        activeHoverBgOpacity: 0.0,
        activeBgOpacity: 0.0
    });

    applyTheme();
    themeTimer = Script.setInterval(checkTheme, THEME_CHECK_MSECS);
    onMuteToggled();

    audioButton.clicked.connect(onMuteClicked);
    audioButton.entered.connect(onMutePressed);
    Audio.mutedChanged.connect(onMuteToggled);
}

// The app theme's icons and colours (Settings > Theme), and again when the theme changes in the app's menus
var THEME_CHECK_MSECS = 2000;
var themeTimer;
var appliedTheme;

function currentTheme() {
    return Settings.getValue("Android/themeIconFolder", "") + "|" + Settings.getValue("Android/themeIcons", "") + "|" +
        Settings.getValue("Android/themeIconColor", "") + "|" + Settings.getValue("Android/themeAlertColor", "");
}

// (through the bar's QML, on its own thread: the button is a QML object, which can't be changed from this one)
function applyTheme() {
    appliedTheme = currentTheme();
    audiobar.sendToQml({ buttons: [{
        icon: themedIcon("mic", "icons/mic-unmute-a.svg"),
        activeIcon: themedIcon("mic-muted", "icons/mic-mute-a.svg"),
        iconColor: themedIconColor("mic", "Android/themeIconColor"),
        activeIconColor: themedIconColor("mic-muted", "Android/themeAlertColor")
    }] });
}

function checkTheme() {
    if (currentTheme() !== appliedTheme) {
        applyTheme();
    }
}

function onMuteClicked() {
    Audio.muted = !Audio.muted;
}

function onMutePressed() {
    Controller.triggerHapticPulseOnDevice(Controller.findDevice("TouchscreenVirtualPad"), 0.1, 40.0, 0);
}

function onMuteToggled() {
    printd("On Mute Toggled");
    // (through the bar's QML, on its own thread)
    audiobar.sendToQml({ buttons: [{ isActive: Audio.muted }] });
}

Script.scriptEnding.connect(function () {
    if (themeTimer) {
        Script.clearInterval(themeTimer);
    }
    if(audioButton) {
        audioButton.clicked.disconnect(onMuteClicked);
        audioButton.entered.disconnect(onMutePressed);
        Audio.mutedChanged.connect(onMuteToggled);
    }
});

init();

}()); // END LOCAL_SCOPE
