//
//  observer.js
//  tests-manual/physics-baseline
//
//  Round 3 of PHYSICS.md: the second client. While baseline-frame.js moves the cabin on one client, this
//  script runs on another client in the same domain: it waits for the cabin, steps on from the station
//  platform while the cabin stands still (the crossing), rides every phase, steps off behind the cabin when
//  the mover does (step-off) and watches it go from the ground. Every script frame it records the cabin as
//  this client sees it, its own pose, and the other avatar's pose as the avatar mixer delivers it. At the end
//  a summary is printed (lines starting with PHYSICS-OBSERVER) and the samples are written with Test.saveObject.
//
//  Round 5 (run-monorail.sh): the physics server drives the cabin, bot riders sit in it and bots walk below, and this
//  client may run under TIVOLI_PACKET_LOSS. The summary then also counts the avatars seen and the riders aboard, gives
//  the riders' stored local positions in aggregate (their jitter as the mixer delivers them), and counts the cabin's
//  steps over twice the mean step: the corrections.
//
//  Run (see run-pair.sh):
//    interface --allowMultipleInstances --url hifi://localhost:<port> --testScript observer.js --testResultsLocation <dir>
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var WALK_SPEED = 1.5;         // m/s, the scripted motor
    var PLATFORM_SPOT = { x: 0, y: 1.3, z: 6.3 };   // on the station platform, behind the cabin's open end
    var ABOARD_Z = 2.5;           // where to stop inside the cabin, in its coordinates
    // where to stand, which way to walk aboard, how to tell one is aboard, and which way off: round 3's cabin is
    // boarded from behind; a mover may say otherwise in the cabin's userData ({"boarding": {...}}, see track.js)
    var boarding = { spot: PLATFORM_SPOT, direction: { x: 0, y: 0, z: -1 }, axis: "z", limit: ABOARD_Z, off: { x: 0, y: 0, z: 1 } };
    var TIMEOUT = 150;            // s, from the cabin's appearance (the world can take 90 s to load on a cold cache under loss)
    var MAX_WAIT = 300;           // s, for the cabin to appear at all
    var cabinSeenT0 = -1;

    var cabin = null;
    var mode = "waiting";         // waiting, arriving, stepping-on, riding, stepping-off, ground
    var samples = [];
    var events = [];
    var t0 = Date.now() / 1000;
    var modeT0 = 0;
    var lastPhase = "";
    var tookSteps = { "step-off": false };

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print("PHYSICS-OBSERVER " + s); }
    function setMode(m) {
        mode = m; modeT0 = now();
        events.push({ t: now(), mode: m });
        log("mode " + m + " at " + now().toFixed(2) + " s");
    }

    function findCabin() {
        // the newest of that name, and only one made since this observer started: a cabin from an earlier run may
        // still be in the world until its lifetime ends (the mover deletes it when it makes its own)
        var found = Entities.findEntitiesByName("baseline-cabin", { x: 0, y: 0, z: 0 }, 500);
        var best = null, bestAge = Infinity;
        found.forEach(function (id) {
            var age = Entities.getEntityProperties(id, ["age"]).age;
            if (age !== undefined && age < bestAge && age < now() + 1) { bestAge = age; best = id; }
        });
        return best;
    }

    function cabinState() {
        return Entities.getEntityProperties(cabin, ["position", "rotation", "velocity", "angularVelocity", "description"]);
    }

    var moverID = null;   // the first other avatar seen is the mover (the bots come parented already or walk below)
    function otherAvatar() {
        var ids = AvatarList.getAvatarIdentifiers();
        if (moverID && ids.indexOf(moverID) >= 0) {
            return AvatarList.getAvatar(moverID);
        }
        for (var i = 0; i < ids.length; i++) {
            if (ids[i] && ids[i] !== MyAvatar.sessionUUID && ids[i] !== Uuid.NULL) {
                var candidate = AvatarList.getAvatar(ids[i]);
                if (candidate && (candidate.displayName || "").indexOf("rider ") !== 0 && (candidate.displayName || "").indexOf("walker ") !== 0) {
                    moverID = ids[i];
                    return candidate;
                }
            }
        }
        return null;
    }

    // every tenth sample: how many avatars there are, how many ride, and each rider's stored local position
    var crowdSamples = [];
    var riderTracks = {};   // id -> [local positions], while parented to the cabin
    function sampleCrowd() {
        var ids = AvatarList.getAvatarIdentifiers();
        var riders = 0, total = 0;
        for (var i = 0; i < ids.length; i++) {
            if (!ids[i] || ids[i] === MyAvatar.sessionUUID || ids[i] === Uuid.NULL) { continue; }
            var a = AvatarList.getAvatar(ids[i]);
            if (!a) { continue; }
            total++;
            if (a.parentID == cabin) {
                riders++;
                (riderTracks[ids[i]] = riderTracks[ids[i]] || []).push(a.localPosition);
            }
        }
        crowdSamples.push({ t: now(), avatars: total, riders: riders });
    }

    function sample(dt) {
        var c = cabinState();
        if (!c.position) { return false; }
        var phase = c.description || "";
        if (phase !== lastPhase) {
            lastPhase = phase;
            log("mover phase " + phase + " at " + now().toFixed(2) + " s");
        }
        var inv = Quat.inverse(c.rotation);
        var myPos = MyAvatar.position;
        var parented = MyAvatar.parentID == cabin;
        var row = {
            index: samples.length, t: now(), dt: dt, phase: phase, mode: mode, parented: parented,
            cabinPos: c.position, cabinRot: c.rotation, cabinVel: c.velocity,
            myPos: myPos,
            myLocal: Vec3.multiplyQbyV(inv, Vec3.subtract(myPos, c.position)),
            myLocalProp: parented ? MyAvatar.localPosition : null
        };
        if (samples.length % 10 === 0) { sampleCrowd(); }
        var other = otherAvatar();
        if (other) {
            var otherPos = other.position;
            var otherParented = other.parentID == cabin;
            row.otherPos = otherPos;
            row.otherParented = otherParented;
            row.otherLocal = Vec3.multiplyQbyV(inv, Vec3.subtract(otherPos, c.position));
            row.otherLocalProp = otherParented ? other.localPosition : null;
            // what this rider sees: the other rider's position relative to its own, in the cabin's frame
            row.relative = Vec3.multiplyQbyV(inv, Vec3.subtract(otherPos, myPos));
        }
        samples.push(row);
        return true;
    }

    function drive() {
        var tt = now() - modeT0;
        if (mode === "arriving") {
            if (tt > 0.5) {
                MyAvatar.motorReferenceFrame = "world";
                MyAvatar.motorTimescale = 0.2;
                MyAvatar.motorVelocity = Vec3.multiply(WALK_SPEED, boarding.direction);
                setMode("stepping-on");
            }
        } else if (mode === "stepping-on") {
            var c = cabinState();
            var local = Vec3.multiplyQbyV(Quat.inverse(c.rotation), Vec3.subtract(MyAvatar.position, c.position));
            if (local[boarding.axis] < boarding.limit || tt > 6) {
                MyAvatar.motorVelocity = Vec3.ZERO;
                log("aboard at local " + JSON.stringify(local) + ", parentID " + MyAvatar.parentID + (tt > 6 ? " (timed out)" : ""));
                setMode("riding");
            }
        } else if (mode === "riding") {
            if (lastPhase === "step-off" && !tookSteps["step-off"] && now() - stepOffSeen > 3) {
                // the mover walked off at two seconds; this rider follows a second later
                tookSteps["step-off"] = true;
                MyAvatar.motorReferenceFrame = "world";
                MyAvatar.motorTimescale = 0.2;
                MyAvatar.motorVelocity = Vec3.multiply(WALK_SPEED, boarding.off);
                setMode("stepping-off");
            }
        } else if (mode === "stepping-off") {
            if (MyAvatar.parentID == Uuid.NULL || tt > 5) {
                MyAvatar.motorVelocity = Vec3.ZERO;
                setMode("ground");
            }
        }
    }
    var stepOffSeen = -1;

    // ---- analysis ----
    function jitter(vectors, indices) {
        // frame-to-frame change minus the mean change, rms and max, horizontal and vertical; with indices given, only
        // between samples that follow one another in time
        if (vectors.length < 3) { return null; }
        var deltas = [];
        for (var i = 1; i < vectors.length; i++) {
            if (indices && indices[i] !== indices[i - 1] + 1) { continue; }
            deltas.push(Vec3.subtract(vectors[i], vectors[i - 1]));
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
        // the step lengths: their mean, and how many are over twice it (the corrections landing)
        var lengths = deltas.map(function (d) { return Vec3.length(d); });
        var lengthMean = lengths.reduce(function (a, b) { return a + b; }, 0) / lengths.length;
        var lengthDev = 0, lengthMax = 0, over = 0;
        lengths.forEach(function (l) { lengthDev += (l - lengthMean) * (l - lengthMean); if (l > lengthMax) { lengthMax = l; } if (l > 2 * lengthMean) { over++; } });
        return { n: deltas.length, meanStepMm: 1000 * Vec3.length(mean),
            rmsMm: 1000 * Math.sqrt(sumH / deltas.length), maxMm: 1000 * maxH,
            verticalRmsMm: 1000 * Math.sqrt(sumV / deltas.length), verticalMaxMm: 1000 * maxV,
            stepMeanMm: 1000 * lengthMean, stepDevMm: 1000 * Math.sqrt(lengthDev / lengths.length), stepMaxMm: 1000 * lengthMax, stepsOverTwiceMean: over };
    }

    function stats(name) {
        var rows = samples.filter(function (r) { return r.phase === name; });
        if (rows.length < 3) { return null; }
        var first = rows[0], last = rows[rows.length - 1];
        var ridingRows = rows.filter(function (r) { return r.parented; });
        var groundRows = rows.filter(function (r) { return !r.parented && r.mode === "ground"; });
        var pick = function (list, key) { return list.filter(function (r) { return r[key]; }).map(function (r) { return r[key]; }); };
        var indicesOf = function (list, key) { return list.filter(function (r) { return r[key]; }).map(function (r) { return r.index; }); };
        return {
            phase: name, frames: rows.length, seconds: last.t - first.t,
            parentedFrames: ridingRows.length,
            otherParentedFrames: rows.filter(function (r) { return r.otherParented; }).length,
            // this rider's own stored local position, while riding (should not move)
            myStoredLocal: jitter(pick(ridingRows, "myLocalProp"), indicesOf(ridingRows, "myLocalProp")),
            // the other rider's stored local position as received (should not move while they stand still)
            otherStoredLocal: jitter(pick(ridingRows, "otherLocalProp"), indicesOf(ridingRows, "otherLocalProp")),
            // the other rider as seen from here, relative, in the cabin's frame (two reads a moment apart)
            relative: jitter(pick(ridingRows, "relative"), indicesOf(ridingRows, "relative")),
            // the cabin's own world motion on this client: the step per frame, and how even it is
            cabinStepRiding: jitter(ridingRows.map(function (r) { return r.cabinPos; }), ridingRows.map(function (r) { return r.index; })),
            cabinStepGround: jitter(groundRows.map(function (r) { return r.cabinPos; }), groundRows.map(function (r) { return r.index; }))
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

    // the frames at which my parent changed and the largest world-position step at and just after each (the pop can
    // land a sample after the change), over all samples in order
    function crossings() {
        var out = [];
        for (var k = 1; k < samples.length; k++) {
            if (samples[k].parented !== samples[k - 1].parented) {
                var before = Vec3.length(Vec3.subtract(samples[k - 1].myPos, samples[Math.max(k - 2, 0)].myPos));
                var at = 0;
                for (var m = k; m < Math.min(k + 3, samples.length); m++) {
                    at = Math.max(at, Vec3.length(Vec3.subtract(samples[m].myPos, samples[m - 1].myPos)));
                }
                out.push({ t: samples[k].t, parented: samples[k].parented, mode: samples[k].mode,
                    worldStepMm: 1000 * at, previousStepMm: 1000 * before });
            }
        }
        return out;
    }

    var finished = false;
    function finish(reason) {
        if (finished) { return; }
        finished = true;
        Script.update.disconnect(onUpdate);
        MyAvatar.motorVelocity = Vec3.ZERO;
        var phases = [];
        samples.forEach(function (r) { if (phases.indexOf(r.phase) < 0) { phases.push(r.phase); } });
        var summary = phases.map(stats).filter(function (s) { return s; });
        summary.forEach(function (s) {
            log(s.phase + ": " + s.frames + " frames in " + s.seconds.toFixed(1) + " s; riding " + s.parentedFrames +
                ", other riding " + s.otherParentedFrames +
                "; my stored local " + fmt(s.myStoredLocal) +
                "; other's stored local " + fmt(s.otherStoredLocal) +
                "; other relative to me " + fmt(s.relative) +
                "; cabin step while riding " + fmtStep(s.cabinStepRiding) +
                "; cabin step from the ground " + fmtStep(s.cabinStepGround));
        });
        // the crowd: avatars seen and riders aboard, and the riders' stored local positions in aggregate
        if (crowdSamples.length) {
            var maxAvatars = 0, maxRiders = 0;
            crowdSamples.forEach(function (c) { if (c.avatars > maxAvatars) { maxAvatars = c.avatars; } if (c.riders > maxRiders) { maxRiders = c.riders; } });
            var worst = null, count = 0, sumRms = 0;
            Object.keys(riderTracks).forEach(function (id) {
                var j = jitter(riderTracks[id]);
                if (!j) { return; }
                count++; sumRms += j.rmsMm;
                if (!worst || j.maxMm > worst.maxMm) { worst = j; }
            });
            log("crowd: up to " + maxAvatars + " other avatars, up to " + maxRiders + " of them riding; " + count +
                " riders' stored local positions: mean rms " + (count ? (sumRms / count).toFixed(2) : "n/a") + " mm, worst " +
                (worst ? fmt(worst) : "n/a"));
        }
        var crossed = crossings();
        log("crossings: " + (crossed.length ? crossed.map(function (c) {
            return (c.parented ? "in" : "out") + " at " + c.t.toFixed(2) + " s (" + c.mode + "), world step " + c.worldStepMm.toFixed(1) +
                " mm (previous " + c.previousStepMm.toFixed(1) + " mm)"; }).join("; ") : "none"));
        Test.saveObject({ summary: summary, events: events, crossings: crossed, crowd: crowdSamples, samples: samples }, "physics-observer.json");
        log("done (" + reason + "), " + samples.length + " samples");
        Script.setTimeout(function () { Test.quit(); }, 1000);
    }

    var lastPoll = 0;
    function onUpdate(dt) {
        if (now() > (cabinSeenT0 >= 0 ? cabinSeenT0 + TIMEOUT : MAX_WAIT)) { finish("timeout"); return; }
        if (mode === "waiting") {
            if (now() - lastPoll < 0.1) { return; }
            lastPoll = now();
            cabin = findCabin();
            if (cabin) {
                log("cabin " + cabin + " seen at " + now().toFixed(2) + " s");
                if (cabinSeenT0 < 0) { cabinSeenT0 = now(); }
                try {
                    var said = JSON.parse(Entities.getEntityProperties(cabin, ["userData"]).userData || "{}");
                    if (said.boarding) {
                        ["spot", "direction", "axis", "limit", "off"].forEach(function (k) {
                            if (said.boarding[k] !== undefined) { boarding[k] = said.boarding[k]; }
                        });
                        log("boarding as the mover says: " + JSON.stringify(boarding));
                    }
                } catch (e) {
                    log("cabin userData unreadable: " + e);
                }
                MyAvatar.position = boarding.spot;
                MyAvatar.orientation = Quat.IDENTITY;
                setMode("arriving");
            }
            return;
        }
        if (!sample(dt)) {
            if (mode === "arriving" || mode === "stepping-on") {
                // the cabin went before this rider was aboard (an earlier run's, replaced): wait for the next one
                log("cabin gone before boarding; waiting again");
                MyAvatar.motorVelocity = Vec3.ZERO;
                cabin = null;
                setMode("waiting");
                return;
            }
            finish("cabin gone");
            return;
        }
        if (lastPhase === "step-off" && stepOffSeen < 0) { stepOffSeen = now(); }
        drive();
    }

    Script.update.connect(onUpdate);
    Script.scriptEnding.connect(function () { MyAvatar.motorVelocity = Vec3.ZERO; });
    log("started");
}());
