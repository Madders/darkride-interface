//
//  track.js
//  tests-manual/physics-baseline
//
//  Round 4 of PHYSICS.md: the monorail on a Track entity. A closed, stadium-shaped track with two stations, a
//  cabin that is a frame and rides the track (trackID), and the avatar on it: it steps on from the platform at the
//  first station while the cabin stands, rides a lap (accelerating, cruising along the straights and round the
//  curves, braking, standing at the far station) and steps off onto the platform when the cabin is back. Every
//  script frame records the cabin's transform and velocities, the avatar's world position, its position in the
//  cabin's frame and the engine's stored local position. The summary (lines starting with PHYSICS-TRACK) is per
//  phase of the ride, the phases read off the cabin's own motion: standing, accelerating, cruising, curve, braking.
//
//  Serverless (run.sh <app> <results> track) or on a domain with observer.js on a second client (run-pair.sh
//  <app> <results> <port> track): the phase in progress is published in the cabin's description, as in round 3,
//  and the observer steps off when it reads "step-off".
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var SPEED = 5.0;              // m/s, the schedule's cruise
    var ACCELERATION = 2.0;       // m/s^2
    var DWELL = 10.0;             // s at each station
    var WALK_SPEED = 1.5;         // m/s, the scripted motor
    var CABIN_TOP = 0.5;          // the cabin floor's top surface, m
    var CABIN_Y = CABIN_TOP - 0.25;
    var ABOARD_X = 1.0;           // where to stop inside the cabin, in its coordinates: through the side door
    var LIFETIME = 600;           // s; on a domain the test's entities expire on their own
    var TIMEOUT = 170;            // s, from the cabin's creation (the world can take 90 s to load on a cold cache)
    var MAX_WAIT = 300;           // s, for the world to arrive at all
    var cabinT0 = -1;

    // the loop: straights of 60 m along z at x = 0 (the station side) and x = -30, half circles of 15 m at the ends;
    // the cabin runs toward -z on the station side; stations at points 2 (the platform) and 10 (the far side). A
    // Catmull-Rom segment is only straight when the points either side of it are in line too, hence the points at
    // +-15 m: the 30 m through each station is straight, and the segments beyond bend gently into the curve.
    var STATION_POINT = 2;
    var FAR_STATION_POINT = 10;
    var TRACK_POINTS = [
        { x: 0, y: 0, z: 30 }, { x: 0, y: 0, z: 15 }, { x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: -15 }, { x: 0, y: 0, z: -30 },
        { x: -4.4, y: 0, z: -40.6 }, { x: -15, y: 0, z: -45 }, { x: -25.6, y: 0, z: -40.6 },
        { x: -30, y: 0, z: -30 }, { x: -30, y: 0, z: -15 }, { x: -30, y: 0, z: 0 }, { x: -30, y: 0, z: 15 }, { x: -30, y: 0, z: 30 },
        { x: -25.6, y: 0, z: 40.6 }, { x: -15, y: 0, z: 45 }, { x: -4.4, y: 0, z: 40.6 }
    ];
    var TRACK_BANK = [0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 6, 0];   // degrees: a little bank at the apex of each curve
    var TRACK_DWELL = [0, 0, DWELL, 0, 0, 0, 0, 0, 0, 0, DWELL, 0, 0, 0, 0, 0];
    var TURNING = 0.1;            // rad/s: a bend tighter than 50 m at the cruise counts as a curve
    // the platform stands beside the cabin at the station (the track runs through where a platform behind it would be),
    // and the cabin has a door in its +x wall; the observer reads these from the cabin's userData
    var PLATFORM_SPOT = { x: 3.8, y: 1.3, z: 0 };
    var BOARDING = { spot: PLATFORM_SPOT, direction: { x: -1, y: 0, z: 0 }, axis: "x", limit: ABOARD_X, off: { x: 1, y: 0, z: 0 } };

    var track = null;
    var cabin = null;
    var walls = [];
    var platform = null;
    var samples = [];
    var events = [];
    var mode = "boot";            // boot, boarding, riding, step-off, ground, end
    var modeT0 = 0;
    var motion = "standing";      // read off the cabin: standing, accelerating, cruising, curve, braking
    var published = "";
    var leftStation = false;
    var lastSpeed = 0;
    var t0 = Date.now() / 1000;

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print("PHYSICS-TRACK " + s); }
    function setMode(m) {
        mode = m; modeT0 = now();
        events.push({ t: now(), mode: m });
        log("mode " + m + " at " + now().toFixed(2) + " s");
        publish();
    }
    function publish() {
        // the observer on the other client reads this; it steps off when it reads "step-off"
        var text = (mode === "riding" || mode === "boarding") ? motion : mode;
        if (cabin && text !== published) {
            published = text;
            Entities.editEntity(cabin, { description: text });
        }
    }

    function ensureWorld() {
        var origin = { x: 0, y: 0, z: 0 };
        if (Entities.findEntitiesByName("Ground", origin, 1000).length === 0) {
            Entities.addEntity({
                type: "Box", name: "Ground", shapeType: "box", locked: false, lifetime: LIFETIME,
                position: { x: 0, y: -0.05, z: 0 }, dimensions: { x: 400, y: 0.1, z: 400 },
                color: { red: 90, green: 110, blue: 90 }, grab: { grabbable: false }
            });
            log("ground created");
        }
        if (Entities.findEntitiesByName("BaselineZone", origin, 1000).length === 0) {
            Entities.addEntity({
                type: "Zone", name: "BaselineZone", shapeType: "box", lifetime: LIFETIME,
                position: origin, dimensions: { x: 1000, y: 1000, z: 1000 },
                keyLightMode: "enabled", keyLight: { color: { red: 255, green: 255, blue: 255 }, intensity: 1.0,
                    direction: { x: 0.5, y: -0.7, z: -0.5 } },
                ambientLightMode: "enabled", ambientLight: { ambientIntensity: 0.5 },
                skyboxMode: "enabled", skybox: { color: { red: 60, green: 80, blue: 120 } }
            });
            log("zone created");
        }
        platform = Entities.addEntity({
            type: "Box", name: "baseline-platform", shapeType: "box", lifetime: LIFETIME,
            position: { x: 3.5, y: CABIN_Y, z: 0 }, dimensions: { x: 3, y: 0.5, z: 4 },
            color: { red: 140, green: 140, blue: 150 }, dynamic: false, collisionless: false, grab: { grabbable: false }
        });
    }

    function spawn() {
        // leftovers of an earlier run (their lifetimes have not run out yet) would confuse everyone's search by name
        ["baseline-cabin", "baseline-track", "baseline-platform", "baseline-wall"].forEach(function (name) {
            Entities.findEntitiesByName(name, { x: 0, y: 0, z: 0 }, 1000).forEach(function (id) { Entities.deleteEntity(id); });
        });
        ensureWorld();
        track = Entities.addEntity({
            type: "Track", name: "baseline-track", lifetime: LIFETIME,
            position: { x: -15, y: CABIN_Y, z: 0 }, dimensions: { x: 40, y: 1, z: 100 },
            trackPoints: TRACK_POINTS.map(function (p) { return { x: p.x + 15, y: p.y, z: p.z }; }),
            trackBank: TRACK_BANK, trackDwell: TRACK_DWELL, trackClosed: true,
            trackSpeed: SPEED, trackAcceleration: ACCELERATION
        });
        var p = Entities.getEntityProperties(track, ["type", "trackPoints", "trackClosed", "trackSpeed"]);
        log("track " + track + ": type " + p.type + ", " + (p.trackPoints ? p.trackPoints.length : 0) + " points, closed " +
            p.trackClosed + ", speed " + p.trackSpeed);
        // the cabin stands at the station, facing -z, and is a frame on the track; its train starts with the dwell
        cabin = Entities.addEntity({
            type: "Box", name: "baseline-cabin", shapeType: "box", lifetime: LIFETIME,
            position: { x: 0, y: CABIN_Y, z: 0 }, rotation: Quat.IDENTITY,
            dimensions: { x: 4, y: 0.5, z: 8 },
            color: { red: 200, green: 160, blue: 60 },
            dynamic: false, collisionless: false,
            damping: 0, angularDamping: 0,
            grab: { grabbable: false },
            description: "boarding", userData: JSON.stringify({ boarding: BOARDING }),
            frame: true, trackID: track, trackOffset: 0
        });
        // low walls, children of the cabin; a 2 m door in the +x wall at the middle, for the platform, and the back
        // end (+z) open in the middle as before
        var wallSpecs = [
            { pos: { x: 2.1, y: 0.5, z: -2.5 }, dim: { x: 0.2, y: 1.0, z: 3 } },
            { pos: { x: 2.1, y: 0.5, z: 2.5 }, dim: { x: 0.2, y: 1.0, z: 3 } },
            { pos: { x: -2.1, y: 0.5, z: 0 }, dim: { x: 0.2, y: 1.0, z: 8 } },
            { pos: { x: 0, y: 0.5, z: -4.1 }, dim: { x: 4.4, y: 1.0, z: 0.2 } },
            { pos: { x: 1.5, y: 0.5, z: 4.1 }, dim: { x: 1.4, y: 1.0, z: 0.2 } },
            { pos: { x: -1.5, y: 0.5, z: 4.1 }, dim: { x: 1.4, y: 1.0, z: 0.2 } }
        ];
        wallSpecs.forEach(function (w) {
            walls.push(Entities.addEntity({
                type: "Box", name: "baseline-wall", shapeType: "box", lifetime: LIFETIME,
                parentID: cabin, localPosition: w.pos, dimensions: w.dim,
                color: { red: 120, green: 90, blue: 40 },
                dynamic: false, collisionless: false, grab: { grabbable: false }
            }));
        });
        var c = Entities.getEntityProperties(cabin, ["frame", "trackID", "trackOffset"]);
        log("cabin " + cabin + ": frame " + c.frame + ", trackID " + c.trackID + ", offset " + c.trackOffset);
        MyAvatar.position = PLATFORM_SPOT;
        MyAvatar.orientation = Quat.IDENTITY;
    }

    function cabinState() {
        return Entities.getEntityProperties(cabin, ["position", "rotation", "velocity", "angularVelocity"]);
    }

    function worldPoint(index) {
        var p = TRACK_POINTS[index];
        return { x: p.x, y: CABIN_Y, z: p.z };
    }

    // the phase of the ride, read off the cabin's own motion
    function readMotion(c, dt) {
        var speed = Vec3.length(c.velocity);
        var turning = Vec3.length(c.angularVelocity) > TURNING;
        var m;
        if (speed < 0.02) {
            m = "standing";
        } else if (turning) {
            m = "curve";
        } else if (speed < SPEED - 0.05) {
            // with a little hysteresis: the creep into a station holds its speed and keeps its label
            m = (speed > lastSpeed + 0.002) ? "accelerating" : (speed < lastSpeed - 0.002) ? "braking" :
                (motion === "accelerating" || motion === "braking") ? motion : "braking";
        } else {
            m = "cruising";
        }
        lastSpeed = speed;
        if (speed > 1) { leftStation = true; }
        if (m !== motion) {
            motion = m;
            events.push({ t: now(), motion: m });
            publish();
        }
        return speed;
    }

    function sample(dt) {
        var c = cabinState();
        if (!c.position) { return false; }
        var speed = readMotion(c, dt);
        var worldPos = MyAvatar.position;
        var parented = MyAvatar.parentID == cabin;
        var inv = Quat.inverse(c.rotation);
        samples.push({
            index: samples.length, t: now(), dt: dt, mode: mode, motion: motion, parented: parented, speed: speed,
            cabinPos: c.position, cabinRot: c.rotation, cabinVel: c.velocity, cabinAngVel: c.angularVelocity,
            avatarPos: worldPos,
            local: Vec3.multiplyQbyV(inv, Vec3.subtract(worldPos, c.position)),
            localProp: parented ? MyAvatar.localPosition : null
        });
        return true;
    }

    function drive() {
        var tt = now() - modeT0;
        var c, local;
        if (mode === "boot") {
            if (tt > 8) { spawn(); cabinT0 = now(); setMode("boarding"); }
        } else if (mode === "boarding") {
            if (tt > 1 && Vec3.length(MyAvatar.motorVelocity) === 0) {
                MyAvatar.motorReferenceFrame = "world";
                MyAvatar.motorTimescale = 0.2;
                MyAvatar.motorVelocity = Vec3.multiply(WALK_SPEED, BOARDING.direction);
            }
            c = cabinState();
            local = Vec3.multiplyQbyV(Quat.inverse(c.rotation), Vec3.subtract(MyAvatar.position, c.position));
            if ((tt > 1 && local[BOARDING.axis] < BOARDING.limit) || tt > 8) {
                MyAvatar.motorVelocity = Vec3.ZERO;
                log("aboard at local " + JSON.stringify(local) + ", parentID " + MyAvatar.parentID + (tt > 8 ? " (timed out)" : ""));
                setMode("riding");
            }
        } else if (mode === "riding") {
            // a lap is done when the cabin stands at the platform station again
            c = cabinState();
            var home = Vec3.distance(c.position, worldPoint(STATION_POINT)) < 1.0;
            if (leftStation && motion === "standing" && home && tt > 10) {
                log("back at the station after " + tt.toFixed(1) + " s, cabin at " + JSON.stringify(c.position));
                setMode("step-off");
            } else if (tt > 140) {
                log("lap not finished in time; stepping off anyway");
                setMode("step-off");
            }
        } else if (mode === "step-off") {
            // out through the door onto the platform
            if (tt > 2 && Vec3.length(MyAvatar.motorVelocity) === 0) {
                MyAvatar.motorReferenceFrame = "world";
                MyAvatar.motorTimescale = 0.2;
                MyAvatar.motorVelocity = Vec3.multiply(WALK_SPEED, BOARDING.off);
            }
            if (tt > 2 && (MyAvatar.parentID == Uuid.NULL || tt > 8)) {
                MyAvatar.motorVelocity = Vec3.ZERO;
                log("off at " + JSON.stringify(MyAvatar.position) + ", parentID " + MyAvatar.parentID);
                setMode("ground");
            }
        } else if (mode === "ground") {
            if (tt > 4) { setMode("end"); finish("done"); }
        }
    }

    // ---- analysis ----
    // frame-to-frame change minus the mean change, rms and max, horizontal and vertical; only between samples that
    // follow one another in time (a phase comes round more than once in a lap)
    function jitter(rows, key) {
        var deltas = [];
        for (var i = 1; i < rows.length; i++) {
            if (rows[i].index === rows[i - 1].index + 1 && rows[i][key] && rows[i - 1][key]) {
                deltas.push(Vec3.subtract(rows[i][key], rows[i - 1][key]));
            }
        }
        if (deltas.length < 2) { return null; }
        var mean = { x: 0, y: 0, z: 0 };
        deltas.forEach(function (d) { mean = Vec3.sum(mean, d); });
        mean = Vec3.multiply(1 / deltas.length, mean);
        var sumH = 0, sumV = 0, maxH = 0, maxV = 0;
        deltas.forEach(function (d) {
            var e = Vec3.subtract(d, mean);
            var h = Math.sqrt(e.x * e.x + e.z * e.z), v = Math.abs(e.y);
            sumH += h * h; sumV += v * v;
            if (h > maxH) { maxH = h; }
            if (v > maxV) { maxV = v; }
        });
        // and the step lengths themselves: their mean, their rms deviation from it and the largest (the glide's evenness
        // round a loop, where the mean of the vectors says nothing)
        var lengths = deltas.map(function (d) { return Vec3.length(d); });
        var lengthMean = lengths.reduce(function (a, b) { return a + b; }, 0) / lengths.length;
        var lengthDev = 0, lengthMax = 0, over = 0;
        lengths.forEach(function (l) { lengthDev += (l - lengthMean) * (l - lengthMean); if (l > lengthMax) { lengthMax = l; } if (l > 2 * lengthMean) { over++; } });
        return { n: deltas.length, meanStepMm: 1000 * Vec3.length(mean),
            rmsMm: 1000 * Math.sqrt(sumH / deltas.length), maxMm: 1000 * maxH,
            verticalRmsMm: 1000 * Math.sqrt(sumV / deltas.length), verticalMaxMm: 1000 * maxV,
            stepMeanMm: 1000 * lengthMean, stepDevMm: 1000 * Math.sqrt(lengthDev / lengths.length), stepMaxMm: 1000 * lengthMax, stepsOverTwiceMean: over };
    }

    function stats(label, rows) {
        if (rows.length < 3) { return null; }
        var first = rows[0], last = rows[rows.length - 1];
        var drift = Vec3.subtract(last.local, first.local);
        var ridingRows = rows.filter(function (r) { return r.parented; });
        var maxSpeed = 0;
        rows.forEach(function (r) { if (r.speed > maxSpeed) { maxSpeed = r.speed; } });
        return {
            phase: label, frames: rows.length, seconds: last.t - first.t, parentedFrames: ridingRows.length,
            maxSpeed: maxSpeed,
            driftM: drift, driftHorizontalM: Math.sqrt(drift.x * drift.x + drift.z * drift.z),
            computedLocal: jitter(rows, "local"),
            storedLocal: jitter(ridingRows, "localProp"),
            cabinStep: jitter(rows, "cabinPos")
        };
    }

    function fmt(j) {
        return j ? ("rms " + j.rmsMm.toFixed(1) + " mm, max " + j.maxMm.toFixed(1) + " mm (vertical rms " +
            j.verticalRmsMm.toFixed(1) + ", max " + j.verticalMaxMm.toFixed(1) + " mm; mean step " + j.meanStepMm.toFixed(1) +
            " mm over " + j.n + " frames)") : "n/a";
    }

    function fmtStep(j) {
        return j ? ("mean " + j.stepMeanMm.toFixed(1) + " mm, rms deviation " + j.stepDevMm.toFixed(1) + " mm, max " +
            j.stepMaxMm.toFixed(1) + " mm, " + j.stepsOverTwiceMean + " over twice the mean, over " + j.n + " frames") : "n/a";
    }

    // the frames at which the avatar's parent changed and the largest world-position step at and just after each (the
    // pop, which can land a sample after the change), over all samples in order
    function crossings(key) {
        var out = [];
        for (var k = 1; k < samples.length; k++) {
            if (samples[k].parented !== samples[k - 1].parented) {
                var before = Vec3.length(Vec3.subtract(samples[k - 1][key], samples[Math.max(k - 2, 0)][key]));
                var at = 0;
                for (var m = k; m < Math.min(k + 3, samples.length); m++) {
                    at = Math.max(at, Vec3.length(Vec3.subtract(samples[m][key], samples[m - 1][key])));
                }
                out.push({ t: samples[k].t, parented: samples[k].parented, mode: samples[k].mode,
                    worldStepMm: 1000 * at, previousStepMm: 1000 * before });
            }
        }
        return out;
    }
    function fmtCrossings(list) {
        return list.length ? list.map(function (c) {
            return (c.parented ? "in" : "out") + " at " + c.t.toFixed(2) + " s (" + c.mode + "), world step " + c.worldStepMm.toFixed(1) +
                " mm (previous " + c.previousStepMm.toFixed(1) + " mm)"; }).join("; ") : "none";
    }

    var finished = false;
    function finish(reason) {
        if (finished) { return; }
        finished = true;
        Script.update.disconnect(onUpdate);
        MyAvatar.motorVelocity = Vec3.ZERO;
        // the ride, by motion phase in order of first appearance, riding only; then boarding and stepping off
        var summary = [];
        var seen = [];
        samples.forEach(function (r) {
            var key = r.mode === "riding" ? "riding/" + r.motion : r.mode;
            if (seen.indexOf(key) < 0) { seen.push(key); }
        });
        seen.forEach(function (key) {
            var rows = samples.filter(function (r) { return (r.mode === "riding" ? "riding/" + r.motion : r.mode) === key; });
            var s = stats(key, rows);
            if (s) { summary.push(s); }
        });
        // where the cabin stood at the far station, against the track point there
        var farRows = samples.filter(function (r) {
            return r.mode === "riding" && r.motion === "standing" && Vec3.distance(r.cabinPos, worldPoint(FAR_STATION_POINT)) < 2;
        });
        var farStop = farRows.length ? Vec3.distance(farRows[farRows.length - 1].cabinPos, worldPoint(FAR_STATION_POINT)) : null;
        summary.forEach(function (s) {
            log(s.phase + ": " + s.frames + " frames in " + s.seconds.toFixed(1) + " s; riding " + s.parentedFrames + "/" + s.frames +
                "; cabin speed up to " + s.maxSpeed.toFixed(2) + " m/s" +
                "; drift " + s.driftHorizontalM.toFixed(3) + " m horizontal (" + s.driftM.y.toFixed(3) + " m vertical)" +
                "; computed local " + fmt(s.computedLocal) +
                "; stored local " + fmt(s.storedLocal) +
                "; cabin world step " + fmtStep(s.cabinStep));
        });
        var crossed = crossings("avatarPos");
        log("crossings: " + fmtCrossings(crossed));
        if (farStop !== null) {
            log("far station: the cabin stood " + (1000 * farStop).toFixed(0) + " mm from the track point, for " +
                (farRows.length ? (farRows[farRows.length - 1].t - farRows[0].t).toFixed(1) : "0") + " s");
        }
        Test.saveObject({ summary: summary, events: events, crossings: crossed, farStopMm: farStop === null ? null : 1000 * farStop, samples: samples },
            "physics-track.json");
        log("done (" + reason + "), " + samples.length + " samples");
        walls.forEach(function (w) { Entities.deleteEntity(w); });
        if (cabin) { Entities.deleteEntity(cabin); }
        if (track) { Entities.deleteEntity(track); }
        if (platform) { Entities.deleteEntity(platform); }
        Script.setTimeout(function () { Test.quit(); }, 1000);
    }

    // who drives the cabin: its simulation owner, logged whenever it changes (round 5: the physics server takes it at
    // priority 255 from this client, the creator), and the track's train state as the server publishes it
    var ownerSeen = "";
    var lastOwnerPoll = 0;
    function watchOwner() {
        if (!cabin || now() - lastOwnerPoll < 1) { return; }
        lastOwnerPoll = now();
        var p = Entities.getEntityProperties(cabin, ["simulationOwner"]);
        var owner = p.simulationOwner ? (p.simulationOwner.ownerID + " at priority " + p.simulationOwner.priority) : "none";
        var tp = Entities.getEntityProperties(track, ["trainDistance", "trainSpeed", "trainStation", "trainDwell", "trainTime"]);
        var state = "distance " + (tp.trainDistance || 0).toFixed(2) + " m, speed " + (tp.trainSpeed || 0).toFixed(2) + ", station " +
            tp.trainStation + ", dwell " + (tp.trainDwell || 0).toFixed(1) + ", time " + tp.trainTime;
        trainStates.push({ t: now(), state: state });
        if (owner !== ownerSeen) {
            ownerSeen = owner;
            var me = p.simulationOwner && p.simulationOwner.ownerID == MyAvatar.sessionUUID ? " (me)" : "";
            events.push({ t: now(), owner: owner });
            log("cabin owner " + owner + me + " at " + now().toFixed(2) + " s; train state: " + state);
        } else if (state !== lastState) {
            log("train state at " + now().toFixed(2) + " s: " + state);
        }
        lastState = state;
    }
    var trainStates = [];
    var lastState = "";

    function onUpdate(dt) {
        if (now() > (cabinT0 >= 0 ? cabinT0 + TIMEOUT : MAX_WAIT)) { finish("timeout"); return; }
        if (cabin && !sample(dt)) { finish("cabin gone"); return; }
        watchOwner();
        drive();
    }

    Script.update.connect(onUpdate);
    Script.scriptEnding.connect(function () { MyAvatar.motorVelocity = Vec3.ZERO; });
    log("started");
}());
