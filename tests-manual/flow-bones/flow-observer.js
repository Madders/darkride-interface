//
//  flow-observer.js
//  tests-manual/flow-bones
//
//  The second client of run-pair.sh: watch the avatar running flow-metrics.js on a domain and measure its flow
//  chains from what arrives over the network. The chains (joint names per chain) and the phase come on the
//  "flow-metrics" message channel. Every script frame the world
//  position of every chain joint is sampled from the other avatar; the summary (lines starting with
//  FLOW-OBSERVER) uses the same arithmetic as the mover's, so the two can be put side by side: if the chain
//  others see moves like the chain the sender sees, the simulation is reaching the network.
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

    var CHANNEL = "flow-metrics";
    var GIVE_UP = 120;             // s without a mover
    var t0 = Date.now() / 1000;
    var samples = [];
    var threads = null;            // chain name -> joint indices on the mover's skeleton
    var threadNames = null;
    var moverID = null;
    var lastPhase = null;
    var messagePhase = null;       // the mover's phase as published on the channel
    var chainsLogged = false;
    var finished = false;
    var rateSamples = [];

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print("FLOW-OBSERVER " + s); }

    function findMover() {
        var ids = AvatarList.getAvatarIdentifiers();
        for (var i = 0; i < ids.length; i++) {
            var a = AvatarList.getAvatar(ids[i]);
            if (a && a.displayName && a.displayName.indexOf("flow-") === 0) {
                return ids[i];
            }
        }
        return null;
    }

    function resolveThreads(avatar) {
        if (!threadNames) {
            return false;
        }
        var jointNames = avatar.jointNames;
        if (!jointNames || jointNames.length === 0) {
            return false;
        }
        var resolved = {};
        var count = 0;
        for (var name in threadNames) {
            var indices = [];
            for (var i = 0; i < threadNames[name].length; i++) {
                var index = jointNames.indexOf(threadNames[name][i]);
                if (index < 0) {
                    return false;   // the model is not loaded yet on this client
                }
                indices.push(index);
            }
            resolved[name] = indices;
            count++;
        }
        if (count === 0) {
            return false;
        }
        threads = resolved;
        log("chains resolved on " + moverID + ": " + count + ", " + jointNames.length + " joints");
        return true;
    }

    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(function (channel, message, sender) {
        if (channel !== CHANNEL) {
            return;
        }
        try {
            var data = JSON.parse(message);
            if (data.threads) {
                threadNames = data.threads;
                moverID = data.avatar;
                if (!chainsLogged) {
                    chainsLogged = true;
                    log("chains received from " + sender + ": " + Object.keys(data.threads).length);
                }
            }
            if (data.phase) {
                messagePhase = data.phase;
                moverID = data.avatar;
            }
        } catch (e) {
            log("bad message: " + e);
        }
    });

    function update() {
        if (finished) {
            return;
        }
        var t = now();
        if (!moverID) {
            moverID = findMover();
            if (!moverID && t > GIVE_UP) {
                log("gave up: no mover seen in " + GIVE_UP + " s");
                finish();
            }
            return;
        }
        var avatar = AvatarList.getAvatar(moverID);
        if (!avatar || (!avatar.displayName && !messagePhase)) {
            if (lastPhase === "done" || t > GIVE_UP * 2) {
                finish();
            }
            return;
        }
        var phase = messagePhase || (avatar.displayName.indexOf("flow-") === 0 ? avatar.displayName.substring(5) : null);
        if (phase !== lastPhase) {
            log("mover phase " + phase + " at " + t.toFixed(2) + " s");
            lastPhase = phase;
            if (phase === "done") {
                finish();
                return;
            }
        }
        if (!threads && !resolveThreads(avatar)) {
            return;
        }
        if (!phase || phase === "boot" || phase === "ready") {
            return;
        }
        var sample = { t: t, phase: phase, pos: [avatar.position.x, avatar.position.y, avatar.position.z], threads: {} };
        for (var name in threads) {
            var positions = [];
            for (var i = 0; i < threads[name].length; i++) {
                var p = avatar.getJointPosition(threads[name][i]);
                positions.push([p.x, p.y, p.z]);
            }
            sample.threads[name] = positions;
        }
        samples.push(sample);
        if (avatar.getSimulationRate) {
            rateSamples.push(avatar.getSimulationRate("jointData"));
        }
    }

    function finish() {
        if (finished) {
            return;
        }
        finished = true;
        var summary = (samples.length && threads) ? FlowMetrics.summarize(samples, threads, log, "observer") : {};
        if (rateSamples.length) {
            var sum = 0;
            for (var i = 0; i < rateSamples.length; i++) { sum += rateSamples[i]; }
            log("joint data received at " + (sum / rateSamples.length).toFixed(1) + " Hz on average");
        }
        log("samples " + samples.length + ", quitting");
        Test.saveObject({ mover: moverID, threads: threads, summary: summary, samples: samples }, "flow-observer.json");
        Script.setTimeout(function () { Test.quit(); }, 1000);
    }

    log("starting, waiting for an avatar named flow-*");
    MyAvatar.position = { x: 0, y: 1.5, z: 3 };
    Script.update.connect(update);
}());
