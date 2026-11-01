//
//  assist-pilot.js
//  tests-manual/physics-baseline
//
//  A scripted pilot for the flight assistant (scripts/system/libraries/flightAssist.js): it builds the tilt-rotor on
//  the pad, takes its stick and flies flight-assist-replica.py's course through the assistant, pressing the keys the
//  course names (the assistant's intent; the keyboard itself is vehicles.js's business), every frame reading the craft
//  as vehicles.js does. Prints ASSIST lines, a summary per phase in the replica's columns, and saves the samples with
//  Test.saveObject. run-assist.sh puts landrover.js, aircraft-rig.js, flightAssist.js and this together.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var config = typeof ASSIST_CONFIG !== "undefined" ? ASSIST_CONFIG : {};
    var LIFETIME = 900;
    var COURSE = [
        ["on the ground", 3, {}],
        ["lift off (Up)", 6, { up: 1 }],
        ["hover, hands off", 4, {}],
        ["turn in hover (A)", 4, { left: 1 }],
        ["creep forward (W briefly)", 0.6, { faster: 1 }],
        ["creeping, hands off", 6, {}],
        ["stop in hover (X)", 0.1, { stop: 1 }],
        ["stopping, hands off", 6, {}],
        ["speed up (W)", 5, { faster: 1 }],
        ["transition, hands off", 20, {}],
        ["cruise, hands off", 10, {}],
        ["turn left (A)", 12, { left: 1 }],
        ["level, hands off", 8, {}],
        ["climb (Up)", 6, { up: 1 }],
        ["hold height, hands off", 8, {}],
        ["turn right (D)", 12, { right: 1 }],
        ["descend (Down)", 5, { down: 1 }],
        ["level, hands off 2", 8, {}],
        ["stop (X)", 1, { stop: 1 }],
        ["slowing to hover, hands off", 40, {}],
        ["hover, hands off 2", 6, {}],
        ["descend to land (Down)", 12, { down: 1 }],
        ["landed", 4, {}]
    ];
    var craft = null, assist = null, previous = null, aboveGround = 1000, frame = 0;
    var phase = -1, phaseEnd = 0, t0 = 0, lastT = 0, flying = false;
    var samples = [];

    function log(s) { print("ASSIST " + s); }
    function now() { return Date.now() / 1000; }

    function groundBelow(p) {
        var ignore = [craft].concat(Entities.getChildrenIDs(craft));
        var hit = Entities.findRayIntersection({ origin: p.position, direction: { x: 0, y: -1, z: 0 } }, true, [], ignore);
        return hit && hit.intersects ? Math.max(0, hit.distance - 0.5 * p.dimensions.y) : 1000;
    }

    function update(dt) {
        if (!flying) {
            return;
        }
        var t = now();
        if (t >= phaseEnd) {
            phase++;
            if (phase >= COURSE.length) {
                finish();
                return;
            }
            phaseEnd = t + COURSE[phase][1];
            log("phase " + COURSE[phase][0] + " at " + (t - t0).toFixed(1) + " s");
        }
        var frameDt = lastT ? Math.min(0.1, t - lastT) : dt;
        lastT = t;
        var p = Entities.getEntityProperties(craft, ["position", "rotation", "velocity", "dimensions"]);
        if (!p || !p.position) {
            return;
        }
        if (frame++ % 3 === 0) {
            aboveGround = groundBelow(p);
        }
        var live = (Entities.getVehicleStats() || {}).live;
        if (live && !live.aircraft) {
            live = null;
        }
        var s = FlightAssist.state(p, live, aboveGround, previous, frameDt);
        previous = s;
        var c = assist.controls(COURSE[phase][2], s, frameDt);
        Entities.setVehicleControls(craft, c);
        samples.push({ phase: COURSE[phase][0], t: t - t0, s: s, c: c, speedTarget: assist.speedTarget, hold: assist.holdHeight,
                       live: !!live, thrust: live ? live.thrust : null, liveTilt: live ? live.tilt : null,
                       pilot: Vec3.distance(MyAvatar.position, p.position) });
    }

    function finish() {
        flying = false;
        Entities.clearVehicleControls(craft);
        log("course done; samples " + samples.length);
        var by = [];
        samples.forEach(function (r) {
            if (!by.length || by[by.length - 1].name !== r.phase) { by.push({ name: r.phase, rows: [] }); }
            by[by.length - 1].rows.push(r);
        });
        log("SUMMARY phase | end s | above ground | airspeed | tilt | pitch | bank | alpha | maxAoA | hold err");
        by.forEach(function (b) {
            var last = b.rows[b.rows.length - 1].s;
            var fast = b.rows.filter(function (r) { return r.s.airspeed > 5; });
            var maxAoA = fast.length ? Math.max.apply(null, fast.map(function (r) { return Math.abs(r.s.alpha); })) : 0;
            var held = b.rows.filter(function (r) { return r.hold !== null; }).map(function (r) { return Math.abs(r.s.height - r.hold); });
            var hold = held.length && (b.name.indexOf("hands off") >= 0 || b.name.indexOf("level") >= 0) ? Math.max.apply(null, held) : NaN;
            log("SUMMARY " + b.name + " | " + b.rows[b.rows.length - 1].t.toFixed(1) + " | " + last.aboveGround.toFixed(1) + " | " +
                last.airspeed.toFixed(1) + " | " + last.tilt.toFixed(2) + " | " + last.pitchDeg.toFixed(1) + " | " + last.bank.toFixed(1) +
                " | " + last.alpha.toFixed(1) + " | " + maxAoA.toFixed(1) + " | " + hold.toFixed(2));
        });
        var stats = Entities.getVehicleStats();
        log("vehicle stats " + JSON.stringify({ corrections: stats.corrections, maxError: stats.maxError, inputsSent: stats.inputsSent }));
        Test.saveObject({ samples: samples }, "assist-pilot.json");
        Entities.deleteEntity(craft);
        Script.setTimeout(function () { Test.quit(); }, 2000);
    }

    function start() {
        var c = LandRover.TERRAIN_CENTER;
        LandRover.clearVehicles();
        craft = Aircraft.create(c.x, c.z, 0, LIFETIME).hull;
        log("aircraft " + craft + " on the pad");
        Script.setTimeout(function () {
            Entities.editEntity(craft, { vehicle: { driver: MyAvatar.sessionUUID } });
            var hp = Entities.getEntityProperties(craft, ["position", "rotation", "vehicle"]);
            assist = FlightAssist.create(hp.vehicle.mass);
            var helm = Vec3.sum(Vec3.sum(hp.position, Vec3.multiplyQbyV(hp.rotation, { x: 0, y: 0.2, z: -1.5 })), { x: 0, y: 1.0, z: 0 });
            var tries = 0;
            function board() {
                if (Vec3.distance(MyAvatar.position, helm) < 1.5) {
                    log("aboard at the stick after " + tries + " tries");
                    t0 = now();
                    flying = true;
                    log("flying the course");
                    return;
                }
                if (++tries > 10) { log("could not board; giving up"); Test.quit(); return; }
                MyAvatar.position = helm;
                Script.setTimeout(board, 1000);
            }
            board();
        }, 4000);
    }

    function setUp(tries) {
        var terrain = LandRover.createTerrain(config.terrainURL || "", LIFETIME);
        if (!terrain || terrain === Uuid.NULL) {
            if (tries < 60) { Script.setTimeout(function () { setUp(tries + 1); }, 1000); } else { log("no terrain; giving up"); Test.quit(); }
            return;
        }
        var c = LandRover.TERRAIN_CENTER;
        MyAvatar.position = { x: c.x + 4, y: LandRover.groundHeight(c.x + 4, c.z) + 1.2, z: c.z + 6 };
        Script.setTimeout(start, 4000);
    }

    Script.update.connect(update);
    Script.scriptEnding.connect(function () { Script.update.disconnect(update); });
    Script.setTimeout(function () { setUp(0); }, 3000);
}());
