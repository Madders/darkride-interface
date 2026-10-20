//
//  seat-jitter.js
//  tests-manual/physics-baseline
//
//  The driver's own view of the car, frame by frame (Caity, 2026-10-19: "a very subtle stutter many times a second while
//  I drive"). Makes the Land Rover beside the pad (landrover.js, concatenated before this file by seat-jitter.sh), sits in
//  the driver's seat as vehicles.js does and drives straight at a steady throttle, sampling every 2 ms (faster than any
//  game loop) the chassis's shown position and rotation, the avatar's world position (what the camera rides) and the
//  camera's. Distinct chassis positions are the game frames. The summary: the chassis's step per frame (mean, rms
//  deviation, max, the first steps in a row: a 90 Hz substep against a 60 Hz frame shows as alternating steps), the
//  seat's position in the chassis's frame per frame (its rms deviation and max: nonzero means the camera and the car are
//  read a frame apart, which the eye sees as a shimmer), the camera's likewise, and the frame gaps. Serverless (this
//  client the authority) or on a domain with a physics server; the two runs tell presentation from networking.
//
//  Distributed under the Apache License, Version 2.0.
//
(function () {
    var config = (typeof ROVER_CONFIG !== "undefined" && ROVER_CONFIG) ? ROVER_CONFIG : {};
    var TERRAIN_URL = config.terrainURL || "";
    var LIFETIME = config.lifetime || 600;
    var OFFSET_X = config.offsetX || 0;      // the car this far from the pad's centre (another car may be on the pad)
    var THROTTLE = config.throttle || 0.35;
    var DRIVE_SECONDS = config.seconds || 10;
    var BOOT = (config.boot !== undefined) ? config.boot : 8;
    var SIT_ANIMATION_URL = "https://s3-us-west-1.amazonaws.com/hifi-content/clement/production/animations/sitting_idle.fbx";
    var DRIVE_KEYS = [DriveKeys.TRANSLATE_X, DriveKeys.TRANSLATE_Y, DriveKeys.TRANSLATE_Z, DriveKeys.YAW, DriveKeys.STEP_YAW,
                      DriveKeys.STEP_TRANSLATE_X, DriveKeys.STEP_TRANSLATE_Y, DriveKeys.STEP_TRANSLATE_Z];
    var terrain = null;
    var chassis = null;
    var seat = null;
    var samples = [];
    var sampler = null;
    var t0 = Date.now();
    var tries = 0;
    function log(s) { print("SEAT-JITTER " + s); }
    function vec3Of(value) {
        if (Array.isArray(value) && value.length >= 3) { return { x: value[0], y: value[1], z: value[2] }; }
        return value;
    }
    function fmtV(v) { return "(" + v.x.toFixed(3) + ", " + v.y.toFixed(3) + ", " + v.z.toFixed(3) + ")"; }
    function rolesToOverride() {
        return MyAvatar.getAnimationRoles().filter(function (role) { return !(role.indexOf("right") === 0 || role.indexOf("left") === 0); });
    }
    function sit(seatSpec) {
        seat = seatSpec;
        MyAvatar.collisionsEnabled = false;
        MyAvatar.hmdLeanRecenterEnabled = false;
        rolesToOverride().forEach(function (role) { MyAvatar.overrideRoleAnimation(role, SIT_ANIMATION_URL, 30, true, 1, 350); });
        DRIVE_KEYS.forEach(function (key) { MyAvatar.disableDriveKey(key); });
        MyAvatar.parentID = chassis;
        MyAvatar.parentJointIndex = 65535;
        MyAvatar.localPosition = vec3Of(seat.position);
        MyAvatar.localOrientation = Quat.fromPitchYawRollDegrees(0, seat.yaw || 0, 0);
        Entities.editEntity(chassis, { vehicle: { driver: MyAvatar.sessionUUID } });
        log("seated as the driver, local " + fmtV(MyAvatar.localPosition) + ", world " + fmtV(MyAvatar.position));
    }
    function stand() {
        Entities.clearVehicleControls(chassis);
        Entities.editEntity(chassis, { vehicle: { driver: Uuid.NULL } });
        var position = MyAvatar.position;
        MyAvatar.parentID = Uuid.NULL;
        MyAvatar.position = Vec3.sum(position, { x: 0, y: 0.5, z: 2 });
        DRIVE_KEYS.forEach(function (key) { MyAvatar.enableDriveKey(key); });
        rolesToOverride().forEach(function (role) { MyAvatar.restoreRoleAnimation(role); });
        MyAvatar.collisionsEnabled = true;
        MyAvatar.hmdLeanRecenterEnabled = true;
    }
    function groundReady(x, z) {
        var expected = LandRover.groundHeight(x, z);
        var ray = { origin: { x: x, y: expected + 30, z: z }, direction: { x: 0, y: -1, z: 0 } };
        var hit = Entities.findRayIntersection(ray, true, [terrain], [], false, true);
        return hit && hit.intersects && hit.entityID == terrain && Math.abs(hit.intersection.y - expected) < 0.5;
    }
    function sample() {
        var c = Entities.getEntityProperties(chassis, ["position", "rotation", "velocity"]);
        if (!c || !c.position) { return; }
        var a = MyAvatar.position;
        var cam = Camera.position;
        var eye = MyAvatar.getDefaultEyePosition();   // what the camera is built from: the skeleton's, refreshed when the avatar simulates
        var head = MyAvatar.getHeadPosition();
        samples.push([Date.now() - t0, c.position.x, c.position.y, c.position.z, c.rotation.x, c.rotation.y, c.rotation.z, c.rotation.w,
                      a.x, a.y, a.z, cam.x, cam.y, cam.z, Vec3.length(c.velocity), eye.x, eye.y, eye.z, head.x, head.y, head.z]);
    }
    function stats(values) {
        var n = values.length;
        if (n === 0) { return { mean: 0, rms: 0, max: 0, n: 0 }; }
        var mean = 0, max = -Infinity;
        values.forEach(function (v) { mean += v; if (v > max) { max = v; } });
        mean /= n;
        var rms = 0;
        values.forEach(function (v) { rms += (v - mean) * (v - mean); });
        return { mean: mean, rms: Math.sqrt(rms / n), max: max, n: n };
    }
    function localOf(row, px, py, pz) {
        var rot = { x: row[4], y: row[5], z: row[6], w: row[7] };
        var d = { x: px - row[1], y: py - row[2], z: pz - row[3] };
        return Vec3.multiplyQbyV(Quat.inverse(rot), d);
    }
    function finish(reason) {
        if (sampler) { Script.clearInterval(sampler); sampler = null; }
        log("finish: " + reason + ", " + samples.length + " samples");
        // distinct chassis positions are the game frames; the middle of the drive, where the speed is steady
        var frames = [];
        for (var i = 1; i < samples.length; i++) {
            if (samples[i][1] !== samples[i - 1][1] || samples[i][3] !== samples[i - 1][3] || samples[i][2] !== samples[i - 1][2]) {
                frames.push(samples[i]);
            }
        }
        var steps = [], gaps = [], seatX = [], seatY = [], seatZ = [], camX = [], camY = [], camZ = [], speeds = [];
        var tStart = frames.length ? frames[0][0] : 0;
        var tEnd = frames.length ? frames[frames.length - 1][0] : 0;
        var lo = tStart + (tEnd - tStart) * 0.3, hi = tStart + (tEnd - tStart) * 0.9;
        var stepsInRow = [];
        for (var k = 1; k < frames.length; k++) {
            var f = frames[k], g = frames[k - 1];
            if (f[0] < lo || f[0] > hi) { continue; }
            var dx = f[1] - g[1], dy = f[2] - g[2], dz = f[3] - g[3];
            var step = 1000 * Math.sqrt(dx * dx + dy * dy + dz * dz);
            steps.push(step);
            if (stepsInRow.length < 40) { stepsInRow.push(step.toFixed(0)); }
            gaps.push(f[0] - g[0]);
            var s = localOf(f, f[8], f[9], f[10]);
            seatX.push(1000 * s.x); seatY.push(1000 * s.y); seatZ.push(1000 * s.z);
            var c = localOf(f, f[11], f[12], f[13]);
            camX.push(1000 * c.x); camY.push(1000 * c.y); camZ.push(1000 * c.z);
            speeds.push(f[14]);
        }
        var st = stats(steps), gp = stats(gaps), sp = stats(speeds);
        var sx = stats(seatX), sy = stats(seatY), sz = stats(seatZ);
        var cx = stats(camX), cy = stats(camY), cz = stats(camZ);
        var over = 0;
        steps.forEach(function (v) { if (v > 1.5 * st.mean || v < 0.5 * st.mean) { over++; } });
        // the camera's offset in the chassis's frame, frame to frame: what the eye in the seat sees of the car; a
        // camera that moves with the car changes it by nothing, one read a frame apart from the car shimmers
        var camChange = [], camChangeZ = [], camSteps = [], carSteps = [];
        for (var m = 1; m < camX.length; m++) {
            var ddx = camX[m] - camX[m - 1], ddy = camY[m] - camY[m - 1], ddz = camZ[m] - camZ[m - 1];
            camChange.push(Math.sqrt(ddx * ddx + ddy * ddy + ddz * ddz));
            camChangeZ.push(ddz);
        }
        var flips = 0;
        for (m = 1; m < camChangeZ.length; m++) { if (camChangeZ[m] * camChangeZ[m - 1] < 0) { flips++; } }
        for (k = 1; k < frames.length; k++) {
            if (frames[k][0] < lo || frames[k][0] > hi) { continue; }
            camSteps.push(1000 * Vec3.distance({ x: frames[k][11], y: frames[k][12], z: frames[k][13] }, { x: frames[k - 1][11], y: frames[k - 1][12], z: frames[k - 1][13] }));
            carSteps.push(1000 * Vec3.distance({ x: frames[k][1], y: frames[k][2], z: frames[k][3] }, { x: frames[k - 1][1], y: frames[k - 1][2], z: frames[k - 1][3] }));
        }
        var eyeSteps = [], headSteps = [], avatarSteps = [];
        for (k = 1; k < frames.length; k++) {
            if (frames[k][0] < lo || frames[k][0] > hi) { continue; }
            var fk = frames[k], fj = frames[k - 1];
            eyeSteps.push(1000 * Vec3.distance({ x: fk[15], y: fk[16], z: fk[17] }, { x: fj[15], y: fj[16], z: fj[17] }));
            headSteps.push(1000 * Vec3.distance({ x: fk[18], y: fk[19], z: fk[20] }, { x: fj[18], y: fj[19], z: fj[20] }));
            avatarSteps.push(1000 * Vec3.distance({ x: fk[8], y: fk[9], z: fk[10] }, { x: fj[8], y: fj[9], z: fj[10] }));
        }
        var es = stats(eyeSteps), hs = stats(headSteps), as = stats(avatarSteps);
        var cc = stats(camChange), cs = stats(camSteps);
        var seatDev = Math.sqrt(sx.rms * sx.rms + sy.rms * sy.rms + sz.rms * sz.rms);
        var camDev = Math.sqrt(cx.rms * cx.rms + cy.rms * cy.rms + cz.rms * cz.rms);
        var seatMax = 0, camMax = 0;
        for (var j = 0; j < seatX.length; j++) {
            var ds = Math.sqrt(Math.pow(seatX[j] - sx.mean, 2) + Math.pow(seatY[j] - sy.mean, 2) + Math.pow(seatZ[j] - sz.mean, 2));
            var dc = Math.sqrt(Math.pow(camX[j] - cx.mean, 2) + Math.pow(camY[j] - cy.mean, 2) + Math.pow(camZ[j] - cz.mean, 2));
            if (ds > seatMax) { seatMax = ds; }
            if (dc > camMax) { camMax = dc; }
        }
        log("game loop " + (Test.getGameLoopRate ? Test.getGameLoopRate().toFixed(0) : "?") + " Hz, render " +
            (Test.getRenderLoopRate ? Test.getRenderLoopRate().toFixed(0) : "?") + " Hz; " + frames.length + " frames in " + samples.length +
            " samples; speed " + sp.mean.toFixed(2) + " m/s");
        log("chassis step per frame: mean " + st.mean.toFixed(1) + " mm, rms deviation " + st.rms.toFixed(1) + " mm (" +
            (st.mean > 0 ? (100 * st.rms / st.mean).toFixed(0) : "?") + "%), max " + st.max.toFixed(1) + " mm, " + over + " of " + st.n +
            " beyond half or one and a half times the mean; frame gap mean " + gp.mean.toFixed(1) + " ms, rms " + gp.rms.toFixed(1) + " ms");
        log("steps in a row (mm): " + stepsInRow.join(" "));
        log("seat in the chassis's frame: mean (" + sx.mean.toFixed(1) + ", " + sy.mean.toFixed(1) + ", " + sz.mean.toFixed(1) +
            ") mm, rms deviation " + seatDev.toFixed(1) + " mm, worst " + seatMax.toFixed(1) + " mm");
        log("camera in the chassis's frame: mean (" + cx.mean.toFixed(1) + ", " + cy.mean.toFixed(1) + ", " + cz.mean.toFixed(1) +
            ") mm, rms deviation " + camDev.toFixed(1) + " mm, worst " + camMax.toFixed(1) + " mm");
        log("camera offset change per frame: mean " + cc.mean.toFixed(1) + " mm, rms " + cc.rms.toFixed(1) + " mm, max " + cc.max.toFixed(1) +
            " mm; sign flips along the car's forward " + flips + " of " + camChangeZ.length + "; camera's own step per frame mean " +
            cs.mean.toFixed(1) + " mm, rms " + cs.rms.toFixed(1) + " mm");
        log("camera change along forward in a row (mm): " + camChangeZ.slice(0, 40).map(function (v) { return (v >= 0 ? "+" : "") + v.toFixed(0); }).join(" "));
        log("steps per frame, mean/rms/max mm: avatar position " + as.mean.toFixed(1) + "/" + as.rms.toFixed(1) + "/" + as.max.toFixed(1) +
            ", default eye " + es.mean.toFixed(1) + "/" + es.rms.toFixed(1) + "/" + es.max.toFixed(1) + ", head " + hs.mean.toFixed(1) + "/" +
            hs.rms.toFixed(1) + "/" + hs.max.toFixed(1) + ", camera " + cs.mean.toFixed(1) + "/" + cs.rms.toFixed(1) + "/" + cs.max.toFixed(1));
        log("car/eye/camera steps in a row (mm): " + carSteps.slice(0, 24).map(function (v, i) { return v.toFixed(0) + "/" + eyeSteps[i].toFixed(0) + "/" + camSteps[i].toFixed(0); }).join(" "));
        log("car/camera steps in a row (mm): " + carSteps.slice(0, 30).map(function (v, i) { return v.toFixed(0) + "/" + camSteps[i].toFixed(0); }).join(" "));
        renderTraceSummary();
        var stats2 = Entities.getVehicleStats ? Entities.getVehicleStats() : {};
        log("vehicle stats: " + JSON.stringify(stats2));
        Test.saveObject({ reason: reason, samples: samples, summary: { step: st, gap: gp, seatDev: seatDev, seatMax: seatMax, camDev: camDev, camMax: camMax, speed: sp, stats: stats2 } }, "seat-jitter.json");
        stand();
        Script.setTimeout(function () { Test.quit(); }, 1500);
    }
    function renderTraceSummary() {
        if (!Test.takeRenderTrace) { return; }
        var trace = Test.takeRenderTrace();
        if (!trace.length) { log("render trace: none"); return; }
        var t0 = trace[0][0], t1 = trace[trace.length - 1][0];
        var lo = t0 + 0.3 * (t1 - t0), hi = t0 + 0.9 * (t1 - t0);
        var itemSteps = [], camSteps = [], gaps = [], offsetChanges = [], along = [], inRow = [];
        for (var i = 1; i < trace.length; i++) {
            var a = trace[i - 1], b = trace[i];
            if (b[0] < lo || b[0] > hi) { continue; }
            var move = { x: b[4] - a[4], y: b[5] - a[5], z: b[6] - a[6] };
            var itemStep = 1000 * Vec3.length(move);
            var camStep = 1000 * Vec3.distance({ x: b[1], y: b[2], z: b[3] }, { x: a[1], y: a[2], z: a[3] });
            // the item's offset from the camera, and how it changed this frame, along the item's motion
            var offA = { x: a[4] - a[1], y: a[5] - a[2], z: a[6] - a[3] }, offB = { x: b[4] - b[1], y: b[5] - b[2], z: b[6] - b[3] };
            var change = { x: offB.x - offA.x, y: offB.y - offA.y, z: offB.z - offA.z };
            var dir = itemStep > 0 ? Vec3.normalize(move) : { x: 0, y: 0, z: 0 };
            itemSteps.push(itemStep); camSteps.push(camStep); gaps.push(b[0] - a[0]);
            offsetChanges.push(1000 * Vec3.length(change));
            along.push(1000 * Vec3.dot(change, dir));
            if (inRow.length < 30) { inRow.push(itemStep.toFixed(0) + "/" + camStep.toFixed(0)); }
        }
        var is = stats(itemSteps), cs = stats(camSteps), gs = stats(gaps), oc = stats(offsetChanges);
        var flips = 0, big = 0;
        for (i = 1; i < along.length; i++) { if (along[i] * along[i - 1] < 0) { flips++; } }
        for (i = 0; i < offsetChanges.length; i++) { if (offsetChanges[i] > 8) { big++; } }
        log("RENDERED frames: " + itemSteps.length + " in the window, gap mean " + gs.mean.toFixed(2) + " ms rms " + gs.rms.toFixed(2) +
            " ms; car step per rendered frame mean " + is.mean.toFixed(1) + " mm rms " + is.rms.toFixed(1) + " max " + is.max.toFixed(1) +
            "; camera step mean " + cs.mean.toFixed(1) + " rms " + cs.rms.toFixed(1) + " max " + cs.max.toFixed(1));
        log("RENDERED car-to-camera offset change per frame: mean " + oc.mean.toFixed(1) + " mm, rms " + oc.rms.toFixed(1) + ", max " +
            oc.max.toFixed(1) + "; frames with a change over 8 mm: " + big + " of " + offsetChanges.length + "; sign flips along the motion " + flips);
        log("RENDERED car/camera steps in a row (mm): " + inRow.join(" "));
        log("RENDERED offset change along the motion in a row (mm): " + along.slice(0, 40).map(function (v) { return (v >= 0 ? "+" : "") + v.toFixed(0); }).join(" "));
    }
    function drive() {
        if (Test.traceRenderedEntity) { log("render trace " + (Test.traceRenderedEntity(chassis) ? "on" : "unavailable")); }
        Entities.setVehicleControls(chassis, { throttle: THROTTLE, steering: 0, brake: 0, handBrake: 0 });
        log("driving straight at throttle " + THROTTLE + " for " + DRIVE_SECONDS + " s");
        sampler = Script.setInterval(sample, 2);
        Script.setTimeout(function () { finish("done"); }, DRIVE_SECONDS * 1000);
    }
    function makeCar() {
        var pad = LandRover.TERRAIN_CENTER;
        if (config.clear && tries === 0) {
            var removed = LandRover.clearVehicles();
            if (removed) { log("cleared " + removed + " vehicle entities of an earlier run"); }
        }
        var x = pad.x + OFFSET_X;
        if (!groundReady(x, pad.z)) {
            if (++tries < 60) { Script.setTimeout(makeCar, 1000); } else { log("no ground"); Test.quit(); }
            return;
        }
        MyAvatar.position = { x: x + 3, y: LandRover.groundHeight(x + 3, pad.z) + 1.2, z: pad.z };
        chassis = LandRover.create(x, pad.z, 0, LIFETIME).chassis;
        log("rover " + chassis + " at x " + x + ", z " + pad.z);
        Script.setTimeout(function () {
            var s = Entities.getEntityProperties(chassis, ["position", "velocity"]);
            log("settled: chassis " + fmtV(s.position) + ", speed " + Vec3.length(s.velocity).toFixed(3));
            sit(LandRover.SEATS[0]);
            Script.setTimeout(drive, 3000);
        }, 4000);
    }
    function setUp() {
        terrain = LandRover.createTerrain(TERRAIN_URL, LIFETIME);
        if (!terrain || terrain === Uuid.NULL) {
            terrain = null;
            if (++tries < 60) { Script.setTimeout(setUp, 1000); } else { log("no entity server; giving up"); Test.quit(); }
            return;
        }
        tries = 0;
        log("terrain " + terrain + ", file " + TERRAIN_URL);
        Script.setTimeout(makeCar, 1000);
    }
    Script.setTimeout(setUp, BOOT * 1000);
    Script.setTimeout(function () { log("watchdog"); finish("watchdog"); }, 120000);
})();
