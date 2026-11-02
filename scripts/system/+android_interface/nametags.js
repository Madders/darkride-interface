"use strict";
//
//  nametags.js
//  scripts/system/+android_interface/
//
//  The phone's nametags: the desktop's own (tivoli-scripts' nametags.js, which the Android build compiles into
//  scripts/tivoli), without the rest of tivoli.js, whose buttons and overview are the desktop's. They follow World
//  settings > Displays > Nametags (Render.nametagsEnabled and nametagsShowSelf) as tivoli.js does there.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () { // BEGIN LOCAL_SCOPE

var initNametags = Script.require(Script.resolvePath("../../tivoli/nametags.js")).initNametags;

var nametags = null;
var enabled = false;
var showSelf = false;

function start() {
    try {
        nametags = initNametags(showSelf);
    } catch (error) {
        print("nametags.js: couldn't start the nametags: " + error);
        nametags = null;
    }
}

function stop() {
    if (nametags) {
        nametags.cleanup();
        nametags = null;
    }
}

function update() {
    var nowEnabled = Render.getNametagsEnabled();
    var nowShowSelf = Render.getNametagsShowSelf();
    if (nowEnabled === enabled && nowShowSelf === showSelf) {
        return;
    }
    stop();
    enabled = nowEnabled;
    showSelf = nowShowSelf;
    if (enabled) {
        start();
    }
}

update();
Render.settingsChanged.connect(update);

Script.scriptEnding.connect(function () {
    Render.settingsChanged.disconnect(update);
    stop();
});

}()); // END LOCAL_SCOPE
