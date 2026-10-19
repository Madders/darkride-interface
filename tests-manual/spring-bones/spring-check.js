//
//  spring-check.js
//  tests-manual/spring-bones
//
//  Loads an imported avatar with spring bones (SPRING_CHECK_AVATAR, prepended by run.sh), stands, walks, stops,
//  spins and settles, recording the chain joints each script frame: per chain and phase the tip's motion against
//  its root, the first bone's swing from its rest, and whether the chain moved at all. Lines start with SPRING-CHECK.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var AVATAR_URL = typeof SPRING_CHECK_AVATAR !== "undefined" ? SPRING_CHECK_AVATAR : "";
    var WALK_SPEED = 1.4;          // m/s
    var SPIN_RATE = 180;           // degrees/s
    var LIFETIME = 300;
    // the chains to watch: the root joint then its line to the tip (the avatar's joint names)
    var CHAINS = {
        "cat ear L": ["KemoEar.L.001", "KemoEar.L.002"],
        "cat ear R": ["KemoEar.R.001", "KemoEar.R.002"],
        "cap ear R": ["Cap_Ear.R"],
        "cap ear L": ["Cap_Ear.L"],
        "name tag": ["NameTag.Root"],
        "bag": ["Bag"],
        "tail": ["Tail.001", "Tail.002", "Tail.003", "Tail.004", "Tail.005", "Tail.006"],
        "back hair B.R": ["BackHair.B.R", "BackHair.B.R.001", "BackHair.B.R.002", "BackHair.B.R.003", "BackHair.B.R.004"],
        "side hair L": ["Hair_Side.L", "Hair_Side.L.001", "Hair_Side.L.002"],
        "ahoge": ["Ahoge.A", "Ahoge.A.001", "Ahoge.A.002"]
    };
    var previousAvatar = MyAvatar.skeletonModelURL;
    var t0 = Date.now() / 1000;
    var chains = {};               // name -> joint indices
    var samples = [];
    var phaseIndex = -1, phase = null, phaseStart = 0, finished = false, lastAvatarSet = 0, snapshots = 0;

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print("SPRING-CHECK " + s); }

    function ready() {
        if (MyAvatar.skeletonModelURL !== AVATAR_URL && now() - lastAvatarSet > 2.0) {
            lastAvatarSet = now();
            log("setting avatar (was " + MyAvatar.skeletonModelURL + ")");
            MyAvatar.skeletonModelURL = AVATAR_URL;
            return false;
        }
        var names = MyAvatar.getJointNames();
        if (!names || names.length < 10) { return false; }
        chains = {};
        var count = 0;
        for (var chain in CHAINS) {
            var indices = [];
            for (var i = 0; i < CHAINS[chain].length; i++) {
                var index = names.indexOf(CHAINS[chain][i]);
                if (index < 0) { break; }
                indices.push(index);
            }
            if (indices.length === CHAINS[chain].length) { chains[chain] = indices; count++; }
        }
        if (count > 0) { log("avatar ready: " + names.length + " joints, " + count + " of " + Object.keys(CHAINS).length + " chains found"); }
        return count > 0;
    }
    function ensureGround() {
        if (Entities.findEntitiesByName("Ground", { x: 0, y: 0, z: 0 }, 1000).length === 0) {
            Entities.addEntity({ type: "Box", name: "Ground", shapeType: "box", lifetime: LIFETIME, position: { x: 0, y: -0.05, z: 0 },
                dimensions: { x: 400, y: 0.1, z: 400 }, color: { red: 90, green: 110, blue: 90 }, grab: { grabbable: false } });
        }
        MyAvatar.position = { x: 0, y: 1.5, z: 0 };
    }
    function snapshot(name) {
        if (!Window.takeSnapshot) { return; }
        var eye = Vec3.sum(MyAvatar.position, Vec3.multiplyQbyV(MyAvatar.orientation, { x: 0.6, y: 0.5, z: -1.6 }));
        var target = Vec3.sum(MyAvatar.position, { x: 0, y: 0.45, z: 0 });
        Camera.mode = "independent";
        Camera.position = eye;
        Camera.orientation = Quat.lookAtSimple(eye, target);
        Script.setTimeout(function () { Window.takeSnapshot(false, false, 0, "spring-" + name); snapshots++; }, 300);
    }
    function startWalk() { MyAvatar.motorReferenceFrame = "world"; MyAvatar.motorTimescale = 0.2; MyAvatar.motorVelocity = { x: WALK_SPEED, y: 0, z: 0 }; }
    function stopWalk() { MyAvatar.motorVelocity = Vec3.ZERO; }
    function spin(dt) { MyAvatar.orientation = Quat.multiply(Quat.angleAxis(SPIN_RATE * dt, { x: 0, y: 1, z: 0 }), MyAvatar.orientation); }

    var phases = [
        { name: "boot", dur: 90, until: ready },
        { name: "ready", dur: 3, enter: ensureGround },
        { name: "stand", dur: 3, enter: function () { snapshot("stand"); } },
        { name: "walk", dur: 5, enter: startWalk },
        { name: "stop", dur: 4, enter: stopWalk },
        { name: "spin", dur: 2, update: spin, enter: function () { snapshot("spin"); } },
        { name: "settle", dur: 4 },
        { name: "done", dur: 3, enter: finish }
    ];
    function setPhase(index) {
        phaseIndex = index; phase = phases[index]; phaseStart = now();
        log("phase " + phase.name + " at " + phaseStart.toFixed(2) + " s" + (Test.getGameLoopRate ? ", game loop " + Test.getGameLoopRate().toFixed(0) + " Hz" : ""));
        if (phase.enter) { phase.enter(); }
    }
    function record(t) {
        var sample = { t: t, phase: phase.name, chains: {} };
        for (var name in chains) {
            var indices = chains[name];
            var joints = [];
            for (var i = 0; i < indices.length; i++) {
                var p = MyAvatar.getJointPosition(indices[i]);
                var r = MyAvatar.getJointRotation(indices[i]);
                joints.push({ p: [p.x, p.y, p.z], r: [r.x, r.y, r.z, r.w] });
            }
            sample.chains[name] = joints;
        }
        samples.push(sample);
    }
    function update(dt) {
        if (finished) { return; }
        var t = now();
        if (phaseIndex < 0) { setPhase(0); }
        if (phase.update) { phase.update(dt); }
        if (phase.name !== "boot" && phase.name !== "ready" && phase.name !== "done") { record(t); }
        var over = t - phaseStart >= phase.dur;
        if (phase.until) {
            if (phase.until()) { over = true; } else if (over) { log("gave up: the avatar never loaded: " + MyAvatar.skeletonModelURL); finish(); return; }
        }
        if (over && phaseIndex + 1 < phases.length) { setPhase(phaseIndex + 1); }
    }
    function angleBetween(a, b) {
        // degrees between two quaternions
        var d = Quat.multiply(Quat.inverse({ x: a[0], y: a[1], z: a[2], w: a[3] }), { x: b[0], y: b[1], z: b[2], w: b[3] });
        var w = Math.min(1, Math.abs(d.w));
        return 2 * Math.acos(w) * 180 / Math.PI;
    }
    function summarize() {
        var names = ["stand", "walk", "stop", "spin", "settle"];
        for (var chain in chains) {
            var line = chain + ":";
            var first = null;
            for (var i = 0; i < samples.length; i++) { if (samples[i].chains[chain]) { first = samples[i].chains[chain]; break; } }
            if (!first) { continue; }
            for (var k = 0; k < names.length; k++) {
                var speeds = [], swings = [], moved = 0, n = 0, prev = null;
                for (var j = 0; j < samples.length; j++) {
                    var s = samples[j];
                    if (s.phase !== names[k] || !s.chains[chain]) { prev = null; continue; }
                    var joints = s.chains[chain];
                    var tip = joints[joints.length - 1].p, root = joints[0].p;
                    var rel = [tip[0] - root[0], tip[1] - root[1], tip[2] - root[2]];
                    if (prev) {
                        var dtp = s.t - prev.t;
                        if (dtp > 0) {
                            var d = [rel[0] - prev.rel[0], rel[1] - prev.rel[1], rel[2] - prev.rel[2]];
                            speeds.push(Math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / dtp);
                        }
                    }
                    swings.push(angleBetween(first[0].r, joints[0].r));
                    prev = { t: s.t, rel: rel };
                    n++;
                }
                var meanSpeed = 0; for (var a = 0; a < speeds.length; a++) { meanSpeed += speeds[a]; } meanSpeed = speeds.length ? meanSpeed / speeds.length : 0;
                var maxSwing = 0; for (var b = 0; b < swings.length; b++) { if (swings[b] > maxSwing) { maxSwing = swings[b]; } }
                line += " " + names[k] + " tip " + (1000 * meanSpeed).toFixed(1) + " mm/s, root swing to " + maxSwing.toFixed(1) + " deg (" + n + ");";
            }
            log(line);
        }
    }
    function finish() {
        if (finished) { return; }
        finished = true;
        stopWalk();
        summarize();
        log("samples " + samples.length + ", snapshots " + snapshots + ", quitting");
        Test.saveObject({ avatar: AVATAR_URL, chains: chains, samples: samples }, "spring-check.json");
        MyAvatar.skeletonModelURL = previousAvatar;
        Script.setTimeout(function () { Test.quit(); }, 2500);
    }
    log("starting, loading " + AVATAR_URL);
    MyAvatar.position = { x: 0, y: 1.5, z: 0 };
    MyAvatar.orientation = Quat.IDENTITY;
    MyAvatar.skeletonModelURL = AVATAR_URL;
    Script.update.connect(update);
    Script.scriptEnding.connect(function () { stopWalk(); });
}());
