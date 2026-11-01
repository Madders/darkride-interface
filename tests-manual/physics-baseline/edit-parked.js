//
//  edit-parked.js
//  tests-manual/physics-baseline
//
//  The two vehicle bugs of 2026-10-19 (PHYSICS.md "The driver's stutter", found on the way): (1) an edit of a parked
//  car's pose while its driver sits in it (the editor righting a flipped car) moved it on the server but the driver's
//  predicted body stayed where it was, corrected every 4.5 s and never sticking; (2) sitting again after standing left
//  the driver's clock for the car behind the server's by the time stood, and the lead jumped to catch up. This script,
//  concatenated after landrover.js by edit-parked.sh: makes the Land Rover on the pad, sits as the driver, drives
//  3 s, stops, waits for the car to park (the stream ends), edits its pose by Entities.editEntity to 15 m aside and
//  upright, as the editor would, and watches for 20 s: the shown pose each second and the corrections in the log. Then
//  stands, waits 10 s, sits again and drives 3 s (the lead's jumps, if any, are in the log). EDIT-PARKED lines say what
//  happened; Test.quit at the end.
//
//  Distributed under the Apache License, Version 2.0.
//
(function () {
    var config = (typeof ROVER_CONFIG !== "undefined" && ROVER_CONFIG) ? ROVER_CONFIG : {};
    var TERRAIN_URL = config.terrainURL || "";
    var LIFETIME = config.lifetime || 600;
    var BOOT = (config.boot !== undefined) ? config.boot : 8;
    var SIT_ANIMATION_URL = "https://s3-us-west-1.amazonaws.com/hifi-content/clement/production/animations/sitting_idle.fbx";
    var DRIVE_KEYS = [DriveKeys.TRANSLATE_X, DriveKeys.TRANSLATE_Y, DriveKeys.TRANSLATE_Z, DriveKeys.YAW, DriveKeys.STEP_YAW,
                      DriveKeys.STEP_TRANSLATE_X, DriveKeys.STEP_TRANSLATE_Y, DriveKeys.STEP_TRANSLATE_Z];
    var terrain = null, chassis = null, seat = null, tries = 0, watcher = null;
    function log(s) { print("EDIT-PARKED " + s); }
    function vec3Of(v) { return (Array.isArray(v) && v.length >= 3) ? { x: v[0], y: v[1], z: v[2] } : v; }
    function fmtV(v) { return "(" + v.x.toFixed(2) + ", " + v.y.toFixed(2) + ", " + v.z.toFixed(2) + ")"; }
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
        log("seated as the driver");
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
        log("stood up");
    }
    function groundReady(x, z) {
        var expected = LandRover.groundHeight(x, z);
        var ray = { origin: { x: x, y: expected + 30, z: z }, direction: { x: 0, y: -1, z: 0 } };
        var hit = Entities.findRayIntersection(ray, true, [terrain], [], false, true);
        return hit && hit.intersects && hit.entityID == terrain && Math.abs(hit.intersection.y - expected) < 0.5;
    }
    function shown(label) {
        var p = Entities.getEntityProperties(chassis, ["position", "rotation", "velocity", "vehicle"]);
        var stats = Entities.getVehicleStats ? Entities.getVehicleStats() : {};
        log(label + ": shown " + fmtV(p.position) + " up " + fmtV(Quat.getUp(p.rotation)) + " speed " + Vec3.length(p.velocity).toFixed(2) +
            "; corrections " + stats.corrections + " maxError " + (stats.maxError || 0).toFixed(3) + " lastError " + (stats.lastError || 0).toFixed(3) +
            " leadJumps " + stats.leadJumps + " leadShifts " + stats.leadShifts + " margin " + stats.bufferMargin + " realigned " + stats.bufferRealigned);
    }
    function controls(c) { Entities.setVehicleControls(chassis, c); }
    function finish() {
        log("done");
        stand();
        Script.setTimeout(function () { Test.quit(); }, 1500);
    }
    // the phases, each a step of Caity's evening (10-19 20:18 to 20:28), with a look at the car each second
    var phases = [
        { name: "first drive", seconds: 3, enter: function () { controls({ throttle: 0.35, steering: 0, brake: 0, handBrake: 0 }); } },
        { name: "parked", seconds: 6, enter: function () { controls({ throttle: 0, steering: 0, brake: 1, handBrake: 1 }); } },
        { name: "standing", seconds: 10, enter: function () { stand(); } },
        { name: "second sit", seconds: 3, enter: function () { sit(LandRover.SEATS[0]); } },
        { name: "second drive", seconds: 5, enter: function () { controls({ throttle: 0.35, steering: 0.3, brake: 0, handBrake: 0 }); } },
        { name: "stopped", seconds: 4, enter: function () { controls({ throttle: 0, steering: 0, brake: 1, handBrake: 1 }); } },
        { name: "flipped by an edit", seconds: 8, enter: function () {
            var p = Entities.getEntityProperties(chassis, ["position", "rotation"]);
            Entities.editEntity(chassis, { position: { x: p.position.x, y: p.position.y + 1.0, z: p.position.z },
                                           rotation: Quat.multiply(Quat.fromPitchYawRollDegrees(0, 0, 180), p.rotation) });
        } },
        { name: "righted by the editor's rotate gizmo", seconds: 8, enter: function () {
            // as Caity did: build mode, the car selected, the rotate ring dragged a half turn about the car's own centre
            // over two seconds (the wheels end in the ground, as they did for her), the release at the end
            Test.selectInEditor([chassis]);
            var k = 0, steps = 120;
            var drag = Script.setInterval(function () {
                var roll = 180 * (k + 1) / steps;
                Test.editorDrag({ x: 0, y: 0, z: 0 }, 0, roll, k === 0, k === steps - 1);
                if (++k >= steps) { Script.clearInterval(drag); log("the drag released at roll " + roll); }
            }, 2000 / steps);
        } },
        { name: "drive away", seconds: 5, enter: function () { controls({ throttle: 0.4, steering: -0.3, brake: 0, handBrake: 0 }); } },
        { name: "stopped again, watching", seconds: 24, enter: function () { controls({ throttle: 0, steering: 0, brake: 1, handBrake: 1 }); } }
    ];
    if (config.quick) { phases = phases.slice(0, 6); }   // through the second drive: the sit bug alone
    var phaseIndex = -1;
    var ticker = null;
    function nextPhase() {
        if (ticker) { Script.clearInterval(ticker); ticker = null; }
        if (++phaseIndex >= phases.length) { finish(); return; }
        var phase = phases[phaseIndex];
        log("phase " + phase.name);
        phase.enter();
        var t = 0;
        ticker = Script.setInterval(function () { shown(phase.name + " " + (++t) + " s"); }, 1000);
        Script.setTimeout(nextPhase, phase.seconds * 1000);
    }
    function firstDrive() { nextPhase(); }
    function makeCar() {
        var pad = LandRover.TERRAIN_CENTER;
        if (!groundReady(pad.x, pad.z)) {
            if (++tries < 60) { Script.setTimeout(makeCar, 1000); } else { log("no ground"); Test.quit(); }
            return;
        }
        LandRover.clearVehicles();
        MyAvatar.position = { x: pad.x + 3, y: LandRover.groundHeight(pad.x + 3, pad.z) + 1.2, z: pad.z };
        chassis = LandRover.create(pad.x, pad.z, 0, LIFETIME).chassis;
        log("rover " + chassis);
        Script.setTimeout(function () {
            shown("settled");
            sit(LandRover.SEATS[0]);
            Script.setTimeout(firstDrive, 3000);
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
        Script.setTimeout(makeCar, 1000);
    }
    Script.setTimeout(setUp, BOOT * 1000);
    Script.setTimeout(function () { log("watchdog"); Test.quit(); }, 200000);
})();
