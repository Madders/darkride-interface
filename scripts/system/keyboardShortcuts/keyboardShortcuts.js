"use strict";

//
//  keyboardShortcuts.js
//  scripts/system/keyboardShortcuts
//
//  Created by Preston Bezos on 06/28/2019
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function () { // BEGIN LOCAL_SCOPE 
    var snapActivateSound = SoundCache.getSound(Script.resourcesPath() + "sounds/snapshot/snap.wav");
    // Ctrl+N: personal space on or off. (High Fidelity's Ctrl+B web browser window is gone: Ctrl+B is Build mode, Caity
    // 10-29)
    function keyPressEvent(event) {
        if (event.text.toUpperCase() === "N" && event.isControl && !event.isShifted && !event.isAlt && !event.isCommand) {
            Users.toggleIgnoreRadius();
        }
    }

    function scriptEnding() {
        Controller.keyPressEvent.disconnect(keyPressEvent);
    }

    Controller.keyPressEvent.connect(keyPressEvent);
    Script.scriptEnding.connect(scriptEnding);
}()); // END LOCAL_SCOPE
