"use strict";

//  avatarMenu.js
//
//  The Avatar Menu app: an imported avatar's menu, with its toggles, buttons and submenus. Its button is on the tablet while the avatar has a menu. Choices set the avatar's
//  parameters (MyAvatar.setAvatarParameter), which drive its effects layer: clothes shown or hidden, shape
//  keys, bones. Parameters the avatar marks as saved are remembered per avatar and put back the next time it's worn.
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT

/* globals Script, Tablet, MyAvatar, Settings, AnimationCache, Messages */

(function () {
    var APP_BASE = "html/AvatarMenuApp.html";
    var APP_URL = Script.resolvePath(APP_BASE);
    var APP_LABEL = "Avatar Menu";
    var APP_SORT_ORDER = 13;
    var SAVED_SETTING = "io.darkride.avatarMenu.saved";  // {avatar URL: {parameter: value}}

    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");
    var button = null;
    var onScreen = false;

    function savedValues() {
        var all = Settings.getValue(SAVED_SETTING, {}) || {};
        return all[MyAvatar.skeletonModelURL] || {};
    }

    function save(name, value) {
        var all = Settings.getValue(SAVED_SETTING, {}) || {};
        var mine = all[MyAvatar.skeletonModelURL] || {};
        mine[name] = value;
        all[MyAvatar.skeletonModelURL] = mine;
        Settings.setValue(SAVED_SETTING, all);
    }

    function hasMenu(menuData) {
        return menuData && menuData.menu && menuData.menu.controls && menuData.menu.controls.length > 0;
    }

    // Emotes: the avatar's emote layer picks one by a parameter's value (MyAvatar.getAvatarMenu().emotes). Its stock
    // animations aren't in the package, so the Emote app's clips stand in, matched by the state's name; the avatar's own
    // emote clips aren't played yet. Others see the emote in the avatar's joints, and hear of it on emote.js's channel.
    var EMOTE_CLIPS = [["wave", "Waving"], ["clap", "Clapping"], ["point", "Pointing"], ["cheer", "Cheering"],
        ["danc", "Dancing"], ["die", "Fall"], ["fall", "Fall"], ["cry", "Crying"], ["surprise", "Surprised"]];
    var EMOTE_FPS = 60;
    var EMOTE_CHANNEL = "tivoli.emote";
    var playing = null;  // {emote, timer}
    var emotes = [];  // the avatar's standing emotes with stock clips
    var emoteValues = {};  // parameter -> the value last seen
    function clipURL(clip) {
        return Script.resolvePath("assets/animations/" + clip + ".fbx");
    }
    EMOTE_CLIPS.forEach(function (entry) {
        AnimationCache.prefetch(clipURL(entry[1]));
    });
    function clipFor(emote) {
        var name = emote.name.toLowerCase();
        for (var i = 0; i < EMOTE_CLIPS.length; i++) {
            if (name.indexOf(EMOTE_CLIPS[i][0]) !== -1) {
                return EMOTE_CLIPS[i][1];
            }
        }
        return null;  // none of ours yet (a backflip, a sad kick)
    }
    function announce(clip, seconds) {
        Messages.sendMessage(EMOTE_CHANNEL, JSON.stringify({ emote: clip, seconds: seconds }), false);
    }
    function stopEmote() {
        if (playing) {
            if (playing.timer) {
                Script.clearTimeout(playing.timer);
            }
            playing = null;
            MyAvatar.restoreAnimation();
            announce(null, 0);
        }
    }
    function emoteParameterSet(name, value) {
        if (value === 0) {  // a toggle switched off, or a button let go: loops and held poses end, one-shots finish
            if (playing && (playing.emote.loop || playing.emote.hold)) {
                stopEmote();
            }
            return;
        }
        var emote = emotes.filter(function (e) {
            return e.parameter === name && e.value === value;
        })[0];
        var clip = emote && clipFor(emote);
        var animation = clip && AnimationCache.getAnimation(clipURL(clip));
        var frames = animation && animation.frames ? animation.frames.length : 0;
        if (!frames) {
            return;
        }
        if (playing && playing.timer) {
            Script.clearTimeout(playing.timer);
        }
        MyAvatar.overrideAnimation(clipURL(clip), EMOTE_FPS, emote.loop, 0, frames - 1);
        var seconds = emote.loop || emote.hold ? null : frames / EMOTE_FPS;  // a hold keeps the last pose until let go
        announce(clip, seconds);
        playing = { emote: emote, timer: null };
        if (seconds !== null) {
            var mine = playing;
            playing.timer = Script.setTimeout(function () {
                if (playing === mine) {
                    playing = null;
                    MyAvatar.restoreAnimation();
                }
            }, seconds * 1000);
        }
    }

    function sendState() {
        var menuData = MyAvatar.getAvatarMenu();
        var url = MyAvatar.skeletonModelURL;
        tablet.emitScriptEvent(JSON.stringify({
            app: "avatarMenu",
            type: "avatarMenu",
            menu: menuData.menu,
            parameters: menuData.parameters,
            folder: url.substring(0, url.lastIndexOf("/") + 1)  // the menu's icons sit beside the .fst
        }));
    }

    function onClicked() {
        if (onScreen) {
            tablet.gotoHomeScreen();
        } else {
            onScreen = true;
            tablet.gotoWebScreen(APP_URL);
        }
    }

    // The emote parameters are watched as well as set from the menu page, so whatever sets them plays the emote; the
    // page's own presses are taken at once, since a quick tap can come and go between two looks.
    var emoteWatch = Script.setInterval(function () {
        Object.keys(emoteValues).forEach(function (name) {
            var value = MyAvatar.getAvatarParameter(name);
            if (value !== emoteValues[name]) {
                emoteValues[name] = value;
                emoteParameterSet(name, value);
            }
        });
    }, 50);

    function onAvatarMenuChanged() {
        stopEmote();
        var menuData = MyAvatar.getAvatarMenu();
        emotes = (menuData.emotes || []).filter(function (emote) {
            return !emote.seated && !emote.own;
        });
        emoteValues = {};
        emotes.forEach(function (emote) {
            emoteValues[emote.parameter] = MyAvatar.getAvatarParameter(emote.parameter);
        });
        if (hasMenu(menuData)) {
            var saved = savedValues();
            menuData.parameters.forEach(function (parameter) {
                if (parameter.saved && saved[parameter.name] !== undefined) {
                    MyAvatar.setAvatarParameter(parameter.name, saved[parameter.name]);
                }
            });
            if (!button) {
                button = tablet.addButton({
                    icon: "icons/tablet-icons/avatar-i.svg",
                    activeIcon: "icons/tablet-icons/avatar-a.svg",
                    text: APP_LABEL,
                    sortOrder: APP_SORT_ORDER
                });
                button.clicked.connect(onClicked);
            }
        } else if (button) {
            if (onScreen) {
                tablet.gotoHomeScreen();
            }
            button.clicked.disconnect(onClicked);
            tablet.removeButton(button);
            button = null;
        }
        if (onScreen) {
            sendState();
        }
    }

    function onScreenChanged(type, url) {
        onScreen = type === "Web" && url.indexOf(APP_BASE) !== -1;
        if (button) {
            button.editProperties({ isActive: onScreen });
        }
    }

    function onWebEventReceived(event) {
        if (typeof event === "string") {
            try {
                event = JSON.parse(event);
            } catch (e) {
                return;
            }
        }
        if (!event || event.app !== "avatarMenu") {
            return;  // the tablet's web events reach every app
        }
        if (event.type === "ready") {
            sendState();
        } else if (event.type === "set") {
            MyAvatar.setAvatarParameter(event.name, event.value);
            if (emoteValues[event.name] !== undefined) {
                emoteValues[event.name] = MyAvatar.getAvatarParameter(event.name);
                emoteParameterSet(event.name, emoteValues[event.name]);
            }
            var parameter = MyAvatar.getAvatarMenu().parameters.filter(function (p) {
                return p.name === event.name;
            })[0];
            if (parameter && parameter.saved) {
                save(event.name, event.value);
            }
            sendState();
        }
    }

    MyAvatar.avatarMenuChanged.connect(onAvatarMenuChanged);
    tablet.screenChanged.connect(onScreenChanged);
    tablet.webEventReceived.connect(onWebEventReceived);
    Script.scriptEnding.connect(function () {
        MyAvatar.avatarMenuChanged.disconnect(onAvatarMenuChanged);
        tablet.screenChanged.disconnect(onScreenChanged);
        tablet.webEventReceived.disconnect(onWebEventReceived);
        Script.clearInterval(emoteWatch);
        stopEmote();
        if (onScreen) {
            tablet.gotoHomeScreen();
        }
        if (button) {
            button.clicked.disconnect(onClicked);
            tablet.removeButton(button);
        }
    });
    onAvatarMenuChanged();
}());
