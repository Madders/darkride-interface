//
//  avatar-sound.js
//  tests-manual/wasm-scripting
//
//  The stand-in agent of avatar-sound.sh: an AI resident's recorded lines, played from its body. It plays a 1 s line to
//  its end, cuts a 3 s line short with Agent.stopAvatarSound, cuts one short by playing another over it, and plays a
//  line that can't be loaded, then stops being an avatar in the middle of a line, and prints each
//  Agent.avatarSoundFinished with the time since its line started. The last line, "AVATAR-SOUND RESULT", says PASS
//  when the six events came as expected: true, false, false, true, false, false.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var BASE = Script.resolvePath("results/avatar-sound/");
    var EXPECTED = [true, false, false, true, false, false];
    var oneSecond = SoundCache.getSound(BASE + "tone-1s.wav");
    var threeSeconds = SoundCache.getSound(BASE + "tone-3s.wav");
    var missing = SoundCache.getSound(BASE + "missing.wav");
    var events = [];
    var started = 0;
    var waiting = 0;  // the finished events the step under way still expects
    var step = 0;

    function log(text) {
        print("AVATAR-SOUND " + text);
    }

    function play(sound, label) {
        started = Date.now();
        log("play " + label);
        Agent.playAvatarSound(sound);
    }

    var steps = [
        function () {
            waiting = 1;
            play(oneSecond, "1 s to its end");
        },
        function () {
            waiting = 1;
            play(threeSeconds, "3 s, stopped after 1 s");
            Script.setTimeout(function () { Agent.stopAvatarSound(); }, 1000);
        },
        function () {
            waiting = 2;
            play(threeSeconds, "3 s, replaced after 0.5 s by 1 s");
            Script.setTimeout(function () { play(oneSecond, "1 s over it"); }, 500);
        },
        function () {
            waiting = 1;
            play(missing, "a line that can't be loaded");
        },
        function () {
            waiting = 1;
            play(threeSeconds, "3 s, no longer an avatar after 0.5 s");
            Script.setTimeout(function () { Agent.isAvatar = false; }, 500);
        }
    ];

    function next() {
        if (step < steps.length) {
            steps[step++]();
            return;
        }
        var pass = JSON.stringify(events) === JSON.stringify(EXPECTED);
        log("RESULT " + JSON.stringify(events) + " " + (pass ? "PASS" : "FAIL, expected " + JSON.stringify(EXPECTED)));
    }

    Agent.avatarSoundFinished.connect(function (completed) {
        events.push(completed);
        log("finished completed=" + completed + " after " + (Date.now() - started) + " ms; still playing " +
            Agent.isPlayingAvatarSound);
        waiting--;
        if (waiting === 0) {
            Script.setTimeout(next, 200);
        }
    });

    Agent.isAvatar = true;
    function whenLoaded() {
        if (oneSecond.downloaded && threeSeconds.downloaded) {
            log("lines loaded");
            next();
        } else {
            Script.setTimeout(whenLoaded, 200);
        }
    }
    whenLoaded();
}());
