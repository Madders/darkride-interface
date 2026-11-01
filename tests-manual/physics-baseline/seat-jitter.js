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
        var renderTrace = renderTraceSummary();
        var presentTrace = presentTraceSummary(renderTrace);
        var refreshTrace = displayedSummary(presentTrace, renderTrace);
        var stats2 = Entities.getVehicleStats ? Entities.getVehicleStats() : {};
        log("vehicle stats: " + JSON.stringify(stats2));
        Test.saveObject({ reason: reason, samples: samples, renderTrace: renderTrace, presentTrace: presentTrace, refreshTrace: refreshTrace,
                          summary: { step: st, gap: gp, seatDev: seatDev, seatMax: seatMax, camDev: camDev, camMax: camMax, speed: sp,
                                     presented: presented, displayed: displayed, stats: stats2 } }, "seat-jitter.json");
        stand();
        Script.setTimeout(function () { Test.quit(); }, 1500);
    }
    function renderTraceSummary() {
        if (!Test.takeRenderTrace) { return []; }
        var trace = Test.takeRenderTrace();
        if (!trace.length) { log("render trace: none"); return trace; }
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
        return trace;
    }
    // The presents (Test.takePresentTrace, the editor renderer's frame-pacing round): each sample is the swap's return,
    // which with vsync on is the display's refresh, and the rendered frame it showed. Paired with the render trace by
    // frame index, the car's step per PRESENTED frame against the gap between presents is the eye's own measure: the
    // shown speed (step over gap) should be the same every frame, whatever the gaps did.
    var presented = null;
    function presentTraceSummary(renderTrace) {
        if (!Test.takePresentTrace) { return []; }
        var presents = Test.takePresentTrace();
        Test.tracePresents(false);
        if (!presents.length) { log("present trace: none"); return presents; }
        var hz = Test.getDisplayRefreshRate ? Test.getDisplayRefreshRate() : 0;
        var t0 = presents[0][0], t1 = presents[presents.length - 1][0];
        var lo = t0 + 0.3 * (t1 - t0), hi = t0 + 0.9 * (t1 - t0);
        var itemByFrame = {};
        renderTrace.forEach(function (s) { if (s.length > 7) { itemByFrame[s[7]] = { x: s[4], y: s[5], z: s[6] }; } });
        var gaps = [], steps = [], shownSpeeds = [], pairs = [], gapRow = [], stepRow = [], byRefresh = {};
        var presentCost = [], swapCost = [], gpuCost = [];
        var n = 0, repeats = 0, within = 0;
        for (var i = 1; i < presents.length; i++) {
            var a = presents[i - 1], b = presents[i];
            if (b[0] < lo || b[0] > hi) { continue; }
            n++;
            var gap = b[0] - a[0];
            gaps.push(gap);
            if (b.length > 5) { presentCost.push(b[3]); swapCost.push(b[4]); gpuCost.push(b[5]); }
            if (gapRow.length < 40) { gapRow.push(gap.toFixed(1)); }
            if (!b[2]) { repeats++; }
            if (hz > 0) {
                var r = gap * hz / 1000;
                var k = Math.round(r);
                byRefresh[k] = (byRefresh[k] || 0) + 1;
                if (Math.abs(r - k) < 0.25) { within++; }
            }
            var pa = itemByFrame[a[1]], pb = itemByFrame[b[1]];
            if (pa && pb && gap > 0) {
                var step = 1000 * Vec3.distance(pa, pb);
                steps.push(step);
                shownSpeeds.push(step / gap);   // m/s as shown: mm per ms
                pairs.push([gap, step]);
                if (stepRow.length < 40) { stepRow.push(step.toFixed(0)); }
            }
        }
        var gs = stats(gaps), ss = stats(steps), vs = stats(shownSpeeds);
        var corr = 0;
        if (pairs.length > 2) {
            var mg = 0, ms = 0;
            pairs.forEach(function (p) { mg += p[0]; ms += p[1]; });
            mg /= pairs.length; ms /= pairs.length;
            var sgg = 0, sss = 0, sgs = 0;
            pairs.forEach(function (p) { sgg += (p[0] - mg) * (p[0] - mg); sss += (p[1] - ms) * (p[1] - ms); sgs += (p[0] - mg) * (p[1] - ms); });
            corr = (sgg > 0 && sss > 0) ? sgs / Math.sqrt(sgg * sss) : 0;
        }
        log("PRESENTED frames: " + n + " in the window, gap mean " + gs.mean.toFixed(2) + " ms rms " + gs.rms.toFixed(2) + " max " +
            gs.max.toFixed(1) + "; repeats (no new frame) " + repeats + "; display " + hz.toFixed(0) + " Hz, vsync " +
            (Test.isVsyncEnabled ? Test.isVsyncEnabled() : "?") + "; gaps by whole refreshes " + JSON.stringify(byRefresh) +
            ", within a quarter refresh of a whole number " + within + " of " + gaps.length);
        log("PRESENTED car step per present mean " + ss.mean.toFixed(1) + " mm rms " + ss.rms.toFixed(1) + " max " + ss.max.toFixed(1) +
            "; step-to-gap correlation " + corr.toFixed(2) + " (1 = each frame advances by its own interval); shown speed mean " +
            vs.mean.toFixed(2) + " m/s rms " + vs.rms.toFixed(2) + " (" + (vs.mean > 0 ? (100 * vs.rms / vs.mean).toFixed(0) : "?") + "%)");
        log("PRESENTED gaps in a row (ms): " + gapRow.join(" "));
        log("PRESENTED car steps in a row (mm): " + stepRow.join(" "));
        var pc = stats(presentCost), sc = stats(swapCost), gc = stats(gpuCost);
        if (presentCost.length) {
            // what a present costs: its whole duration, the swap's share (the wait for the display, with vsync), and the
            // GPU's time per frame; the present less the swap is the thread's own work, what must fit in a refresh
            log("PRESENTED cost: present mean " + pc.mean.toFixed(2) + " ms rms " + pc.rms.toFixed(2) + " max " + pc.max.toFixed(1) +
                "; swap mean " + sc.mean.toFixed(2) + " max " + sc.max.toFixed(1) + "; present less swap mean " + (pc.mean - sc.mean).toFixed(2) +
                "; GPU per frame mean " + gc.mean.toFixed(2) + " ms max " + gc.max.toFixed(1));
        }
        presented = { n: n, gap: gs, step: ss, shownSpeed: vs, correlation: corr, repeats: repeats, displayHz: hz, byRefresh: byRefresh, within: within,
                      presentCost: pc, swapCost: sc, gpuCost: gc };
        return presents;
    }
    // The display's own refreshes (Test.takeRefreshTrace, macOS's display link), against the presents: at each refresh
    // the compositor shows the newest frame flushed before it, so the frame shown per refresh follows from the swap's
    // return times. A refresh showing the frame before's again is a repeat, one with a frame never shown between is a
    // skip; the car's step per refresh is what the eye gets, the measure that matters.
    var displayed = null;
    function displayedSummary(presents, renderTrace) {
        if (!Test.takeRefreshTrace) { return []; }
        var refreshes = Test.takeRefreshTrace();
        Test.traceRefreshes(false);
        if (!refreshes.length || !presents.length) { log("refresh trace: none"); return refreshes; }
        var itemByFrame = {};
        renderTrace.forEach(function (s) { if (s.length > 7) { itemByFrame[s[7]] = { x: s[4], y: s[5], z: s[6] }; } });
        var t0 = presents[0][0], t1 = presents[presents.length - 1][0];
        var lo = t0 + 0.3 * (t1 - t0), hi = t0 + 0.9 * (t1 - t0);
        var rgaps = [], shown = [], j = 0, n = 0;
        for (var i = 1; i < refreshes.length; i++) {
            var v = refreshes[i];
            if (v < lo || v > hi) { continue; }
            rgaps.push(v - refreshes[i - 1]);
            while (j + 1 < presents.length && presents[j + 1][0] <= v) { j++; }
            if (presents[j][0] <= v) { shown.push(presents[j][1]); n++; }
        }
        var repeats = 0, skips = 0, steps = [], stepRow = [];
        for (i = 1; i < shown.length; i++) {
            if (shown[i] === shown[i - 1]) { repeats++; }
            else if (shown[i] - shown[i - 1] >= 2) { skips++; }
            var pa = itemByFrame[shown[i - 1]], pb = itemByFrame[shown[i]];
            if (pa && pb) {
                var step = 1000 * Vec3.distance(pa, pb);
                steps.push(step);
                if (stepRow.length < 40) { stepRow.push(step.toFixed(0)); }
            }
        }
        var rs = stats(rgaps), ds = stats(steps);
        // the step's change from one refresh to the next: the car accelerates through the window, so the steps' own
        // rms is mostly that; a repeat or a skip is a change of a whole step
        var changes = [];
        for (i = 1; i < steps.length; i++) { changes.push(Math.abs(steps[i] - steps[i - 1])); }
        var cs2 = stats(changes);
        var seconds = (hi - lo) / 1000;
        log("DISPLAYED: " + n + " refreshes in the window, refresh gap mean " + rs.mean.toFixed(3) + " ms rms " + rs.rms.toFixed(3) +
            "; repeats " + repeats + " skips " + skips + " (" + (repeats / seconds).toFixed(1) + " and " + (skips / seconds).toFixed(1) +
            " a second); car step per refresh mean " + ds.mean.toFixed(1) + " mm rms " + ds.rms.toFixed(1) + " (" +
            (ds.mean > 0 ? (100 * ds.rms / ds.mean).toFixed(0) : "?") + "%) max " + ds.max.toFixed(1) +
            "; step change refresh to refresh mean " + cs2.mean.toFixed(2) + " mm rms " + cs2.rms.toFixed(2) + " max " + cs2.max.toFixed(1));
        log("DISPLAYED car steps per refresh in a row (mm): " + stepRow.join(" "));
        // the phase: where in its refresh interval each present started (its return less its duration, after the
        // last refresh before it), against the flush's wait and whether its frame was shown at the next refresh (the
        // map a sweep draws: TIVOLI_PRESENT_PHASE_SWEEP=1 walks the phase through the interval)
        var BUCKET = 0.5;
        var buckets = {};
        var k = 0;
        for (i = 0; i < presents.length; i++) {
            var p = presents[i];
            if (p.length < 6 || p[0] < lo || p[0] > hi) { continue; }
            var start = p[0] - p[3];
            while (k + 1 < refreshes.length && refreshes[k + 1] <= start) { k++; }
            if (refreshes[k] > start || k + 1 >= refreshes.length) { continue; }
            var phase = start - refreshes[k];
            var nextRefresh = refreshes[k + 1];
            // the frame shown at the next refresh: the newest present returned before it
            var m = i;
            while (m + 1 < presents.length && presents[m + 1][0] <= nextRefresh) { m++; }
            var shownNext = presents[m][0] <= nextRefresh ? presents[m][1] : -1;
            var b = Math.floor(phase / BUCKET) * BUCKET;
            var key = b.toFixed(1);
            if (!buckets[key]) { buckets[key] = { n: 0, swap: 0, swapMax: 0, shown: 0 }; }
            buckets[key].n++;
            buckets[key].swap += p[4];
            if (p[4] > buckets[key].swapMax) { buckets[key].swapMax = p[4]; }
            if (shownNext === p[1]) { buckets[key].shown++; }
        }
        var keys = Object.keys(buckets).sort(function (a, b) { return parseFloat(a) - parseFloat(b); });
        if (keys.length) {
            log("PHASE map (start after the refresh, ms: presents, flush wait mean/max ms, shown at the next refresh %): " +
                keys.map(function (key) {
                    var b = buckets[key];
                    return key + ": " + b.n + ", " + (b.swap / b.n).toFixed(2) + "/" + b.swapMax.toFixed(1) + ", " + (100 * b.shown / b.n).toFixed(0);
                }).join(" | "));
        }
        displayed = { refreshes: n, refreshGap: rs, repeats: repeats, skips: skips, step: ds, seconds: seconds, phase: buckets };
        return refreshes;
    }
    function drive() {
        if (Test.traceRenderedEntity) { log("render trace " + (Test.traceRenderedEntity(chassis) ? "on" : "unavailable")); }
        if (Test.traceRefreshes) { Test.traceRefreshes(true); }
        if (Test.tracePresents) {
            Test.tracePresents(true);
            log("present trace on; display " + (Test.getDisplayRefreshRate ? Test.getDisplayRefreshRate().toFixed(0) : "?") + " Hz, vsync " +
                (Test.isVsyncEnabled ? Test.isVsyncEnabled() : "?"));
        }
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
        // the car before any input, once a second until the drive: a car that moves here is moved by something
        // other than its driver (10-20: one run had it 3 m away at 3.5 m/s by the settle check, and in the hills)
        var preDrive = Script.setInterval(function () {
            var s = Entities.getEntityProperties(chassis, ["position", "velocity", "angularVelocity"]);
            if (!s || !s.position) { return; }
            var st = Entities.getVehicleStats ? Entities.getVehicleStats() : {};
            var live = st.live || {};
            log("pre-drive: chassis " + fmtV(s.position) + ", speed " + Vec3.length(s.velocity).toFixed(3) + ", spin " +
                Vec3.length(s.angularVelocity).toFixed(3) + ", throttle " + (live.forward !== undefined ? live.forward.toFixed(2) : "?") +
                " steering " + (live.right !== undefined ? live.right.toFixed(2) : "?") + " gear " + live.gear + " wheelRPM " +
                (live.wheelRPM ? live.wheelRPM.map(function (v) { return v.toFixed(0); }).join("/") : "?"));
        }, 1000);
        Script.setTimeout(function () {
            var s = Entities.getEntityProperties(chassis, ["position", "velocity"]);
            log("settled: chassis " + fmtV(s.position) + ", speed " + Vec3.length(s.velocity).toFixed(3));
            sit(LandRover.SEATS[0]);
            Script.setTimeout(function () { Script.clearInterval(preDrive); drive(); }, 3000);
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
