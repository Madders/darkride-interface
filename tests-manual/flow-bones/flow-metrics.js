//
//  flow-metrics.js
//  tests-manual/flow-bones
//
//  Measure the flow bones (spring bones) on an avatar with flow groups (ookatoo is the standard): load the
//  avatar, stand, walk, stop, spin, settle, and every script frame record the position of every joint of
//  every flow chain. At the end print per chain and per phase how the chain's tip moved relative to its root
//  (lines starting with FLOW-METRICS) and save the samples with Test.saveObject when
//  the client was started with --testResultsLocation.
//
//  Run (serverless):   tests-manual/flow-bones/run.sh [Darkride.app] [results dir]
//  On a domain, with flow-observer.js on a second client watching this avatar: run-pair.sh. The chains and
//  each phase are published on the "flow-metrics" message channel (the phase also in the display name).
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    // The summary of a run's samples: the same arithmetic in flow-metrics.js and flow-observer.js, so the
    // sender's chain and the chain other clients receive can be put side by side.
    var FlowMetrics = (function () {
        var REST_SPEED = 0.03;         // m/s: below this the tip counts as still
        var REST_HOLD = 0.5;           // s it must stay still
        var PHASES = ["stand", "walk", "stop", "spin", "settle"];

        function sub(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]; }
        function len(a) { return Math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
        function mean(a) { var sum = 0; for (var j = 0; j < a.length; j++) { sum += a[j]; } return a.length ? sum / a.length : 0; }
        function max(a) { var m = 0; for (var j = 0; j < a.length; j++) { if (a[j] > m) { m = a[j]; } } return m; }
        function same(a, b) {
            for (var name in a) {
                if (!b[name] || a[name].length !== b[name].length) {
                    return false;
                }
                for (var i = 0; i < a[name].length; i++) {
                    if (a[name][i][0] !== b[name][i][0] || a[name][i][1] !== b[name][i][1] || a[name][i][2] !== b[name][i][2]) {
                        return false;
                    }
                }
            }
            return true;
        }

        // A sample whose joints did not move since the last one saw no new pose (the script samples on its own
        // clock; another client only gets poses at the mixer's rate). Drop it so velocities are per new pose.
        function dedupe(samples) {
            var out = [];
            for (var i = 0; i < samples.length; i++) {
                var s = samples[i];
                if (out.length && s.phase === out[out.length - 1].phase && same(s.threads, out[out.length - 1].threads)) {
                    continue;
                }
                out.push(s);
            }
            return out;
        }

        // Per chain and phase: the tip's motion relative to the chain root (what the eye sees against the body):
        // mean and peak speed, mean and peak frame-to-frame change of velocity (jitter), and in a phase where the
        // body is still, how long the tip took to come to rest.
        function summarize(rawSamples, threads, log, tag) {
            var samples = dedupe(rawSamples);
            var summary = {};
            var byPhase = {};
            for (var i = 0; i < samples.length; i++) {
                if (!byPhase[samples[i].phase]) {
                    byPhase[samples[i].phase] = [];
                }
                byPhase[samples[i].phase].push(samples[i]);
            }
            for (var name in threads) {
                summary[name] = {};
                var tip = threads[name].length - 1;
                for (var phaseName in byPhase) {
                    var list = byPhase[phaseName];
                    var speeds = [];
                    var accels = [];
                    var lastV = null;
                    var stillSince = null;
                    var settle = null;
                    for (var k = 1; k < list.length; k++) {
                        var dt = list[k].t - list[k - 1].t;
                        if (dt <= 0 || !list[k].threads[name] || !list[k - 1].threads[name]) {
                            continue;
                        }
                        var rel1 = sub(list[k].threads[name][tip], list[k].threads[name][0]);
                        var rel0 = sub(list[k - 1].threads[name][tip], list[k - 1].threads[name][0]);
                        var v = sub(rel1, rel0);
                        v = [v[0] / dt, v[1] / dt, v[2] / dt];
                        var speed = len(v);
                        speeds.push(speed);
                        if (lastV) {
                            accels.push(len(sub(v, lastV)) / dt);
                        }
                        lastV = v;
                        var tt = list[k].t - list[0].t;
                        if (settle === null) {
                            if (speed < REST_SPEED) {
                                if (stillSince === null) {
                                    stillSince = tt;
                                } else if (tt - stillSince >= REST_HOLD) {
                                    settle = stillSince;
                                }
                            } else {
                                stillSince = null;
                            }
                        }
                    }
                    var row = {
                        frames: list.length,
                        speedMean: mean(speeds), speedMax: max(speeds),
                        accelMean: mean(accels), accelMax: max(accels),
                        settle: settle
                    };
                    summary[name][phaseName] = row;
                    log(name + " " + phaseName + " frames=" + row.frames +
                        " relSpeed=" + row.speedMean.toFixed(3) + " max=" + row.speedMax.toFixed(3) +
                        " relAccel=" + row.accelMean.toFixed(1) + " max=" + row.accelMax.toFixed(1) +
                        (settle !== null ? " settle=" + settle.toFixed(2) : " settle=none"));
                }
            }
            // the mean over every chain per phase: the one-line comparison between builds and between clients
            for (var p = 0; p < PHASES.length; p++) {
                var sumSpeed = 0, sumAccel = 0, worst = 0, n = 0, settled = 0, settleSum = 0;
                for (var chain in summary) {
                    var r = summary[chain][PHASES[p]];
                    if (r) {
                        sumSpeed += r.speedMean;
                        sumAccel += r.accelMean;
                        if (r.accelMax > worst) {
                            worst = r.accelMax;
                        }
                        if (r.settle !== null) {
                            settled++;
                            settleSum += r.settle;
                        }
                        n++;
                    }
                }
                if (n) {
                    log("ALL " + PHASES[p] + " chains=" + n + " relSpeed=" + (sumSpeed / n).toFixed(3) +
                        " relAccel=" + (sumAccel / n).toFixed(1) + " worst=" + worst.toFixed(1) +
                        " settled=" + settled + (settled ? " meanSettle=" + (settleSum / settled).toFixed(2) : ""));
                }
            }
            log("poses " + samples.length + " of " + rawSamples.length + " samples" + (tag ? " (" + tag + ")" : ""));
            return summary;
        }

        return { summarize: summarize, dedupe: dedupe, PHASES: PHASES };
    }());

    var AVATAR_URL = "https://files.vr.darkride.ai/caitlyn/avatars/ookatoo/ookatoo.fst";
    var WALK_SPEED = 1.4;          // m/s
    var SPIN_RATE = 180;           // degrees per second
    var CHANNEL = "flow-metrics";
    var LIFETIME = 600;            // s; on a domain the ground the test makes expires on its own
    // Group settings to try instead of the avatar's own (what a changed FST would give), applied once flow is up.
    // Give every key of a group you override: MyAvatar.useFlow starts each listed group from the defaults.
    var FLOW_OVERRIDES = {
        Tail: { active: true, stiffness: 0.6, radius: 0.01, gravity: -0.0096, damping: 0.85, inertia: 0.8, delta: 0.55, maxAngle: 25, lift: 12 }
    };

    var previousAvatar = MyAvatar.skeletonModelURL;
    var t0 = Date.now() / 1000;
    var samples = [];
    var threads = {};              // chain name -> joint indices (real skeleton joints only)
    var threadNames = {};          // chain name -> joint names, for the observer
    var phaseIndex = -1;
    var phase = null;
    var phaseStart = 0;
    var finished = false;

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print("FLOW-METRICS " + s); }

    var lastAvatarSet = 0;
    function flowReady() {
        // the client may put its saved avatar back while it is still starting up: insist
        if (MyAvatar.skeletonModelURL !== AVATAR_URL && now() - lastAvatarSet > 2.0) {
            lastAvatarSet = now();
            log("setting avatar (was " + MyAvatar.skeletonModelURL + ")");
            MyAvatar.skeletonModelURL = AVATAR_URL;
        }
        var data = MyAvatar.getFlowData();
        if (!data || !data.initialized || !data.active) {
            return false;
        }
        var jointNames = MyAvatar.getJointNames();
        threads = {};
        threadNames = {};
        var count = 0;
        for (var name in data.threads) {
            var indices = [];
            var names = [];
            for (var i = 0; i < data.threads[name].length; i++) {
                var index = data.threads[name][i];
                if (index < jointNames.length) {
                    indices.push(index);
                    names.push(jointNames[index]);
                }
            }
            if (indices.length > 1) {
                threads[name] = indices;
                threadNames[name] = names;
                count++;
            }
        }
        if (count > 0) {
            log("flow ready: " + count + " chains, groups " + JSON.stringify(Object.keys(data.physics)));
            // the modelled (bind pose) shape of each chain, in the avatar's frame: where the chain rests with no gravity
            for (var chainName in threads) {
                var root = MyAvatar.getAbsoluteDefaultJointTranslationInObjectFrame(threads[chainName][0]);
                var tipIndex = threads[chainName][threads[chainName].length - 1];
                var tip = MyAvatar.getAbsoluteDefaultJointTranslationInObjectFrame(tipIndex);
                var rel = Vec3.subtract(tip, root);
                log("bind pose " + chainName + " tip-root=" + JSON.stringify({ x: +rel.x.toFixed(3), y: +rel.y.toFixed(3), z: +rel.z.toFixed(3) }) +
                    " root=" + JSON.stringify({ x: +root.x.toFixed(2), y: +root.y.toFixed(2), z: +root.z.toFixed(2) }));
            }
            for (var group in data.physics) {
                var p = data.physics[group];
                log("group " + group + " stiffness=" + p.stiffness + " damping=" + p.damping + " inertia=" + p.inertia +
                    " gravity=" + p.gravity + (p.maxAngle !== undefined ? " maxAngle=" + p.maxAngle : "") +
                    " joints=" + p.jointIndices.length);
            }
        }
        return count > 0;
    }

    // On a domain the world may be empty: give the avatar a floor to stand on (the serverless world has one).
    function ensureGround() {
        var origin = { x: 0, y: 0, z: 0 };
        if (Entities.findEntitiesByName("Ground", origin, 1000).length === 0) {
            Entities.addEntity({
                type: "Box", name: "Ground", shapeType: "box", locked: false, lifetime: LIFETIME,
                position: { x: 0, y: -0.05, z: 0 }, dimensions: { x: 400, y: 0.1, z: 400 },
                color: { red: 90, green: 110, blue: 90 }, grab: { grabbable: false }
            });
            log("ground created");
        }
        MyAvatar.position = { x: 0, y: 1.5, z: 0 };
    }

    function applyOverrides() {
        if (Object.keys(FLOW_OVERRIDES).length === 0) {
            return;
        }
        MyAvatar.useFlow(true, true, FLOW_OVERRIDES, {});
        var data = MyAvatar.getFlowData();
        for (var group in FLOW_OVERRIDES) {
            var p = data.physics[group];
            if (p) {
                log("override " + group + " stiffness=" + p.stiffness + " maxAngle=" + p.maxAngle + " lift=" + p.lift + " inertia=" + p.inertia);
            }
        }
    }

    var republished = false;
    function publishThreads() {
        Messages.sendMessage(CHANNEL, JSON.stringify({ avatar: MyAvatar.sessionUUID, threads: threadNames }));
    }
    function republishThreads() {
        // once more, for an observer that was still loading when the first message went out
        if (!republished && now() - phaseStart > 1.5) {
            republished = true;
            publishThreads();
        }
    }
    function publishPhase(name) {
        Messages.sendMessage(CHANNEL, JSON.stringify({ avatar: MyAvatar.sessionUUID, phase: name }));
    }

    function startWalk() {
        MyAvatar.motorReferenceFrame = "world";
        MyAvatar.motorTimescale = 0.2;
        MyAvatar.motorVelocity = { x: WALK_SPEED, y: 0, z: 0 };
    }
    function stopWalk() {
        MyAvatar.motorVelocity = Vec3.ZERO;
    }
    function spin(dt) {
        MyAvatar.orientation = Quat.multiply(Quat.angleAxis(SPIN_RATE * dt, { x: 0, y: 1, z: 0 }), MyAvatar.orientation);
    }

    var phases = [
        { name: "boot", dur: 60, until: flowReady },
        { name: "ready", dur: 3, enter: function () { ensureGround(); applyOverrides(); publishThreads(); }, update: republishThreads },
        { name: "stand", dur: 3 },
        { name: "walk", dur: 6, enter: startWalk },
        { name: "stop", dur: 4, enter: stopWalk },
        { name: "spin", dur: 2, update: spin },
        { name: "settle", dur: 4 },
        { name: "done", dur: 3, enter: finish }
    ];

    function setPhase(index) {
        phaseIndex = index;
        phase = phases[index];
        phaseStart = now();
        log("phase " + phase.name + " at " + phaseStart.toFixed(2) + " s" +
            (Test.getGameLoopRate ? ", game loop " + Test.getGameLoopRate().toFixed(0) + " Hz, render " + Test.getRenderLoopRate().toFixed(0) + " Hz" : ""));
        // the observer on another client reads the phase from the message channel (and the display name)
        MyAvatar.displayName = "flow-" + phase.name;
        publishPhase(phase.name);
        if (phase.enter) {
            phase.enter();
        }
    }

    function record(t) {
        var ori = MyAvatar.orientation;
        var sample = { t: t, phase: phase.name, pos: [MyAvatar.position.x, MyAvatar.position.y, MyAvatar.position.z],
            ori: [ori.x, ori.y, ori.z, ori.w], threads: {} };
        for (var name in threads) {
            var indices = threads[name];
            var positions = [];
            for (var i = 0; i < indices.length; i++) {
                var p = MyAvatar.getJointPosition(indices[i]);
                positions.push([p.x, p.y, p.z]);
            }
            sample.threads[name] = positions;
        }
        samples.push(sample);
    }

    function update(dt) {
        if (finished) {
            return;
        }
        var t = now();
        if (phaseIndex < 0) {
            setPhase(0);
        }
        if (phase.update) {
            phase.update(dt);
        }
        if (phase.name !== "boot" && phase.name !== "ready" && phase.name !== "done") {
            record(t);
        }
        var elapsed = t - phaseStart;
        var over = elapsed >= phase.dur;
        if (phase.until) {
            if (phase.until()) {
                over = true;
            } else if (over) {
                log("gave up: flow never initialized on " + MyAvatar.skeletonModelURL);
                finish();
                return;
            }
        }
        if (over && phaseIndex + 1 < phases.length) {
            setPhase(phaseIndex + 1);
        }
    }

    function finish() {
        if (finished) {
            return;
        }
        finished = true;
        stopWalk();
        var summary = samples.length ? FlowMetrics.summarize(samples, threads, log, "mover") : {};
        log("samples " + samples.length + ", quitting");
        Test.saveObject({ avatar: AVATAR_URL, threads: threads, threadNames: threadNames, summary: summary, samples: samples }, "flow-metrics.json");
        MyAvatar.skeletonModelURL = previousAvatar;
        // a last word for the observer, then out
        Script.setTimeout(function () { Test.quit(); }, 2500);
    }

    log("starting, loading " + AVATAR_URL);
    MyAvatar.position = { x: 0, y: 1.5, z: 0 };
    MyAvatar.orientation = Quat.IDENTITY;
    MyAvatar.skeletonModelURL = AVATAR_URL;
    Script.update.connect(update);
    Script.scriptEnding.connect(function () { stopWalk(); });
}());
