//
//  avatar-volume-speaker.js
//  tests-manual/wasm-scripting
//
//  The speaker of avatar-volume.sh: an agent plays the same one-second tone with Agent.playAvatarSound at volume 1,
//  0.25 and 0, three seconds apart, once avatar-volume-listener.js says it's listening (the domain may start either agent
//  first), and tells it each volume on the "avatar-volume" channel as it starts.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "avatar-volume";
    var VOLUMES = [1, 0.25, 0];
    var tone = SoundCache.getSound(Script.resolvePath("results/avatar-volume/tone-1s.wav"));
    var step = -1;
    var listening = false;

    Agent.isAvatar = true;
    Avatar.displayName = "avatar-volume speaker";
    Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
    Avatar.position = { x: 70, y: 1, z: 70 };
    Avatar.orientation = Quat.IDENTITY;

    function next() {
        step++;
        if (step >= VOLUMES.length) {
            Messages.sendMessage(CHANNEL, JSON.stringify({ done: true }));
            return;
        }
        Messages.sendMessage(CHANNEL, JSON.stringify({ volume: VOLUMES[step] }));
        Agent.playAvatarSound(tone, VOLUMES[step]);
        print("AVATAR-VOLUME speaker plays at " + VOLUMES[step]);
        Script.setTimeout(next, 3000);
    }

    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(function (channel, message) {
        if (channel === CHANNEL && message.indexOf("\"listening\"") >= 0) {
            listening = true;
        }
    });

    // once the listener is in and the tone has downloaded
    function start() {
        if (listening && tone.downloaded) {
            next();
        } else {
            Script.setTimeout(start, 500);
        }
    }
    Script.setTimeout(start, 5000);
}());
