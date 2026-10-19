//
//  rover.js
//  tests-manual/physics-baseline
//
//  Round 6 of PHYSICS.md: the Land Rover on the terrain. One client is the driver: it makes the terrain (landrover.js,
//  the file make-terrain.py wrote, served over HTTP), waits until the ground is there, makes the Land Rover on the start
//  pad, sits in the driver's seat as vehicles.js would and drives a scripted course: idle, full throttle, brake, reverse,
//  hand brake, a loop to the right, a loop to the left, a slalom, a stop; then it stands up. A second client is the
//  passenger: it finds the newest Land Rover, sits in the passenger seat and rides the course. Every script frame both
//  record the chassis's pose and velocities, the speed along its forward, the authority's state as published on the
//  entity (the input sequence it reached, the engine, the gear), the vehicle statistics (corrections, replays, inputs),
//  their own stored local position against the seat, and now and then the wheels' local transforms. The summary (lines
//  starting with PHYSICS-ROVER) is per phase of the course; the samples land in physics-rover.json.
//
//  The role and the terrain's URL come from a global, ROVER_CONFIG; run-rover.sh writes one script per role into the
//  results dir: a line that sets it, then landrover.js, then this file (a local test script's Script.include of a file
//  beside it leaves no global behind, so the three are concatenated). Serverless (the fallback) or on a domain with the
//  physics server driving the vehicle.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var config = (typeof ROVER_CONFIG !== "undefined" && ROVER_CONFIG) ? ROVER_CONFIG : {};
    var ROLE = config.role || "driver";
    var TERRAIN_URL = config.terrainURL || "";
    var LIFETIME = config.lifetime || 600;   // s; on a domain the test's entities expire on their own
    var TIMEOUT = config.timeout || 160;     // s, the whole run
    var BOOT = (config.boot !== undefined) ? config.boot : 8;   // s before anything is made (the domain connection)
    var GUARD_RADIUS = 70;                   // m from the terrain's centre: beyond it the course is cut short and the car braked
    var SIT_ANIMATION_URL = "https://s3-us-west-1.amazonaws.com/hifi-content/clement/production/animations/sitting_idle.fbx";
    var DRIVE_KEYS = [DriveKeys.TRANSLATE_X, DriveKeys.TRANSLATE_Y, DriveKeys.TRANSLATE_Z, DriveKeys.YAW, DriveKeys.STEP_YAW,
                      DriveKeys.STEP_TRANSLATE_X, DriveKeys.STEP_TRANSLATE_Y, DriveKeys.STEP_TRANSLATE_Z];

    // the course: each phase holds its controls (or a function of the phase's time) for so many seconds
    var COURSE = [
        { name: "idle", seconds: 3, controls: { throttle: 0, steering: 0, brake: 0, handBrake: 0 } },
        { name: "throttle", seconds: 6, controls: { throttle: 1 } },
        { name: "brake", seconds: 4, controls: { brake: 1 } },
        { name: "reverse", seconds: 4, controls: { throttle: -1 } },
        { name: "handbrake", seconds: 3, controls: { handBrake: 1 } },
        { name: "right-loop", seconds: 10, controls: { throttle: 0.5, steering: 1 } },
        { name: "left-loop", seconds: 10, controls: { throttle: 0.5, steering: -1 } },
        { name: "slalom", seconds: 12, controls: function (t) { return { throttle: 0.4, steering: Math.sin(t * 2 * Math.PI / 3) }; } },
        { name: "stop", seconds: 4, controls: { brake: 1 } }
    ];

    var terrain = null;
    var chassis = null;
    var wheels = [];
    var seat = null;
    var seated = false;
    var mode = "boot";          // driver: boot, terrain, settle, seat, course, stand, end; passenger: boot, find, seat, ride, stand, end
    var modeT0 = 0;
    var phaseIndex = -1;
    var phase = "";             // the course phase (driver), or the driver's phase as read from the chassis (passenger)
    var published = "";
    var guardTripped = false;
    var samples = [];
    var events = [];
    var wheelSamples = [];
    var lastControls = { throttle: 0, steering: 0, brake: 0, handBrake: 0 };
    var t0 = Date.now() / 1000;
    var finished = false;
    var bootTried = -10;

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print("PHYSICS-ROVER " + s); }
    function setMode(m) {
        mode = m; modeT0 = now();
        events.push({ t: now(), mode: m });
        log("mode " + m + " at " + now().toFixed(2) + " s");
    }
    function setPhase(p) {
        if (p === phase) { return; }
        phase = p;
        events.push({ t: now(), phase: p });
        log("phase " + p + " at " + now().toFixed(2) + " s");
        publish();
    }
    function publish() {
        // the passenger reads the driver's phase off the chassis
        if (ROLE === "driver" && chassis && phase !== published) {
            published = phase;
            Entities.editEntity(chassis, { description: phase });
        }
    }
    function vec3Of(value) {
        if (Array.isArray(value) && value.length >= 3) { return { x: value[0], y: value[1], z: value[2] }; }
        if (value && typeof value.x === "number") { return value; }
        return { x: 0, y: 0, z: 0 };
    }
    function fmtV(v) { return v ? "(" + v.x.toFixed(2) + ", " + v.y.toFixed(2) + ", " + v.z.toFixed(2) + ")" : "none"; }

    // ---- sitting, as vehicles.js does it (that script answers clicks; a test script seats itself) ----
    function rolesToOverride() {
        return MyAvatar.getAnimationRoles().filter(function (role) {
            return !(role.indexOf("right") === 0 || role.indexOf("left") === 0);
        });
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
        if (seat.driver) {
            Entities.editEntity(chassis, { vehicle: { driver: MyAvatar.sessionUUID } });
        }
        seated = true;
        log("seated in " + seat.name + (seat.driver ? " as the driver" : "") + ": local " + fmtV(MyAvatar.localPosition) + ", parent " +
            (MyAvatar.parentID == chassis ? "the chassis" : MyAvatar.parentID) + ", world " + fmtV(MyAvatar.position));
    }
    function stand() {
        if (!seated) { return; }
        seated = false;
        Entities.clearVehicleControls(chassis);
        if (seat.driver) {
            var p = Entities.getEntityProperties(chassis, ["vehicle"]);
            if (p && p.vehicle && p.vehicle.driver === MyAvatar.sessionUUID) {
                Entities.editEntity(chassis, { vehicle: { driver: Uuid.NULL } });
            }
        }
        var position = MyAvatar.position;
        var orientation = MyAvatar.orientation;
        var right = Quat.getRight(orientation);
        MyAvatar.parentID = Uuid.NULL;
        MyAvatar.position = Vec3.sum(position, Vec3.sum(Vec3.multiply(1.5, right), { x: 0, y: 0.5, z: 0 }));
        MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, Quat.safeEulerAngles(orientation).y, 0);
        DRIVE_KEYS.forEach(function (key) { MyAvatar.enableDriveKey(key); });
        rolesToOverride().forEach(function (role) { MyAvatar.restoreRoleAnimation(role); });
        MyAvatar.collisionsEnabled = true;
        MyAvatar.hmdLeanRecenterEnabled = true;
        log("stood up at " + fmtV(MyAvatar.position) + " from " + fmtV(position));
    }

    // ---- the driver's world ----
    function deleteLeftovers() {
        var names = ["rover-chassis", "rover-wheel-FL", "rover-wheel-FR", "rover-wheel-RL", "rover-wheel-RR"];
        var removed = 0;
        names.forEach(function (name) {
            Entities.findEntitiesByName(name, LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false).forEach(function (id) {
                Entities.deleteEntity(id); removed++;
            });
        });
        if (removed > 0) { log("deleted " + removed + " leftover rover entities of an earlier run"); }
    }
    function startPad() {
        return { x: LandRover.TERRAIN_CENTER.x, z: LandRover.TERRAIN_CENTER.z };
    }
    function standBesidePad() {
        var pad = startPad();
        MyAvatar.position = { x: pad.x + 3, y: LandRover.groundHeight(pad.x + 3, pad.z) + 1.2, z: pad.z };
        MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, 90, 0);
        log("standing beside the pad at " + fmtV(MyAvatar.position));
    }
    function groundReady() {
        var pad = startPad();
        var expected = LandRover.groundHeight(pad.x, pad.z);
        var ray = { origin: { x: pad.x, y: expected + 30, z: pad.z }, direction: { x: 0, y: -1, z: 0 } };
        var hit = Entities.findRayIntersection(ray, true, [terrain], [], false, true);
        if (hit && hit.intersects && hit.entityID == terrain && Math.abs(hit.intersection.y - expected) < 0.5) {
            return hit.intersection.y;
        }
        return null;
    }
    function chassisState() {
        return Entities.getEntityProperties(chassis, ["position", "rotation", "velocity", "angularVelocity", "vehicle", "description", "simulationOwner"]);
    }
    function findRover() {
        var found = Entities.findEntitiesByName("rover-chassis", LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false);
        var best = null, bestAge = Infinity;
        found.forEach(function (id) {
            // the newest chassis, whenever it was made: with sign-in first (login-first) the passenger's script can start
            // after the driver's car exists, so its age is not measured against this script's start
            var age = Entities.getEntityProperties(id, ["age"]).age;
            if (age !== undefined && age < bestAge) { bestAge = age; best = id; }
        });
        return best;
    }
    function findWheels() {
        wheels = [];
        if (!chassis) { return; }
        Entities.getChildrenIDs(chassis).forEach(function (id) {
            var p = Entities.getEntityProperties(id, ["name", "vehicle"]);
            if (p && p.vehicle && p.vehicle.type === "wheel") { wheels.push({ id: id, name: p.vehicle.wheel }); }
        });
    }

    // ---- sampling ----
    var statsAtPhaseStart = null;
    var lastOwnerLine = "";
    var driverAvatarID = null;
    function sample(dt) {
        var c = chassisState();
        if (!c || !c.position) { return false; }
        var front = Quat.getFront(c.rotation);
        var up = Quat.getUp(c.rotation);
        var euler = Quat.safeEulerAngles(c.rotation);
        var stats = Entities.getVehicleStats();
        var v = c.vehicle || {};
        var row = {
            index: samples.length, t: now(), dt: dt, mode: mode, phase: ROLE === "driver" ? phase : (c.description || ""),
            pos: c.position, rot: c.rotation, vel: c.velocity, angVel: c.angularVelocity,
            speedAlong: Vec3.dot(c.velocity, front), speed: Vec3.length(c.velocity),
            yaw: euler.y, pitch: euler.x, roll: euler.z, upY: up.y,
            controls: lastControls,
            state: { sequence: v.inputSequence, throttle: v.throttle, steering: v.steering, brake: v.brake, handBrake: v.handBrake,
                     rpm: v.engineRPM, gear: v.gear, stateTime: v.stateTime, driver: v.driver },
            // kept small: the file holds some 4,000 rows
            stats: { corrections: stats.corrections, replaySteps: stats.replaySteps, maxError: stats.maxError, inputsSent: stats.inputsSent,
                     inputsReceived: stats.inputsReceived, inputsDropped: stats.inputsDropped, inputsStarved: stats.inputsStarved, vehicles: stats.vehicles,
                     streamStatesReceived: stats.streamStatesReceived, streamExtrapolations: stats.streamExtrapolations, rttMs: stats.rttMs,
                     delayMs: stats.delayMs, streamIntervalMs: stats.streamIntervalMs, streamJitterMs: stats.streamJitterMs,
                     inputsLate: stats.inputsLate, inputsEarly: stats.inputsEarly, bufferMargin: stats.bufferMargin, leadShifts: stats.leadShifts,
                     leadJumps: stats.leadJumps, timeShiftMs: stats.timeShiftMs },
            live: stats.live ? { engineRPM: stats.live.engineRPM, gear: stats.live.gear, clutch: stats.live.clutch, speed: stats.live.speed,
                                 wheelRPM: stats.live.wheelRPM, wheelSlip: stats.live.wheelSlip, contact: stats.live.contact } : null,
            seated: seated, parented: MyAvatar.parentID == chassis,
            myPos: MyAvatar.position,
            myLocal: (MyAvatar.parentID == chassis) ? MyAvatar.localPosition : null,
            seatError: (seated && MyAvatar.parentID == chassis) ? Vec3.distance(MyAvatar.localPosition, vec3Of(seat.position)) : null
        };
        if (ROLE === "passenger") {
            // the driver as the mixer delivers it: parented to the chassis, at the seat
            // the driver as the state names it (a property comes to a script as a UUID, not a string: Uuid.isNull, not
            // a comparison with Uuid.NULL; and AvatarList.getAvatar of the null UUID is this client's own avatar)
            if (v.driver && !Uuid.isNull(v.driver) && !Uuid.isEqual(v.driver, MyAvatar.sessionUUID)) { driverAvatarID = v.driver; }
            if (driverAvatarID) {
                var a = AvatarList.getAvatar(driverAvatarID);
                if (a && a.sessionUUID) {
                    row.driverParented = a.parentID == chassis;
                    row.driverLocal = a.parentID == chassis ? a.localPosition : null;
                    row.driverRelative = Vec3.multiplyQbyV(Quat.inverse(c.rotation), Vec3.subtract(a.position, c.position));
                }
            }
        }
        samples.push(row);
        if (wheels.length < 4 && samples.length % 60 === 0) { findWheels(); }   // a passenger's wheels arrive after the chassis
        if (samples.length % 10 === 0 && wheels.length > 0) {
            var ws = { t: now(), phase: row.phase, wheels: {} };
            wheels.forEach(function (w) {
                var p = Entities.getEntityProperties(w.id, ["localPosition", "localRotation"]);
                ws.wheels[w.name] = { local: p.localPosition, localRot: p.localRotation };
            });
            wheelSamples.push(ws);
        }
        if (samples.length % 60 === 0) {
            var owner = c.simulationOwner ? (c.simulationOwner.ownerID + " at priority " + c.simulationOwner.priority) : "none";
            var me = c.simulationOwner && c.simulationOwner.ownerID == MyAvatar.sessionUUID ? " (me)" : "";
            var line = "owner " + owner + me;
            if (line !== lastOwnerLine) {
                lastOwnerLine = line;
                events.push({ t: now(), owner: owner });
                log(line + " at " + now().toFixed(2) + " s; state sequence " + v.inputSequence + ", gear " + v.gear + ", rpm " +
                    (v.engineRPM || 0).toFixed(0) + ", driver " + v.driver);
            }
        }
        return true;
    }

    // ---- the driver's course ----
    function applyPhaseControls(tt) {
        var step = COURSE[phaseIndex];
        var controls = (typeof step.controls === "function") ? step.controls(tt) : step.controls;
        var c = { throttle: controls.throttle || 0, steering: controls.steering || 0, brake: controls.brake || 0, handBrake: controls.handBrake || 0 };
        var p = Entities.getEntityProperties(chassis, ["position"]);
        if (p && p.position) {
            var dx = p.position.x - LandRover.TERRAIN_CENTER.x, dz = p.position.z - LandRover.TERRAIN_CENTER.z;
            if (Math.sqrt(dx * dx + dz * dz) > GUARD_RADIUS) {
                if (!guardTripped) { guardTripped = true; log("guard: the car is " + Math.sqrt(dx * dx + dz * dz).toFixed(0) + " m from the centre; braking"); }
                c = { throttle: 0, steering: 0, brake: 1, handBrake: 0 };
            }
        }
        lastControls = c;
        var accepted = Entities.setVehicleControls(chassis, c);
        if (!controlsLogged) {
            controlsLogged = true;
            log("controls " + (accepted ? "accepted" : "REFUSED (not a vehicle here)") + "; parent " + (MyAvatar.parentID == chassis ? "the chassis" : MyAvatar.parentID) +
                ", local " + fmtV(MyAvatar.localPosition) + ", world " + fmtV(MyAvatar.position));
        }
    }
    var controlsLogged = false;
    function nextPhase() {
        phaseIndex++;
        if (phaseIndex >= COURSE.length) { return false; }
        setPhase(COURSE[phaseIndex].name);
        modeT0 = now();
        return true;
    }

    function driveDriver() {
        var tt = now() - modeT0;
        if (mode === "boot") {
            // the domain connection takes a while (longer under packet loss and delay): an entity made before the entity
            // server is there comes back null, so try once a second
            if (tt > BOOT && tt > bootTried + 1) {
                bootTried = tt;
                deleteLeftovers();
                terrain = LandRover.createTerrain(TERRAIN_URL, LIFETIME);
                if (!terrain || terrain === Uuid.NULL) {
                    terrain = null;
                    if (bootTried < BOOT + 2) { log("no entity server yet at " + tt.toFixed(1) + " s; trying again each second"); }
                    return;
                }
                log("terrain " + terrain + " at " + fmtV(LandRover.TERRAIN_CENTER) + ", " + LandRover.TERRAIN_SIZE + " m, file " + TERRAIN_URL +
                    " (at " + tt.toFixed(1) + " s)");
                setMode("terrain");
            }
        } else if (mode === "terrain") {
            var y = groundReady();
            if (y !== null) {
                log("ground ready after " + tt.toFixed(1) + " s: ray hit y " + y.toFixed(3) + ", expected " +
                    LandRover.groundHeight(startPad().x, startPad().z).toFixed(3));
                // the physics only simulates what is near the avatar (the workload's regions): stand beside the pad
                standBesidePad();
                setMode("ground-settle");
            } else if (tt > 60) {
                log("no ground after " + tt.toFixed(0) + " s"); finish("no ground"); return;
            }
        } else if (mode === "ground-settle") {
            if (tt > 2) {
                var pad = startPad();
                var made = LandRover.create(pad.x, pad.z, 0, LIFETIME);
                chassis = made.chassis;
                findWheels();
                var cp = Entities.getEntityProperties(chassis, ["position", "vehicle", "dynamic"]);
                log("rover " + chassis + " at " + fmtV(cp.position) + ", dynamic " + cp.dynamic + ", vehicle type " + (cp.vehicle && cp.vehicle.type) +
                    ", mass " + (cp.vehicle && cp.vehicle.mass) + ", " + wheels.length + " wheel entities; ground " +
                    LandRover.groundHeight(pad.x, pad.z).toFixed(3));
                setMode("settle");
            }
        } else if (mode === "settle") {
            var s = Entities.getEntityProperties(chassis, ["position", "velocity"]);
            if (tt > 3) {
                var ground = LandRover.groundHeight(s.position.x, s.position.z);
                log("settled: chassis y " + s.position.y.toFixed(3) + " = ground + " + (s.position.y - ground).toFixed(3) + " (expected about " +
                    LandRover.CHASSIS_ABOVE_GROUND.toFixed(2) + "), speed " + Vec3.length(s.velocity).toFixed(3));
                if (s.position.y < ground - 1) { log("the car fell through the ground"); finish("fell through"); return; }
                sit(LandRover.SEATS[0]);
                setMode("seat");
            }
        } else if (mode === "seat") {
            if (tt > 2) { setMode("course"); nextPhase(); }
        } else if (mode === "course") {
            if (tt >= COURSE[phaseIndex].seconds) {
                if (!nextPhase()) { setMode("stand"); stand(); return; }
                tt = 0;
            }
            applyPhaseControls(tt);
        } else if (mode === "stand") {
            if (tt > 3) { setPhase("done"); finish("done"); }
        }
    }

    // ---- the passenger ----
    function drivePassenger() {
        var tt = now() - modeT0;
        if (mode === "boot") {
            if (tt > 2) { standBesidePad(); setMode("find"); }
        } else if (mode === "find") {
            // the entity server sends what is near the avatar: stand by the pad (again every few seconds, the ground
            // is not there before the driver makes it)
            if (tt > 3) { standBesidePad(); modeT0 = now(); }
            var id = findRover();
            if (id) {
                chassis = id;
                findWheels();
                var cp = Entities.getEntityProperties(chassis, ["position", "vehicle"]);
                log("found rover " + chassis + " at " + fmtV(cp.position) + " after " + tt.toFixed(1) + " s, " + wheels.length + " wheel entities");
                standBesidePad();
                setMode("approach");
            } else if (tt > 90) { finish("no rover"); }
        } else if (mode === "approach") {
            if (tt > 2) {
                if (wheels.length < 4) { findWheels(); }
                sit(LandRover.SEATS[1]);
                setMode("ride");
            }
        } else if (mode === "ride") {
            var c = Entities.getEntityProperties(chassis, ["description"]);
            var p = c ? (c.description || "") : "";
            if (p !== phase) { setPhase(p); }
            if (p === "done" || tt > 130) { setMode("stand"); stand(); }
        } else if (mode === "stand") {
            if (tt > 2) { finish("done"); }
        }
    }

    // ---- analysis ----
    function steps(rows, key) {
        var lengths = [];
        for (var i = 1; i < rows.length; i++) {
            if (rows[i].index === rows[i - 1].index + 1 && rows[i][key] && rows[i - 1][key]) {
                lengths.push(Vec3.length(Vec3.subtract(rows[i][key], rows[i - 1][key])));
            }
        }
        if (lengths.length < 2) { return null; }
        var mean = lengths.reduce(function (a, b) { return a + b; }, 0) / lengths.length;
        var dev = 0, max = 0, over = 0;
        lengths.forEach(function (l) { dev += (l - mean) * (l - mean); if (l > max) { max = l; } if (l > 2 * mean && l > 0.02) { over++; } });
        return { n: lengths.length, meanMm: 1000 * mean, devMm: 1000 * Math.sqrt(dev / lengths.length), maxMm: 1000 * max, overTwice: over };
    }
    function jitter(rows, key) {
        // frame-to-frame change of a stored local position: should be zero while seated
        var out = { n: 0, rmsMm: 0, maxMm: 0, driftMm: 0 };
        var first = null, last = null, sum = 0;
        for (var i = 1; i < rows.length; i++) {
            if (rows[i].index === rows[i - 1].index + 1 && rows[i][key] && rows[i - 1][key]) {
                var d = Vec3.length(Vec3.subtract(rows[i][key], rows[i - 1][key]));
                sum += d * d; out.n++; if (d > out.maxMm) { out.maxMm = d; }
            }
            if (rows[i][key]) { if (!first) { first = rows[i][key]; } last = rows[i][key]; }
        }
        if (out.n === 0) { return null; }
        out.rmsMm = 1000 * Math.sqrt(sum / out.n); out.maxMm *= 1000;
        out.driftMm = first && last ? 1000 * Vec3.distance(first, last) : 0;
        return out;
    }
    function unwrapDelta(a, b) { var d = b - a; while (d > 180) { d -= 360; } while (d < -180) { d += 360; } return d; }
    function phaseStats(name, rows) {
        if (rows.length < 3) { return null; }
        var first = rows[0], last = rows[rows.length - 1];
        var maxAlong = -Infinity, minAlong = Infinity, maxRpm = 0, gears = {}, maxRoll = 0, maxPitch = 0, heading = 0, distance = 0;
        var seatErrMax = 0, seatErrSum = 0, seatErrN = 0, parentedFrames = 0, minUpY = 1;
        var liveN = 0, clutchSum = 0, wheelRpmMax = [], slipMax = [], airFrames = [];
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i];
            if (r.speedAlong > maxAlong) { maxAlong = r.speedAlong; }
            if (r.speedAlong < minAlong) { minAlong = r.speedAlong; }
            var rpm = r.live ? r.live.engineRPM : r.state.rpm;
            var gear = r.live ? r.live.gear : r.state.gear;
            if (rpm > maxRpm) { maxRpm = rpm; }
            gears[gear] = (gears[gear] || 0) + 1;
            if (r.live) {
                liveN++; clutchSum += r.live.clutch;
                for (var w = 0; w < r.live.wheelRPM.length; w++) {
                    wheelRpmMax[w] = Math.max(wheelRpmMax[w] || 0, Math.abs(r.live.wheelRPM[w]));
                    slipMax[w] = Math.max(slipMax[w] || 0, Math.abs(r.live.wheelSlip[w]));
                    if (!r.live.contact[w]) { airFrames[w] = (airFrames[w] || 0) + 1; }
                }
            }
            if (Math.abs(r.roll) > maxRoll) { maxRoll = Math.abs(r.roll); }
            if (Math.abs(r.pitch) > maxPitch) { maxPitch = Math.abs(r.pitch); }
            if (r.upY < minUpY) { minUpY = r.upY; }
            if (i > 0 && r.index === rows[i - 1].index + 1) { heading += unwrapDelta(rows[i - 1].yaw, r.yaw); distance += Vec3.distance(rows[i - 1].pos, r.pos); }
            if (r.seatError !== null && r.seatError !== undefined) { seatErrN++; seatErrSum += r.seatError; if (r.seatError > seatErrMax) { seatErrMax = r.seatError; } }
            if (r.parented) { parentedFrames++; }
        }
        var s0 = first.stats || {}, s1 = last.stats || {};
        return {
            phase: name, frames: rows.length, seconds: last.t - first.t, parentedFrames: parentedFrames,
            speedAlongMax: maxAlong, speedAlongMin: minAlong, speedAlongEnd: last.speedAlong, distanceM: distance, headingDeg: heading,
            rpmMax: maxRpm, gears: gears, rollMaxDeg: maxRoll, pitchMaxDeg: maxPitch, minUpY: minUpY,
            clutchMean: liveN ? clutchSum / liveN : null, wheelRpmMax: wheelRpmMax, slipMax: slipMax, airFrames: airFrames,
            seatErrorMeanMm: seatErrN ? 1000 * seatErrSum / seatErrN : null, seatErrorMaxMm: seatErrN ? 1000 * seatErrMax : null,
            myLocal: jitter(rows, "myLocal"), driverLocal: jitter(rows, "driverLocal"),
            chassisStep: steps(rows, "pos"),
            corrections: (s1.corrections || 0) - (s0.corrections || 0), replaySteps: (s1.replaySteps || 0) - (s0.replaySteps || 0),
            maxError: s1.maxError, inputsSent: (s1.inputsSent || 0) - (s0.inputsSent || 0),
            sequenceFrom: first.state.sequence, sequenceTo: last.state.sequence,
            stateUpdates: countStateUpdates(rows)
        };
    }
    function countStateUpdates(rows) {
        var n = 0;
        for (var i = 1; i < rows.length; i++) { if (rows[i].state.stateTime !== rows[i - 1].state.stateTime) { n++; } }
        return n;
    }
    function fmtSteps(j) {
        return j ? ("mean " + j.meanMm.toFixed(1) + " mm, rms dev " + j.devMm.toFixed(1) + " mm, max " + j.maxMm.toFixed(1) + " mm, " +
            j.overTwice + " over twice the mean of " + j.n) : "n/a";
    }
    function fmtJitter(j) {
        return j ? ("rms " + j.rmsMm.toFixed(2) + " mm, max " + j.maxMm.toFixed(2) + " mm, drift " + j.driftMm.toFixed(1) + " mm over " + j.n) : "n/a";
    }
    function wheelReport() {
        // the suspension's travel per wheel over the course, and the spin rate at speed (the visual placement)
        var names = {}, out = [];
        wheelSamples.forEach(function (ws) { Object.keys(ws.wheels).forEach(function (n) { names[n] = true; }); });
        Object.keys(names).forEach(function (n) {
            var ys = [], spin = [], prev = null;
            wheelSamples.forEach(function (ws) {
                var w = ws.wheels[n];
                if (!w || !w.local) { return; }
                ys.push(w.local.y);
                if (prev && ws.phase === "throttle") {
                    var dq = Quat.multiply(w.localRot, Quat.inverse(prev.localRot));
                    spin.push(Quat.safeEulerAngles(dq));
                }
                prev = { localRot: w.localRot, t: ws.t };
            });
            if (ys.length === 0) { return; }
            var min = Math.min.apply(null, ys), max = Math.max.apply(null, ys);
            var x = wheelSamples[0].wheels[n] && wheelSamples[0].wheels[n].local ? wheelSamples[0].wheels[n].local.x : 0;
            out.push(n + ": x " + x.toFixed(2) + ", y " + min.toFixed(3) + " to " + max.toFixed(3) + " (travel " + (1000 * (max - min)).toFixed(0) +
                " mm), " + spin.length + " spin samples");
        });
        return out.join("; ");
    }

    function finish(reason) {
        if (finished) { return; }
        finished = true;
        Script.update.disconnect(onUpdate);
        if (seated) { stand(); }
        var summary = [];
        var seen = [];
        samples.forEach(function (r) { if (r.phase && seen.indexOf(r.phase) < 0) { seen.push(r.phase); } });
        seen.forEach(function (name) {
            var s = phaseStats(name, samples.filter(function (r) { return r.phase === name; }));
            if (s) { summary.push(s); }
        });
        summary.forEach(function (s) {
            log(s.phase + ": " + s.frames + " frames in " + s.seconds.toFixed(1) + " s; seated " + s.parentedFrames + "/" + s.frames +
                "; speed along forward " + s.speedAlongMin.toFixed(2) + " to " + s.speedAlongMax.toFixed(2) + " m/s (end " + s.speedAlongEnd.toFixed(2) + ")" +
                "; " + s.distanceM.toFixed(1) + " m; heading " + s.headingDeg.toFixed(0) + " deg" +
                "; rpm up to " + s.rpmMax.toFixed(0) + ", gears " + JSON.stringify(s.gears) +
                (s.clutchMean !== null ? ", clutch mean " + s.clutchMean.toFixed(2) + ", wheel rpm up to " + s.wheelRpmMax.map(function (v) { return v.toFixed(0); }).join("/") +
                    ", slip up to " + s.slipMax.map(function (v) { return v.toFixed(2); }).join("/") + ", frames in the air " + JSON.stringify(s.airFrames) : "") +
                "; tilt up to " + (180 / Math.PI * Math.acos(Math.max(-1, Math.min(1, s.minUpY)))).toFixed(1) + " deg" +
                "; seat error mean " + (s.seatErrorMeanMm === null ? "n/a" : s.seatErrorMeanMm.toFixed(1) + " mm, max " + s.seatErrorMaxMm.toFixed(1) + " mm") +
                "; my stored local " + fmtJitter(s.myLocal) +
                (ROLE === "passenger" ? "; the driver's stored local " + fmtJitter(s.driverLocal) : "") +
                "; chassis world step " + fmtSteps(s.chassisStep) +
                "; corrections " + s.corrections + " (" + s.replaySteps + " substeps replayed), inputs sent " + s.inputsSent +
                ", state sequence " + s.sequenceFrom + " to " + s.sequenceTo + ", " + s.stateUpdates + " state updates");
        });
        var last = samples.length ? samples[samples.length - 1].stats : {};
        log("totals: " + samples.length + " samples; corrections " + (last.corrections || 0) + ", replay substeps " + (last.replaySteps || 0) +
            ", max error " + (1000 * (last.maxError || 0)).toFixed(0) + " mm, inputs sent " + (last.inputsSent || 0) + ", received " + (last.inputsReceived || 0) +
            ", dropped " + (last.inputsDropped || 0) + ", starved " + (last.inputsStarved || 0) + ", vehicles " + (last.vehicles || 0) +
            "; stream: states received " + (last.streamStatesReceived || 0) + ", extrapolated substeps " + (last.streamExtrapolations || 0) +
            ", rtt " + (last.rttMs || 0).toFixed(1) + " ms, delay " + (last.delayMs || 0).toFixed(1) + " ms, interval " + (last.streamIntervalMs || 0).toFixed(1) +
            " ms, jitter " + (last.streamJitterMs || 0).toFixed(1) + " ms" +
            "; input buffer: margin " + (last.bufferMargin || 0) + ", lead shifts " + (last.leadShifts || 0) + " (jumps " + (last.leadJumps || 0) +
            "), late " + (last.inputsLate || 0) + ", early " + (last.inputsEarly || 0));
        log("wheels: " + wheelReport());
        Test.saveObject({ role: ROLE, summary: summary, events: events, wheelSamples: wheelSamples, samples: samples }, "physics-rover.json");
        log("done (" + reason + "), " + samples.length + " samples");
        if (ROLE === "driver") {
            wheels.forEach(function (w) { Entities.deleteEntity(w.id); });
            if (chassis) { Entities.deleteEntity(chassis); }
        }
        Script.setTimeout(function () { Test.quit(); }, 1500);
    }

    function onUpdate(dt) {
        if (now() > TIMEOUT) { finish("timeout"); return; }
        if (chassis && !sample(dt)) { finish("rover gone"); return; }
        if (ROLE === "driver") { driveDriver(); } else { drivePassenger(); }
    }

    Script.update.connect(onUpdate);
    Script.scriptEnding.connect(function () { if (seated) { stand(); } });
    log("started as " + ROLE + (TERRAIN_URL ? ", terrain " + TERRAIN_URL : ""));
}());
