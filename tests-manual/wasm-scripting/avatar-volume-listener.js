//
//  avatar-volume-listener.js
//  tests-manual/wasm-scripting
//
//  The listener of avatar-volume.sh: an agent 1 m from avatar-volume-speaker.js hears the audio mixer's mix
//  (Agent.takeHeardAudio) and keeps, for each volume the speaker announces, the loudest 100 ms of what it heard (RMS).
//  The last line, "AVATAR-VOLUME RESULT", says PASS when the tone at 0.25 is a quarter as loud as at 1 (0.15 to 0.35:
//  the mixer's codec and spatialising aren't exact) and the one at 0 is silent.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "avatar-volume";
    var WINDOW = 2400; // 100 ms at 24,000 Hz
    var label = null;
    var peaks = {};
    var pending = [];

    Agent.isAvatar = true;
    Agent.isListeningToAudioStream = true;
    Avatar.displayName = "avatar-volume listener";
    Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
    Avatar.position = { x: 71, y: 1, z: 70 };
    Avatar.orientation = Quat.IDENTITY;

    function log(text) {
        print("AVATAR-VOLUME " + text);
    }

    function report() {
        var full = peaks["1"] || 0, quarter = peaks["0.25"] || 0, none = peaks["0"] || 0;
        var ratio = full > 0 ? quarter / full : 0;
        var pass = full > 100 && ratio >= 0.15 && ratio <= 0.35 && none < full * 0.02;
        log("peaks (RMS) " + JSON.stringify(peaks) + ", 0.25 / 1 = " + ratio.toFixed(3));
        log("RESULT " + (pass ? "PASS" : "FAIL"));
    }

    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(function (channel, message) {
        if (channel !== CHANNEL) {
            return;
        }
        var m = JSON.parse(message);
        if (m.done) {
            Script.setTimeout(report, 1500);
            return;
        }
        if (m.volume === undefined) {
            return; // its own "listening", should it come back
        }
        label = String(m.volume);
        peaks[label] = 0;
        pending = [];
    });

    // until the speaker starts
    var calling = Script.setInterval(function () {
        if (label) {
            Script.clearInterval(calling);
        } else {
            Messages.sendMessage(CHANNEL, JSON.stringify({ listening: true }));
        }
    }, 1000);

    Script.setInterval(function () {
        var pcm = Agent.takeHeardAudio();
        if (!label || !pcm || pcm.byteLength < 2) {
            return;
        }
        var samples = new Int16Array(pcm);
        for (var i = 0; i < samples.length; i++) {
            pending.push(samples[i]);
        }
        while (pending.length >= WINDOW) {
            var sum = 0;
            for (var j = 0; j < WINDOW; j++) {
                sum += pending[j] * pending[j];
            }
            pending.splice(0, WINDOW);
            var rms = Math.sqrt(sum / WINDOW);
            if (rms > peaks[label]) {
                peaks[label] = Math.round(rms);
            }
        }
    }, 100);
}());
