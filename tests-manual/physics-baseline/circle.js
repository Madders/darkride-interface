//
//  circle.js
//  tests-manual/physics-baseline
//
//  Running in circles (GitHub #15: "the circle that the avatar runs slowly drifts in the direction of the turn"): the
//  avatar runs (sprint + forward) and turns at the keyboard's rate, as Shift + W + A/D do, through a controller mapping
//  (no real keys). Its position is sampled every 2 ms; the distinct positions are the game frames. Each lap (one turn of
//  the body's heading) gives a centre (the time-weighted mean of the lap's positions) and a radius; the drift is how far
//  the centre moves per lap, and which way. Phases: the camera mode ("look at", the desktop default, and "third
//  person") at a fixed frame rate (the display's, 60, about 30), and with a "heavy side": the frame rate drops while the
//  avatar faces +x (Test.setPresentLoad), as it would in a world with more to draw on one side. Then turns standing
//  still (A/D alone): the turn rate, how far the avatar slides, and how soon it stops after letting go.
//    run-circle.sh [path/to/Darkride.app] [results dir]
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//
(function () {
    var SETTLE = 3.0;        // s standing, then running before the recording (the turn and the speed ramp up)
    var RAMP = 3.0;
    var RECORD = 30.0;       // s recorded per phase: about 8 laps at 100 degrees a second
    var HEAVY_LOAD_US = 15000;
    var SLOW_LOAD_US = 31000;
    var PROFILE_REALTIME = 2, PROFILE_DISPLAY = 3;   // Performance.RefreshRateProfile
    var START = { x: 0, y: 1.0, z: 0 };
    var PHASES = [
        { name: "look at, display rate", camera: "look at", profile: PROFILE_DISPLAY, load: 0 },
        { name: "look at, 60", camera: "look at", profile: PROFILE_REALTIME, load: 0 },
        { name: "look at, ~30", camera: "look at", profile: PROFILE_REALTIME, load: SLOW_LOAD_US },
        { name: "look at, heavy side", camera: "look at", profile: PROFILE_DISPLAY, load: 0, heavy: HEAVY_LOAD_US },
        { name: "third person, display rate", camera: "third person", profile: PROFILE_DISPLAY, load: 0 },
        { name: "third person, ~30", camera: "third person", profile: PROFILE_REALTIME, load: SLOW_LOAD_US },
        { name: "third person, heavy side", camera: "third person", profile: PROFILE_DISPLAY, load: 0, heavy: HEAVY_LOAD_US },
        { name: "standing turn, display rate", camera: "look at", profile: PROFILE_DISPLAY, load: 0, standing: true },
        { name: "standing turn, ~30", camera: "look at", profile: PROFILE_REALTIME, load: SLOW_LOAD_US, standing: true }
    ];
    var STANDING_TURN = 3.0;   // s turning, recorded after the ramp
    var STANDING_STOP = 1.0;   // s recorded after letting go
    var drive = { forward: 0, yaw: 0, sprint: 0 };
    var mapping = Controller.newMapping("physics-circle-test");
    mapping.from(function () { return drive.forward; }).to(Controller.Actions.LONGITUDINAL_FORWARD);
    mapping.from(function () { return drive.yaw; }).to(Controller.Actions.Yaw);
    mapping.from(function () { return drive.sprint; }).to(Controller.Actions.SPRINT);
    var results = [];
    var t0 = Date.now();
    function log(s) { print("CIRCLE " + s); }
    function after(seconds, f) { Script.setTimeout(f, seconds * 1000); }
    function heading(q) {   // the body's yaw, radians about +y (0 facing -z)
        var f = Quat.getFront(q);
        return Math.atan2(-f.x, -f.z);
    }

    function runPhase(index) {
        if (index >= PHASES.length) { finish(); return; }
        var phase = PHASES[index];
        Camera.mode = phase.camera;
        Performance.refreshRateProfile = phase.profile;
        Test.setPresentLoad(phase.load);
        drive.forward = 0; drive.yaw = 0; drive.sprint = 0;
        MyAvatar.position = START;
        MyAvatar.orientation = Quat.IDENTITY;
        after(SETTLE, function () {
            if (phase.standing) {
                drive.yaw = 1;
                after(1.0, function () { standingTurn(phase, function () { runPhase(index + 1); }); });
                return;
            }
            drive.forward = 1; drive.sprint = 1; drive.yaw = 1;
            after(RAMP, function () { record(phase, function () { runPhase(index + 1); }); });
        });
    }

    function standingTurn(phase, next) {
        var samples = [];   // [ms, heading, x, z, game loop Hz], distinct headings or positions
        var start = Date.now();
        var released = 0;
        var timer = Script.setInterval(function () {
            var p = MyAvatar.position;
            var h = heading(MyAvatar.orientation);
            var last = samples[samples.length - 1];
            if (!last || last[1] !== h || last[2] !== p.x || last[3] !== p.z) {
                samples.push([Date.now() - start, h, p.x, p.z, Test.getGameLoopRate()]);
            }
            var t = Date.now() - start;
            if (!released && t > STANDING_TURN * 1000) {
                released = t;
                drive.yaw = 0;
            } else if (released && t > released + STANDING_STOP * 1000) {
                Script.clearInterval(timer);
                Test.setPresentLoad(0);
                results.push(summarizeStanding(phase, samples, released));
                next();
            }
        }, 2);
    }

    function summarizeStanding(phase, samples, released) {
        var turned = 0, turnedAfter = 0, lastTurn = released, moved = 0, fps = 0, frames = 0;
        for (var i = 1; i < samples.length; i++) {
            var d = samples[i][1] - samples[i - 1][1];
            while (d > Math.PI) { d -= 2 * Math.PI; }
            while (d < -Math.PI) { d += 2 * Math.PI; }
            if (samples[i][0] <= released) {
                turned += d;
                fps += samples[i][4]; frames++;
            } else {
                turnedAfter += d;
                if (d !== 0) { lastTurn = samples[i][0]; }
            }
            moved = Math.max(moved, Math.sqrt(Math.pow(samples[i][2] - samples[0][2], 2) +
                                              Math.pow(samples[i][3] - samples[0][3], 2)));
        }
        var turning = samples.length > 0 && released > samples[0][0] ? (released - samples[0][0]) / 1000 : 1;
        var summary = {
            name: phase.name, fps: fps / Math.max(1, frames),
            degreesPerSecond: Math.abs(turned) * 180 / Math.PI / turning,
            movedMm: 1000 * moved,
            degreesAfterRelease: Math.abs(turnedAfter) * 180 / Math.PI,
            stoppedAfterSeconds: (lastTurn - released) / 1000
        };
        log(summary.name + ": turns " + summary.degreesPerSecond.toFixed(1) + " deg/s (yaw speed " + MyAvatar.yawSpeed +
            ") at " + summary.fps.toFixed(0) + " fps; slid " + summary.movedMm.toFixed(0) + " mm; after letting go " +
            summary.degreesAfterRelease.toFixed(1) + " deg more, still after " + summary.stoppedAfterSeconds.toFixed(2) + " s");
        return summary;
    }

    function record(phase, next) {
        var samples = [];   // [ms, x, z, heading, game loop Hz]
        var start = Date.now();
        var loaded = false;
        var timer = Script.setInterval(function () {
            var p = MyAvatar.position;
            var h = heading(MyAvatar.orientation);
            if (phase.heavy) {
                var facingX = -Math.sin(h) > 0;   // the body's front has +x in it
                if (facingX !== loaded) { loaded = facingX; Test.setPresentLoad(loaded ? phase.heavy : 0); }
            }
            var last = samples[samples.length - 1];
            if (!last || last[1] !== p.x || last[2] !== p.z) {
                samples.push([Date.now() - start, p.x, p.z, h, Test.getGameLoopRate()]);
            }
            if (Date.now() - start > RECORD * 1000) {
                Script.clearInterval(timer);
                Test.setPresentLoad(0);
                drive.forward = 0; drive.yaw = 0; drive.sprint = 0;
                results.push(summarize(phase, samples));
                next();
            }
        }, 2);
    }

    function summarize(phase, samples) {
        // unwrap the heading; lap k holds the frames whose turned angle is in [k, k+1) turns
        var turned = 0, laps = [], lap = null;
        for (var i = 0; i < samples.length; i++) {
            if (i > 0) {
                var d = samples[i][3] - samples[i - 1][3];
                while (d > Math.PI) { d -= 2 * Math.PI; }
                while (d < -Math.PI) { d += 2 * Math.PI; }
                turned += d;
            }
            var k = Math.floor(Math.abs(turned) / (2 * Math.PI));
            if (!lap || lap.k !== k) { lap = { k: k, frames: [] }; laps.push(lap); }
            lap.frames.push(samples[i]);
        }
        var centres = [];
        laps.forEach(function (l) {
            if (l.k === Math.floor(Math.abs(turned) / (2 * Math.PI))) { return; }   // the last, unfinished lap
            var sx = 0, sz = 0, st = 0, fps = 0;
            for (var j = 1; j < l.frames.length; j++) {   // time-weighted: frames come unevenly when the rate changes
                var dt = l.frames[j][0] - l.frames[j - 1][0];
                sx += dt * l.frames[j][1]; sz += dt * l.frames[j][2]; st += dt;
                fps += l.frames[j][4];
            }
            if (st <= 0) { return; }
            var c = { x: sx / st, z: sz / st, seconds: st / 1000, frames: l.frames.length, fps: fps / (l.frames.length - 1) };
            var r = 0;
            for (j = 0; j < l.frames.length; j++) {
                r += Math.sqrt(Math.pow(l.frames[j][1] - c.x, 2) + Math.pow(l.frames[j][2] - c.z, 2));
            }
            c.radius = r / l.frames.length;
            centres.push(c);
        });
        var drifts = [];
        for (var n = 1; n < centres.length; n++) {
            drifts.push({ x: centres[n].x - centres[n - 1].x, z: centres[n].z - centres[n - 1].z });
        }
        var dx = 0, dz = 0, mag = 0;
        drifts.forEach(function (d) { dx += d.x; dz += d.z; mag += Math.sqrt(d.x * d.x + d.z * d.z); });
        var count = Math.max(1, drifts.length);
        var mean = { x: dx / count, z: dz / count };
        var lapSeconds = 0, radius = 0, fps = 0;
        centres.forEach(function (c) { lapSeconds += c.seconds; radius += c.radius; fps += c.fps; });
        var summary = {
            name: phase.name, laps: centres.length, frames: samples.length,
            fps: fps / Math.max(1, centres.length), lapSeconds: lapSeconds / Math.max(1, centres.length),
            radius: radius / Math.max(1, centres.length),
            driftPerLapMm: 1000 * Math.sqrt(mean.x * mean.x + mean.z * mean.z),
            driftDirectionDeg: Math.atan2(mean.x, -mean.z) * 180 / Math.PI,
            wanderPerLapMm: 1000 * mag / count,
            centres: centres
        };
        log(summary.name + ": " + summary.laps + " laps of " + summary.lapSeconds.toFixed(2) + " s at " +
            summary.fps.toFixed(0) + " fps, radius " + summary.radius.toFixed(3) + " m; the centre moves " +
            summary.driftPerLapMm.toFixed(0) + " mm a lap on average, toward " + summary.driftDirectionDeg.toFixed(0) +
            " deg (0 = -z, 90 = +x); " + summary.wanderPerLapMm.toFixed(0) + " mm a lap lap to lap");
        return summary;
    }

    function finish() {
        mapping.disable();
        Performance.refreshRateProfile = PROFILE_DISPLAY;
        Test.saveObject({ displayRefreshRate: Test.getDisplayRefreshRate(), yawSpeed: MyAvatar.yawSpeed,
                          results: results }, "circle.json");
        log("DONE");
        after(1, function () { Test.quit(); });
    }

    mapping.enable();
    log("display " + Test.getDisplayRefreshRate().toFixed(0) + " Hz, yaw speed " + MyAvatar.yawSpeed + " deg/s");
    after(2, function () { runPhase(0); });
    Script.scriptEnding.connect(function () { mapping.disable(); Test.setPresentLoad(0); });
}());
