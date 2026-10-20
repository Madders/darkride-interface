//
//  airship.js
//  tests-manual/physics-baseline
//
//  Round 7 of PHYSICS.md: the airship. One client is the pilot: it makes the terrain (landrover.js, the file make-terrain.py
//  wrote, served over HTTP), waits for the ground, makes the ship on the pad (airship-rig.js), takes the helm as vehicles.js
//  would (it stands at the helm on the deck, a member of the ship's frame by where it stands; no parenting by hand) and
//  flies a scripted course through Entities.setVehicleControls: moored, ballast dropped, lift-off on the vertical thruster,
//  a climb to cruising height, a cruise, a turn each way under full rudder, the nose up and down with the elevator, the
//  way back to the pad, a descent with gas vented, a landing; then it leaves the helm. A second client is the crew: it
//  walks aboard from the pad, drops a ball on the deck, and through the flight stands and walks fore and aft on the deck
//  by turns; after the landing it moves the ship with one explicit edit of its position (the editor's teleport, which the
//  physics server adopts) and watches everyone come along. Every script frame both record the hull's pose and velocities,
//  the live telemetry (altitude, air density, buoyancy, ballast, fill, thrust, airspeed), the authority's state as
//  published (the input sequence, the applied controls, ballast, fill), the vehicle statistics, their own membership of
//  the frame and stored local position, and the crew the ball's local position and the pilot's stored local position as
//  the mixer delivers it. The summary (lines starting with PHYSICS-AIRSHIP) is per phase; the samples land in
//  physics-airship.json.
//
//  The role and the terrain's URL come from a global, AIRSHIP_CONFIG; run-airship.sh writes one script per role: a line
//  that sets it, then landrover.js, airship-rig.js and this file. Serverless (the pilot alone, the fallback) or on a domain
//  with the physics server flying the ship.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var config = (typeof AIRSHIP_CONFIG !== "undefined" && AIRSHIP_CONFIG) ? AIRSHIP_CONFIG : {};
    var ROLE = config.role || "pilot";
    var KIND = config.vehicle || "airship";                         // "airship" (round 7) or "aircraft" (round 8)
    var Rig = KIND === "aircraft" ? Aircraft : Airship;             // the rig that makes it (airship-rig.js, aircraft-rig.js)
    var HULL_NAME = Rig.HULL_NAME || "airship-hull";
    var TAG = KIND === "aircraft" ? "PHYSICS-AIRCRAFT" : "PHYSICS-AIRSHIP";
    var TERRAIN_URL = config.terrainURL || "";
    var LIFETIME = config.lifetime || 600;
    var TIMEOUT = config.timeout || 260;    // s of course; the wait for the world before it (a cold cache took 90 s) is not counted
    var courseT0 = -1;
    var BOOT = (config.boot !== undefined) ? config.boot : 8;
    var DECK_CAR = !!config.deckCar;         // round 8: the trolley on the deck; the crew rides it instead of walking the deck
    var CRUISE_HEIGHT = 35;                  // m above the pad's ground
    var RETURN_RADIUS = 60;                  // m from the pad: close enough to brake and come down (the braking takes about 50 m)
    var GUARD_RADIUS = 80;                   // m from the pad: beyond it the autopilot heads home whatever the phase (the terrain ends at 96 m)
    var WALK_SPEED = 1.0;                    // m/s, the crew's scripted motor
    var DRIVE_KEYS = [DriveKeys.TRANSLATE_X, DriveKeys.TRANSLATE_Y, DriveKeys.TRANSLATE_Z, DriveKeys.YAW, DriveKeys.STEP_YAW,
                      DriveKeys.STEP_TRANSLATE_X, DriveKeys.STEP_TRANSLATE_Y, DriveKeys.STEP_TRANSLATE_Z];

    // the pilot's course: each phase holds its controls, or an autopilot setting, until its time is up or its `until` says so
    var COURSE = [
        { name: "moored", seconds: 3, controls: {} },
        { name: "ballast", seconds: 4, controls: { trim: 1 } },
        { name: "lift-off", seconds: 4, controls: { lift: 0.5 } },
        { name: "climb", seconds: 25, auto: { throttle: 0.2, height: CRUISE_HEIGHT, heading: "hold" }, until: function (s) { return s.height > CRUISE_HEIGHT - 5; } },
        { name: "cruise", seconds: 6, auto: { throttle: 0.4, height: CRUISE_HEIGHT, heading: "hold" } },
        { name: "turn-right", seconds: 12, auto: { throttle: 0.3, height: CRUISE_HEIGHT, steering: 1 } },
        { name: "turn-left", seconds: 24, auto: { throttle: 0.3, height: CRUISE_HEIGHT, steering: -1 } },
        { name: "nose-up", seconds: 4, auto: { throttle: 0.3, pitch: 1 } },
        { name: "nose-down", seconds: 4, auto: { throttle: 0.3, pitch: -1 } },
        { name: "return", seconds: 90, auto: { throttle: "home", height: CRUISE_HEIGHT, heading: "pad" }, until: function (s) { return s.padDistance < RETURN_RADIUS; } },
        { name: "brake", seconds: 25, auto: { throttle: -0.6, height: CRUISE_HEIGHT, heading: "hold" }, until: function (s) { return s.speedAlong < 0.5; } },
        { name: "vent", seconds: 6, auto: { throttle: 0, trim: -1, height: CRUISE_HEIGHT } },
        { name: "descend", seconds: 90, auto: { throttle: 0, descend: true }, until: function (s) { return s.aboveGround < 2.0 && Math.abs(s.climbRate) < 0.05; } },
        { name: "landed", seconds: 5, controls: { lift: -0.3 } },
        { name: "teleport", seconds: 8, auto: { throttle: 0, hover: true } },
        { name: "done", seconds: 2, controls: {} }
    ];

    // the aircraft's course (round 8): a hover lift-off on vertical thrust, a climb, the nacelles tilted level into
    // forward flight, a cruise, a turn each way on the ailerons, the way back, the nacelles tilted up again to hover
    // and brake, a descent and a landing. The autopilot holds height with the throttle in hover and with the elevator
    // in flight, heads by yaw in hover and by bank in flight, and in hover pitches against the forward speed to stop.
    var AIRCRAFT_COURSE = [
        { name: "moored", seconds: 3, controls: {} },
        { name: "lift-off", seconds: 6, auto: { hover: true, climb: 1.5 } },
        { name: "climb", seconds: 30, auto: { hover: true, height: CRUISE_HEIGHT, heading: "hold" }, until: function (s) { return s.height > CRUISE_HEIGHT - 3; } },
        { name: "transition", seconds: 15, auto: { hover: true, tiltTo: 0.5, height: CRUISE_HEIGHT, heading: "hold" }, until: function (s) { return s.airspeed > 25; } },
        { name: "level", seconds: 8, auto: { flight: true, tiltTo: 0, height: CRUISE_HEIGHT, heading: "hold" } },
        { name: "cruise", seconds: 8, auto: { flight: true, height: CRUISE_HEIGHT, heading: "hold" } },
        { name: "turn-right", seconds: 10, auto: { flight: true, height: CRUISE_HEIGHT, bank: 30 } },
        { name: "turn-left", seconds: 12, auto: { flight: true, height: CRUISE_HEIGHT, bank: -30 } },
        { name: "return", seconds: 60, auto: { flight: true, height: CRUISE_HEIGHT, heading: "pad" }, until: function (s) { return s.padDistance < 450; } },
        { name: "brake", seconds: 30, auto: { hover: true, tiltTo: 1, height: CRUISE_HEIGHT, stop: true }, until: function (s) { return s.horizontalSpeed < 3.0; } },
        { name: "home", seconds: 60, auto: { hover: true, tiltTo: 1, height: CRUISE_HEIGHT, toPad: true }, until: function (s) { return s.padDistance < 8; } },
        { name: "descend", seconds: 60, auto: { hover: true, tiltTo: 1, descend: true, toPad: true }, until: function (s) { return s.aboveGround < 0.4 && Math.abs(s.climbRate) < 0.5; } },
        { name: "landed", seconds: 5, controls: { throttle: 0 } },
        { name: "done", seconds: 2, controls: {} }
    ];
    if (KIND === "aircraft") { COURSE = AIRCRAFT_COURSE; }

    var terrain = null;
    var hull = null;
    var ball = null;
    var atHelm = false;
    var mode = "boot";
    var modeT0 = 0;
    var phaseIndex = -1;
    var phase = "";
    var published = "";
    var holdHeading = null;
    var helmChecked = false;
    var samples = [];
    var events = [];
    var crossings = [];
    var lastParent = null;
    var lastControls = {};
    var t0 = Date.now() / 1000;
    var finished = false;
    var bootTried = -10;
    var teleportDone = false;
    var padGround = 0;
    var crewLeg = { t: 0, walking: false, direction: -1 };
    var track = null;
    var car = null;
    // the crew's ride on the trolley: on at a station, off at every second stop, on again when it is back
    var carRide = { state: "none", t: 0, stops: 0, lastStation: -1, seenMoving: false, offAtStation: -1, boardings: 0, failures: 0,
                    probed: false, probeHold: false, probe: null };
    var probes = [];   // the rail pushes (round 8): walking from the deck into the car's end rail, which is a copy in the ship's world

    function now() { return Date.now() / 1000 - t0; }
    function log(s) { print(TAG + " " + s); }
    function setMode(m) { mode = m; modeT0 = now(); events.push({ t: now(), mode: m }); log("mode " + m + " at " + now().toFixed(2) + " s"); if (m === "course" && courseT0 < 0) { courseT0 = now(); } }
    function setPhase(p) {
        if (p === phase) { return; }
        phase = p;
        events.push({ t: now(), phase: p });
        log("phase " + p + " at " + now().toFixed(2) + " s");
        if (ROLE === "pilot" && hull && phase !== published) { published = phase; Entities.editEntity(hull, { description: phase }); }
    }
    function vec3Of(value) {
        if (Array.isArray(value) && value.length >= 3) { return { x: value[0], y: value[1], z: value[2] }; }
        if (value && typeof value.x === "number") { return value; }
        return { x: 0, y: 0, z: 0 };
    }
    function facingRange(rows) {
        var min = null, max = null;
        rows.forEach(function (r) {
            if (typeof r.myLocalYaw !== "number") { return; }
            var yaw = r.myLocalYaw <= -179.5 ? r.myLocalYaw + 360 : r.myLocalYaw;   // -180 and 180 are one facing
            if (min === null || yaw < min) { min = yaw; }
            if (max === null || yaw > max) { max = yaw; }
        });
        return min === null ? null : { min: min, max: max };
    }
    function fmtV(v) { return v ? "(" + v.x.toFixed(2) + ", " + v.y.toFixed(2) + ", " + v.z.toFixed(2) + ")" : "none"; }
    function parentNameOf(id) {
        if (Uuid.isNull(id)) { return "none"; }
        if (hull && Uuid.isEqual(id, hull)) { return "ship"; }
        if (car && Uuid.isEqual(id, car)) { return "car"; }
        return "other";
    }
    // the trolley's pose relative to its track (on the line: x and y near 0, z between the stations) and the train
    function carPose() {
        if (!car || !track) { return null; }
        var cp = Entities.getEntityProperties(car, ["localPosition", "parentID"]);
        var tp = Entities.getEntityProperties(track, ["trainSpeed", "trainStation", "trainDistance"]);
        if (!cp || !cp.localPosition) { return null; }
        return { x: cp.localPosition.x, y: cp.localPosition.y, z: cp.localPosition.z, parentIsTrack: cp.parentID == track,
                 speed: tp ? tp.trainSpeed : null, station: tp ? tp.trainStation : null, distance: tp ? tp.trainDistance : null };
    }
    function pad() { return { x: LandRover.TERRAIN_CENTER.x, z: LandRover.TERRAIN_CENTER.z }; }
    function wrap(d) { while (d > 180) { d -= 360; } while (d < -180) { d += 360; } return d; }
    function headingOf(rotation) { var f = Quat.getFront(rotation); return 180 / Math.PI * Math.atan2(f.x, -f.z); }
    function clamp1(v) { return Math.max(-1, Math.min(1, v)); }

    // ---- the pilot at the helm, as vehicles.js puts them there ----
    function takeHelm() {
        var p = Entities.getEntityProperties(hull, ["position", "rotation"]);
        var seat = Rig.HELM;
        var helm = Vec3.sum(p.position, Vec3.multiplyQbyV(p.rotation, vec3Of(seat.position)));
        DRIVE_KEYS.forEach(function (key) { MyAvatar.disableDriveKey(key); });
        MyAvatar.position = Vec3.sum(helm, { x: 0, y: 1.0, z: 0 });
        MyAvatar.orientation = p.rotation;
        Entities.editEntity(hull, { vehicle: { driver: MyAvatar.sessionUUID } });
        atHelm = true;
        log("at the helm: world " + fmtV(MyAvatar.position) + ", parent " + (MyAvatar.parentID == hull ? "the ship" : MyAvatar.parentID));
    }
    function leaveHelm() {
        if (!atHelm) { return; }
        atHelm = false;
        Entities.clearVehicleControls(hull);
        var p = Entities.getEntityProperties(hull, ["vehicle"]);
        if (p && p.vehicle && p.vehicle.driver === MyAvatar.sessionUUID) { Entities.editEntity(hull, { vehicle: { driver: Uuid.NULL } }); }
        DRIVE_KEYS.forEach(function (key) { MyAvatar.enableDriveKey(key); });
        log("left the helm at " + fmtV(MyAvatar.position) + ", parent " + (MyAvatar.parentID == hull ? "the ship" : MyAvatar.parentID));
    }

    // ---- the world ----
    function standBesidePad(side) {
        var c = pad();
        MyAvatar.position = { x: c.x + (side || 6), y: LandRover.groundHeight(c.x + (side || 6), c.z) + 1.2, z: c.z };
        MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, 90, 0);
    }
    function groundReady() {
        var c = pad();
        var expected = LandRover.groundHeight(c.x, c.z);
        var ray = { origin: { x: c.x, y: expected + 30, z: c.z }, direction: { x: 0, y: -1, z: 0 } };
        var hit = Entities.findRayIntersection(ray, true, [terrain], [], false, true);
        if (hit && hit.intersects && hit.entityID == terrain && Math.abs(hit.intersection.y - expected) < 0.5) { return hit.intersection.y; }
        return null;
    }
    function hullState() {
        return Entities.getEntityProperties(hull, ["position", "rotation", "velocity", "angularVelocity", "vehicle", "description", "simulationOwner"]);
    }
    function findCar() {
        var found = Entities.findEntitiesByName("airship-car", LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false);
        if (!found.length) { return false; }
        car = found[0];
        track = Entities.getEntityProperties(car, ["parentID"]).parentID;
        return true;
    }
    function findShip() {
        var found = Entities.findEntitiesByName(HULL_NAME, LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false);
        var best = null, bestAge = Infinity;
        found.forEach(function (id) {
            var age = Entities.getEntityProperties(id, ["age"]).age;
            if (age !== undefined && age < bestAge && age < now() + 1) { bestAge = age; best = id; }
        });
        return best;
    }

    // ---- sampling ----
    var lastOwnerLine = "";
    var pilotAvatarID = null;
    var pilotLookupLogged = false;
    function flight(c) {
        // what the autopilot and the summary read off the hull
        var ground = LandRover.groundHeight(c.position.x, c.position.z);
        var dx = c.position.x - pad().x, dz = c.position.z - pad().z;
        return {
            height: c.position.y - padGround, aboveGround: c.position.y - ground, climbRate: c.velocity.y,
            heading: headingOf(c.rotation), padDistance: Math.sqrt(dx * dx + dz * dz),
            padBearing: 180 / Math.PI * Math.atan2(-dx, dz),   // the compass direction from the ship to the pad
            speed: Vec3.length(c.velocity), speedAlong: Vec3.dot(c.velocity, Quat.getFront(c.rotation)),
            // the bank, positive with the right wing down, the pitch, positive nose up, and their rates; the live tilt and
            // airspeed from the model
            bank: -180 / Math.PI * Math.asin(Math.max(-1, Math.min(1, Quat.getRight(c.rotation).y))),
            pitchDeg: 180 / Math.PI * Math.asin(Math.max(-1, Math.min(1, Quat.getFront(c.rotation).y))),
            pitchRate: 180 / Math.PI * Vec3.dot(c.angularVelocity, Quat.getRight(c.rotation)),
            rollRate: -180 / Math.PI * Vec3.dot(c.angularVelocity, Quat.getFront(c.rotation)),
            tilt: liveTilt(), airspeed: liveAirspeed(), wingLift: liveLift(),
            // the horizontal velocity: its speed and its compass heading, as the hull's heading is measured
            horizontalSpeed: Math.sqrt(c.velocity.x * c.velocity.x + c.velocity.z * c.velocity.z),
            velocityHeading: 180 / Math.PI * Math.atan2(c.velocity.x, -c.velocity.z),
            vx: c.velocity.x, vz: c.velocity.z
        };
    }
    function liveTilt() { var l = Entities.getVehicleStats().live; return l && l.tilt !== undefined ? l.tilt : 1; }
    function liveAirspeed() { var l = Entities.getVehicleStats().live; return l && l.airspeed !== undefined ? l.airspeed : 0; }
    function liveLift() { var l = Entities.getVehicleStats().live; return l && l.wingLift !== undefined ? l.wingLift : 0; }
    function sample(dt) {
        var c = hullState();
        if (!c || !c.position) { return false; }
        // the nose's pitch and the deck's bank from the hull's axes (Euler angles misbehave about a yaw of 90 degrees)
        var frontY = Quat.getFront(c.rotation).y, rightY = Quat.getRight(c.rotation).y;
        var euler = { x: 180 / Math.PI * Math.asin(Math.max(-1, Math.min(1, frontY))), z: 180 / Math.PI * Math.asin(Math.max(-1, Math.min(1, rightY))) };
        var stats = Entities.getVehicleStats();
        var v = c.vehicle || {};
        var f = flight(c);
        var parentName = parentNameOf(MyAvatar.parentID);
        var parented = parentName === "ship" || parentName === "car";
        if (lastParent !== null && parentName !== lastParent) {
            crossings.push({ t: now(), index: samples.length, phase: phase, from: lastParent, to: parentName, aboard: parented, world: MyAvatar.position,
                             local: parented ? MyAvatar.localPosition : null });
            log("crossed from the " + lastParent + " to the " + parentName + " at " + now().toFixed(2) + " s, world " + fmtV(MyAvatar.position) +
                (parented ? ", local " + fmtV(MyAvatar.localPosition) : ""));
        }
        lastParent = parentName;
        var row = {
            index: samples.length, t: now(), dt: dt, mode: mode, phase: ROLE === "pilot" ? phase : (c.description || ""),
            pos: c.position, rot: c.rotation, vel: c.velocity, angVel: c.angularVelocity,
            height: f.height, aboveGround: f.aboveGround, climbRate: f.climbRate, heading: f.heading, padDistance: f.padDistance,
            speed: f.speed, speedAlong: f.speedAlong, pitch: euler.x, roll: euler.z, upY: Quat.getUp(c.rotation).y,
            controls: lastControls,
            state: { sequence: v.inputSequence, throttle: v.throttle, steering: v.steering, pitch: v.pitch, lift: v.lift, trim: v.trim,
                     ballast: v.ballast, fill: v.fill, tilt: v.tilt, stateTime: v.stateTime, driver: v.driver },
            stats: { corrections: stats.corrections, replaySteps: stats.replaySteps, maxError: stats.maxError, inputsSent: stats.inputsSent,
                     inputsReceived: stats.inputsReceived, inputsDropped: stats.inputsDropped, inputsStarved: stats.inputsStarved, vehicles: stats.vehicles,
                     streamStatesReceived: stats.streamStatesReceived, streamExtrapolations: stats.streamExtrapolations, rttMs: stats.rttMs,
                     delayMs: stats.delayMs, streamIntervalMs: stats.streamIntervalMs, streamJitterMs: stats.streamJitterMs,
                     inputsLate: stats.inputsLate, inputsEarly: stats.inputsEarly, bufferMargin: stats.bufferMargin, leadShifts: stats.leadShifts,
                     leadJumps: stats.leadJumps, timeShiftMs: stats.timeShiftMs },
            live: stats.live && (stats.live.airship || stats.live.aircraft) ? { altitude: stats.live.altitude, airDensity: stats.live.airDensity, buoyancy: stats.live.buoyancy,
                    ballastWeight: stats.live.ballastWeight, thrust: stats.live.thrust, verticalThrust: stats.live.verticalThrust,
                    airspeed: stats.live.airspeed, sideslip: stats.live.sideslip, incidence: stats.live.incidence, ballast: stats.live.ballast,
                    fill: stats.live.fill, tilt: stats.live.tilt, angleOfAttack: stats.live.angleOfAttack, wingLift: stats.live.wingLift,
                    wingDrag: stats.live.wingDrag, liftCoefficient: stats.live.liftCoefficient } : null,
            parented: parented, parentName: parentName, myPos: MyAvatar.position, myLocal: parented ? MyAvatar.localPosition : null,
            carLocal: carPose(),
            myLocalVel: parented ? MyAvatar.localVelocity : null,
            myLocalYaw: parented ? Quat.safeEulerAngles(MyAvatar.localOrientation).y : null,   // the facing on the deck, degrees
            atHelm: atHelm, walking: crewLeg.walking
        };
        if (ROLE === "crew") {
            if (ball) {
                var b = Entities.getEntityProperties(ball, ["localPosition", "parentID"]);
                row.ballLocal = (b && (b.parentID == hull || (car && b.parentID == car))) ? b.localPosition : null;
                row.ballParent = b ? parentNameOf(b.parentID) : "none";
            }
            // the driver as the state names it (a property comes to a script as a UUID, not a string: Uuid.isNull, not
            // a comparison with Uuid.NULL; and AvatarList.getAvatar of the null UUID is this client's own avatar)
            if (v.driver && !Uuid.isNull(v.driver) && !Uuid.isEqual(v.driver, MyAvatar.sessionUUID)) { pilotAvatarID = v.driver; }
            if (pilotAvatarID) {
                var a = AvatarList.getAvatar(pilotAvatarID);
                if (!pilotLookupLogged && a && a.sessionUUID) {
                    pilotLookupLogged = true;
                    log("the pilot " + pilotAvatarID + " looked up as " + a.sessionUUID + " (me " + MyAvatar.sessionUUID + "), at " +
                        JSON.stringify(a.position) + " local " + JSON.stringify(a.localPosition) + "; I am at " + JSON.stringify(MyAvatar.position) +
                        "; avatars " + JSON.stringify(AvatarList.getAvatarIdentifiers()));
                }
                if (a && a.sessionUUID) {
                    row.pilotAboard = a.parentID == hull;
                    row.pilotLocal = a.parentID == hull ? a.localPosition : null;
                }
            }
        }
        samples.push(row);
        if (samples.length % 60 === 0) {
            var owner = c.simulationOwner ? (c.simulationOwner.ownerID + " at priority " + c.simulationOwner.priority) : "none";
            var me = c.simulationOwner && c.simulationOwner.ownerID == MyAvatar.sessionUUID ? " (me)" : "";
            var line = "owner " + owner + me;
            if (line !== lastOwnerLine) {
                lastOwnerLine = line;
                events.push({ t: now(), owner: owner });
                log(line + " at " + now().toFixed(2) + " s; state sequence " + v.inputSequence + ", ballast " + (v.ballast || 0).toFixed(1) +
                    " kg, fill " + (v.fill || 0).toFixed(3) + ", driver " + v.driver);
            }
        }
        return true;
    }

    // ---- the pilot's course and autopilot ----
    var controlsLogged = false;
    var HOVER_THROTTLE = 0.59;                  // the aircraft's thrust that carries its weight: 1,200 kg x 9.8 over 20 kN
    var WEIGHT = 1200 * 9.8;                    // N, what the wing's lift is measured against
    var CRUISE_SPEED = 30;                      // m/s: the wing carries the weight at a lift coefficient of 0.9
    var HOME_GAIN = 0.2;                        // /s: the homing speed per metre left to the pad (a 5 s approach constant)
    var HOME_SPEED = 8;                         // m/s: the homing speed's cap
    var HOME_DEAD = 0.5;                        // m: close enough; inside it the craft only kills its velocity
    var HOVER_ATTITUDE = 24;                    // degrees of nose-down per unit of pitch input in hover, from the torque balance
                                                // (pitch lever 0.5 x tail arm 4.5 over levelling lever 0.5 x span 11)
    var HOVER_TRIM = 0.5 / (0.5 * 4.5);         // the pitch input that holds the nose level in hover: the thrust point is 0.5 m
                                                // aft of the centre of mass, so the thrust pitches the nose down
    var homeTrim = 0;                           // the rest of the trim, learned from the measured attitude
    var autoT = null;
    // The aircraft's autopilot. Hover: the throttle holds height (divided by the vertical part of the tilted thrust),
    // the nacelles' tilt goes to its target, the nose pitches against the forward speed to stop (or toward the pad to
    // go), the heading by yaw. Flight: the throttle holds the airspeed, the elevator the height, the ailerons a bank
    // (a turn's, or the one that brings the heading round), the rudder along with them.
    function aircraftAutopilot(step, s, tt) {
        var c = { throttle: 0, steering: 0, pitch: 0, lift: 0, trim: 0 };
        var a = step.auto;
        var dtA = autoT === null ? 0 : Math.max(0, Math.min(0.1, now() - autoT));
        autoT = now();
        if (a.tiltTo !== undefined) {
            var d = a.tiltTo - s.tilt;
            c.lift = Math.abs(d) < 0.005 ? 0 : (d > 0 ? 1 : -1);
        }
        if (a.hover) {
            var climbWanted = a.climb !== undefined ? a.climb : a.descend ? (s.aboveGround > 15 ? -3.0 : s.aboveGround > 6 ? -1.5 : -0.4) : clamp1(0.15 * (a.height - s.height));
            if (a.descend && s.aboveGround < 0.6) { climbWanted = -0.5; }
            var vertical = Math.max(0.35, Math.sin(s.tilt * Math.PI / 2));
            // braking from forward flight, the wing still carries part of the weight: the thrust carries the rest
            var wingShare = a.stop ? Math.max(0, Math.min(1, s.wingLift / WEIGHT)) : 0;
            c.throttle = Math.max(0.0, Math.min(1.0, (HOVER_THROTTLE * (1 - wingShare) + 0.08 * (climbWanted - s.climbRate)) / vertical));
            if (a.tiltTo === 1 && s.tilt < 0.7) { c.throttle = 0; }   // no forward blast while the nacelles come up
            var along = s.speedAlong;
            // the nose against the speed: gently while the wing still flies (a brake from forward flight), fully in hover
            var pitchMax = s.airspeed > 15 ? 0.5 : 1.0;
            if (a.stop || a.toPad) {
                // The hover's horizontal law, designed on a replica of the hover (hover-replica.py, beside this file) after five flights
                // circled the pad. The horizontal velocity wanted is toward the pad at a speed by the distance left (none
                // when stopping, or inside the last half metre); the error from it is taken in the craft's frame. A hover
                // has no sideways thrust, so the nose is turned along the correction (or the tail, when the correction is
                // small or a stop and the tail is nearer: the craft backs up rather than turning round over the pad), and
                // the nose-down attitude is set by the error along the nose, but only once the nose is within 15 degrees
                // of the correction: thrust while the nose still turns swings the velocity as fast as the heading, and the
                // two chase each other round. The yaw is throttled while the craft is pitched: a turn about a tilted axis
                // banks it, and the bank pushes it sideways into a circle (8 degrees of bank at full yaw and 15 degrees
                // of pitch, the flights showed). The attitude is commanded through the hover's torque balance, with the
                // thrust point's trim fed forward and the rest learned from the measured attitude, so "no thrust" is
                // exact: a residual of a third of a degree swings a creeping craft round. Braking from forward flight,
                // the nose-up is held back by any climb, so the wing's extra lift does not carry the craft up.
                var wantedSpeed = a.toPad ? Math.min(HOME_SPEED, HOME_GAIN * Math.max(0, s.padDistance - HOME_DEAD)) : 0;
                var b = s.padBearing * Math.PI / 180, h = s.heading * Math.PI / 180;
                var ex = wantedSpeed * Math.sin(b) - s.vx, ez = -wantedSpeed * Math.cos(b) - s.vz;
                var eAlong = ex * Math.sin(h) - ez * Math.cos(h), eSide = ex * Math.cos(h) + ez * Math.sin(h);
                var eMag = Math.sqrt(eAlong * eAlong + eSide * eSide);
                var off = 180 / Math.PI * Math.atan2(eSide, eAlong);
                if (a.stop || eMag < 4) {
                    if (off > 90) { off -= 180; } else if (off < -90) { off += 180; }
                }
                c.steering = clamp1(off / 30) * Math.min(1, Math.max(0, (eMag - 0.05) / 0.15));
                c.steering *= 3.0 / Math.max(3.0, Math.abs(s.pitchDeg));
                var nose;
                if (s.airspeed > 15) {
                    nose = Math.min(pitchMax, 0.25 * s.speedAlong) - 0.2 * s.climbRate - (a.height !== undefined ? 0.05 * (s.height - a.height) : 0);
                } else {
                    var attitude = Math.abs(off) < 15 ? Math.max(-20, Math.min(20, 4.0 * eAlong)) : 0;   // degrees nose-down wanted
                    homeTrim = Math.max(-0.3, Math.min(0.3, homeTrim + 0.05 * (-attitude - s.pitchDeg) * dtA));
                    nose = HOVER_TRIM + homeTrim - attitude / HOVER_ATTITUDE;
                }
                c.pitch = Math.max(-pitchMax, Math.min(pitchMax, nose));
            } else {
                // in the transition the speed is wanted; otherwise the nose comes up against it
                c.pitch = a.tiltTo !== undefined && a.tiltTo < 1 ? 0 : Math.max(-pitchMax, Math.min(pitchMax, 0.4 * along));
                if (a.heading === "hold" && holdHeading !== null) { c.steering = clamp1(wrap(holdHeading - s.heading) / 90); }
                if (a.heading === "pad") { c.steering = clamp1(wrap(s.padBearing - s.heading) / 90); }
            }
        } else if (a.flight) {
            // the classic pairing: the throttle holds the height, the elevator holds the airspeed, the latter through an
            // attitude loop (a pitch angle wanted, the elevator for the angle with the pitch rate damped): level flight
            // at the cruising speed is the nose 10 degrees up, and the nose comes up to slow and down to speed up
            var speedWanted = a.speed || CRUISE_SPEED;
            c.throttle = Math.max(0.05, Math.min(0.8, 0.1 + 0.03 * (a.height - s.height) - 0.05 * s.climbRate));
            var pitchWanted = Math.max(-8, Math.min(16, 10 + 0.5 * (s.airspeed - speedWanted)));
            c.pitch = clamp1(0.08 * (pitchWanted - s.pitchDeg) - 0.01 * s.pitchRate);
            var bankWanted = 0;
            if (a.bank !== undefined) {
                bankWanted = a.bank;
            } else if (a.heading === "hold" && holdHeading !== null) {
                bankWanted = Math.max(-30, Math.min(30, 0.6 * wrap(holdHeading - s.heading)));
            } else if (a.heading === "pad") {
                bankWanted = Math.max(-30, Math.min(30, 0.6 * wrap(s.padBearing - s.heading)));
            }
            c.steering = Math.max(-0.5, Math.min(0.5, 0.03 * (bankWanted - s.bank) - 0.005 * s.rollRate));
        }
        return c;
    }
    function autopilot(step, s, tt) {
        var c = { throttle: 0, steering: 0, pitch: 0, lift: 0, trim: 0 };
        if (step.controls) {
            Object.keys(step.controls).forEach(function (k) { c[k] = step.controls[k]; });
            return c;
        }
        if (KIND === "aircraft") { return aircraftAutopilot(step, s, tt); }
        var a = step.auto;
        c.throttle = a.throttle || 0;
        if (a.throttle === "home") {
            // slow while the nose is off the pad (an airship's turning circle is its speed over its turn rate)
            c.throttle = Math.abs(wrap(s.padBearing - s.heading)) < 25 ? 0.5 : 0.15;
        }
        c.trim = a.trim || 0;
        c.pitch = a.pitch || 0;
        if (a.steering !== undefined) {
            c.steering = a.steering;
        } else if (a.heading === "hold" || a.heading === "pad") {
            var target = a.heading === "pad" ? s.padBearing : holdHeading;
            if (target !== null) { c.steering = clamp1(wrap(target - s.heading) / 30); }
        }
        if (s.padDistance > GUARD_RADIUS) {
            // too far out whatever the phase: home
            c.steering = clamp1(wrap(s.padBearing - s.heading) / 30);
        }
        if (a.hover) {
            c.lift = clamp1(-1.0 * s.climbRate);
        } else if (a.descend) {
            // down at a metre a second until the last metres, then gently, and not into the ground
            var wanted = s.aboveGround > 8 ? -1.5 : -0.3;
            c.lift = clamp1((wanted - s.climbRate) * 0.8);
            if (s.aboveGround < 0.6) { c.lift = -1; }   // settle onto the ground and stay
        } else if (a.height !== undefined) {
            c.lift = clamp1(0.1 * (a.height - s.height) - 0.5 * s.climbRate);
        }
        return c;
    }
    function applyPhase(tt) {
        var step = COURSE[phaseIndex];
        var hs = hullState();
        if (!hs || !hs.position) { return; }
        var s = flight(hs);
        var c = autopilot(step, s, tt);
        lastControls = c;
        var accepted = Entities.setVehicleControls(hull, c);
        if (!controlsLogged) {
            controlsLogged = true;
            log("controls " + (accepted ? "accepted" : "REFUSED (not a vehicle here)") + "; parent " + (MyAvatar.parentID == hull ? "the ship" : MyAvatar.parentID) +
                ", local " + fmtV(MyAvatar.localPosition) + ", world " + fmtV(MyAvatar.position));
        }
        if (step.until && step.until(s)) {
            log(step.name + " reached its mark at " + tt.toFixed(1) + " s: height " + s.height.toFixed(1) + " m, " + s.padDistance.toFixed(0) + " m from the pad");
            nextPhase();
        }
    }
    function nextPhase() {
        phaseIndex++;
        if (phaseIndex >= COURSE.length) { return false; }
        var step = COURSE[phaseIndex];
        if (step.auto && step.auto.heading === "hold") {
            var hs = hullState();
            holdHeading = hs && hs.rotation ? headingOf(hs.rotation) : holdHeading;
        }
        setPhase(step.name);
        modeT0 = now();
        return true;
    }

    function drivePilot() {
        var tt = now() - modeT0;
        if (mode === "boot") {
            if (tt > BOOT && tt > bootTried + 1) {
                bootTried = tt;
                var removed = Rig.deleteAll(LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE);
                if (removed > 0) { log("deleted " + removed + " leftover airship entities of an earlier run"); }
                terrain = LandRover.createTerrain(TERRAIN_URL, LIFETIME);
                if (!terrain || terrain === Uuid.NULL) {
                    terrain = null;
                    if (bootTried < BOOT + 2) { log("no entity server yet at " + tt.toFixed(1) + " s; trying again each second"); }
                    return;
                }
                log("terrain " + terrain + ", file " + TERRAIN_URL + " (at " + tt.toFixed(1) + " s)");
                setMode("terrain");
            }
        } else if (mode === "terrain") {
            var y = groundReady();
            if (y !== null) {
                padGround = y;
                log("ground ready after " + tt.toFixed(1) + " s: y " + y.toFixed(3));
                standBesidePad(6);
                setMode("ground-settle");
            } else if (tt > 60) { finish("no ground"); return; }
        } else if (mode === "ground-settle") {
            if (tt > 2) {
                var c = pad();
                var made = Rig.create(c.x, c.z, 0, LIFETIME);
                hull = made.hull;
                if (DECK_CAR && KIND === "airship") {
                    var deck = Airship.createDeckTrack(hull, LIFETIME);
                    track = deck.track; car = deck.car;
                    var cp = Entities.getEntityProperties(car, ["parentID", "trackID", "frame", "localPosition"]);
                    log("deck track " + track + ", car " + car + ": parent is the track " + (cp.parentID == track) + ", trackID is the track " +
                        (cp.trackID == track) + ", frame " + cp.frame + ", local " + fmtV(cp.localPosition));
                }
                var hp = Entities.getEntityProperties(hull, ["position", "vehicle", "dynamic", "frame"]);
                log(KIND + " " + hull + " at " + fmtV(hp.position) + ", dynamic " + hp.dynamic + ", frame " + hp.frame + ", type " +
                    (hp.vehicle && hp.vehicle.type) + ", mass " + (hp.vehicle && hp.vehicle.mass) + " kg" +
                    (KIND === "airship" ? ", ballast " + (hp.vehicle && hp.vehicle.ballast) + " kg, envelope " + (hp.vehicle && hp.vehicle.envelopeVolume) + " m3"
                        : ", wing " + (hp.vehicle && hp.vehicle.wingArea) + " m2, span " + (hp.vehicle && hp.vehicle.wingSpan) + " m, tilt " + (hp.vehicle && hp.vehicle.tilt)) +
                    ", " + made.parts.length + " parts");
                setMode("settle");
            }
        } else if (mode === "settle") {
            if (tt > 4) {
                var s = Entities.getEntityProperties(hull, ["position", "velocity"]);
                var ground = LandRover.groundHeight(s.position.x, s.position.z);
                log("settled: hull y " + s.position.y.toFixed(3) + " = ground + " + (s.position.y - ground).toFixed(3) + " (expected about " +
                    Rig.DECK_CENTER_ABOVE_GROUND.toFixed(2) + "), speed " + Vec3.length(s.velocity).toFixed(3));
                if (s.position.y < ground - 1) { finish("fell through"); return; }
                if (s.position.y > ground + 5) { log("the ship is already airborne: too light on the ground"); }
                takeHelm();
                setMode("helm");
            }
        } else if (mode === "helm") {
            if (tt > 1 && !helmChecked) {
                helmChecked = true;
                var hp = Entities.getEntityProperties(hull, ["position", "rotation"]);
                var seatAt = Vec3.sum(Vec3.sum(hp.position, Vec3.multiplyQbyV(hp.rotation, vec3Of(Rig.HELM.position))), { x: 0, y: 1.0, z: 0 });
                if (Vec3.distance(MyAvatar.position, seatAt) > 1.5) {
                    log("not at the helm after 1 s (at " + fmtV(MyAvatar.position) + ", the seat at " + fmtV(seatAt) + "): placed again");
                    takeHelm();
                }
            }
            if (tt > 3) { setMode("course"); nextPhase(); }
        } else if (mode === "course") {
            if (tt >= COURSE[phaseIndex].seconds) {
                if (!nextPhase()) { setMode("leave"); leaveHelm(); return; }
                tt = 0;
            }
            if (phase === "done") { setMode("leave"); leaveHelm(); return; }
            applyPhase(tt);
        } else if (mode === "leave") {
            if (tt > 2) { finish("done"); }
        }
    }

    // ---- the crew ----
    function walk(direction) {
        // along the deck, facing the bow throughout, in the ship's own axes: forward to the bow (direction -1), backward
        // to the stern (+1). The scripted motor's "avatar" frame has +z forward (MyAvatar::updateMotors turns it by 180
        // degrees); a crew that faced the stern would be turned back by a look-at camera, the default, which turns the
        // avatar to face where the camera looks once it is more than 135 degrees off. The facing is set at every leg, so
        // a stray turn of the avatar (a hand on the mouse) does not send the crew into a rail.
        crewLeg.walking = true;
        crewLeg.direction = direction;
        var hp = Entities.getEntityProperties(hull, ["rotation"]);
        if (hp && hp.rotation) { MyAvatar.orientation = hp.rotation; }
        MyAvatar.motorReferenceFrame = "avatar";
        MyAvatar.motorTimescale = 0.2;
        MyAvatar.motorVelocity = { x: 0, y: 0, z: direction < 0 ? WALK_SPEED : -WALK_SPEED };
    }
    function stopWalking() {
        crewLeg.walking = false;
        MyAvatar.motorVelocity = Vec3.ZERO;
    }
    // walk toward a point on the deck (x, z in the ship's coordinates), facing the bow: the "avatar" motor frame is the
    // avatar's turned by 180 degrees, so the ship's +x and +z are the motor's -x and -z. Returns true when there.
    function walkToward(x, z) {
        var me = MyAvatar.localPosition;
        var dx = x - me.x, dz = z - me.z;
        var d = Math.sqrt(dx * dx + dz * dz);
        if (d < 0.2) { if (crewLeg.walking) { stopWalking(); } return true; }
        if (!crewLeg.walking) {
            var hp = Entities.getEntityProperties(hull, ["rotation"]);
            if (hp && hp.rotation) { MyAvatar.orientation = hp.rotation; }
            crewLeg.walking = true;
            crewLeg.direction = 0;
            MyAvatar.motorReferenceFrame = "avatar";
            MyAvatar.motorTimescale = 0.2;
        }
        MyAvatar.motorVelocity = { x: -WALK_SPEED * dx / d, y: 0, z: -WALK_SPEED * dz / d };
        return false;
    }
    // where the car's centre is on the deck, in the ship's coordinates (the track's middle plus the car's place on it)
    function carDeckZ(cp) { return 0.5 * (Airship.CAR.stations[0] + Airship.CAR.stations[1]) + cp.z; }
    function setRide(state) {
        carRide.state = state; carRide.t = now();
        events.push({ t: now(), ride: state });
        log("ride: " + state + " at " + now().toFixed(2) + " s, parent the " + parentNameOf(MyAvatar.parentID) + ", local " + fmtV(MyAvatar.localPosition));
    }
    // The crew's ride on the trolley: walk across the deck onto it at a station, ride standing still, step off onto
    // the deck at every second stop, wait for it to come back to that station, step on again.
    function driveCar() {
        var tt = now() - carRide.t;
        var pn = parentNameOf(MyAvatar.parentID);
        var cp = carPose();
        var standing = cp && cp.speed !== null && Math.abs(cp.speed) < 0.01 && cp.station >= 0;
        if (cp && cp.speed !== null && Math.abs(cp.speed) > 0.1) { carRide.seenMoving = true; }
        if (carRide.state.indexOf("probe") === 0 || carRide.state === "probing") {
            driveProbe(tt, pn, cp, standing);
        } else if (carRide.state === "toCar") {
            if (pn === "car") {
                stopWalking();
                carRide.boardings++;
                carRide.lastStation = standing ? cp.station : -1;
                carRide.seenMoving = false;
                if (!ball) { dropBall(); }
                setRide("riding");
            } else if (tt > 8 || !cp) {
                stopWalking();
                carRide.failures++;
                log("could not board the car in " + tt.toFixed(1) + " s: parent the " + pn + ", local " + fmtV(MyAvatar.localPosition) +
                    (cp ? ", the car at deck z " + carDeckZ(cp).toFixed(2) + " speed " + cp.speed.toFixed(2) : ""));
                setRide("waiting");
            } else if (Math.abs(carDeckZ(cp) - MyAvatar.localPosition.z) > 0.5 * Airship.CAR.dimensions.z - 0.3) {
                // not abreast of the car (its ends are railed, round 8): to port of its line first, then along it
                var sideX = Airship.CAR.x - 0.5 * Airship.CAR.dimensions.x - 0.5;
                if (MyAvatar.localPosition.x > sideX + 0.2) {
                    walkToward(sideX, MyAvatar.localPosition.z);
                } else {
                    walkToward(sideX, carDeckZ(cp));
                }
            } else {
                // to the car's centre, up onto its floor (a 0.3 m step)
                walkToward(Airship.CAR.x, carDeckZ(cp));
            }
        } else if (carRide.state === "riding") {
            // a stop: the train standing at a station other than the one it last stood at
            if (standing && carRide.seenMoving && cp.station !== carRide.lastStation) {
                carRide.stops++;
                carRide.lastStation = cp.station;
                carRide.seenMoving = false;
                log("the car stopped at station " + cp.station + " (stop " + carRide.stops + "), my local in the car " + fmtV(MyAvatar.localPosition));
                if (carRide.stops % 2 === 0) {
                    carRide.offAtStation = cp.station;
                    setRide("stepOff");
                }
            }
        } else if (carRide.state === "stepOff") {
            // off the port side onto the deck, beside the station: a metre to port of wherever I am (the car's axes are
            // the ship's), until the ship has me and I am clear of the car
            var offX = Airship.CAR.x - 0.5 * Airship.CAR.dimensions.x - 0.5;
            if (pn === "ship" && MyAvatar.localPosition.x < offX + 0.1) {
                stopWalking();
                carRide.seenMoving = false;
                carRide.probed = false;
                setRide("waiting");
            } else if (pn === "ship" || pn === "car") {
                walkToward(MyAvatar.localPosition.x - 1.0, MyAvatar.localPosition.z);
            } else if (tt > 8) {
                stopWalking();
                carRide.failures++;
                log("could not step off the car in " + tt.toFixed(1) + " s: parent the " + pn + ", local " + fmtV(MyAvatar.localPosition));
                setRide("waiting");
            }
        } else if (carRide.state === "waiting") {
            // the car standing within reach (its floor spans a metre each way of its centre): walk to it; once per
            // wait, first push into its end rail from the deck (the rail probe)
            var near = cp && Math.abs(carDeckZ(cp) - MyAvatar.localPosition.z) < 0.5 * Airship.CAR.dimensions.z + 1.5;
            if (!carRide.probed && pn === "ship" && cp) {
                // the probe first, once the car has left the bow station (it stands there when I step off)
                if (tt > 1 && !(standing && Math.abs(carDeckZ(cp) - Airship.CAR.stations[0]) < 0.3)) {
                    carRide.probed = true;
                    setRide("probeOut");
                }
            } else if (standing && near && tt > 1) {
                setRide("toCar");
            } else if (tt > 60) {
                log("waited " + tt.toFixed(0) + " s for the car; boarding wherever it is");
                carRide.offAtStation = -1;
                carRide.seenMoving = true;
            }
        }
    }
    // The rail probe (round 8): stand on the track's line 1.2 m beyond the bow station, where the car never passes,
    // wait for it to arrive and stand, then push toward its centre for 2.5 s. The end rail is a child of the car, in
    // the car's world; its copy on the car's body in the ship's world is what stops the crew, at the rail's face plus
    // the capsule's radius, with the ship still their parent. Without the copy the crew walks onto the floor and the
    // car's world takes them. Then back to the side, to board at the car's next stop.
    function probeLeg(x, z, next, tt) {
        if (walkToward(x, z)) {
            setRide(next);
        } else if (tt > 15) {
            stopWalking();
            log("rail probe: could not reach (" + x.toFixed(2) + ", " + z.toFixed(2) + ") in " + tt.toFixed(1) + " s, at " + fmtV(MyAvatar.localPosition));
            setRide("waiting");
        }
    }
    function driveProbe(tt, pn, cp, standing) {
        var offX = Airship.CAR.x - 0.5 * Airship.CAR.dimensions.x - 0.5;
        var stationZ = Airship.CAR.stations[0];
        var probeZ = stationZ - (0.5 * Airship.CAR.dimensions.z + 1.2);
        var atBow = standing && cp && Math.abs(carDeckZ(cp) - stationZ) < 0.3;
        if (pn === "car") {
            // through the rail: no copy, or a hole in it
            stopWalking();
            if (carRide.probe) { carRide.probe.boardedThroughRail = true; probes.push(carRide.probe); carRide.probe = null; }
            log("rail probe: the car's world took me at local " + fmtV(MyAvatar.localPosition) + " (parent the car) in " + carRide.state);
            carRide.boardings++;
            carRide.lastStation = -1;
            carRide.seenMoving = false;
            setRide("riding");
            return;
        }
        if (carRide.state === "probeOut") {
            probeLeg(offX, probeZ, "probeIn", tt);
        } else if (carRide.state === "probeIn") {
            probeLeg(Airship.CAR.x, probeZ, "probeWait", tt);
            if (carRide.state === "probeWait") { carRide.probeHold = atBow; }   // standing there already: its next stop
        } else if (carRide.state === "probeWait") {
            if (!atBow) { carRide.probeHold = false; }
            if (atBow && !carRide.probeHold) {
                carRide.probe = { t: now(), stationZ: stationZ, fromZ: probeZ, minDz: 1.0e9, maxDy: -1.0e9, minDy: 1.0e9,
                                  minX: 1.0e9, maxX: -1.0e9, samples: 0, parents: {}, boardedThroughRail: false };
                setRide("probing");
            } else if (tt > 40) {
                log("rail probe: the car did not come to the bow station in " + tt.toFixed(0) + " s");
                setRide("probeBack");
            }
        } else if (carRide.state === "probing") {
            var me = MyAvatar.localPosition;
            var pr = carRide.probe;
            pr.samples++;
            pr.minDz = Math.min(pr.minDz, Math.abs(me.z - stationZ));
            pr.maxDy = Math.max(pr.maxDy, me.y); pr.minDy = Math.min(pr.minDy, me.y);
            pr.minX = Math.min(pr.minX, me.x); pr.maxX = Math.max(pr.maxX, me.x);
            pr.parents[pn] = (pr.parents[pn] || 0) + 1;
            walkToward(Airship.CAR.x, stationZ);
            if (tt > 2.5 || !atBow) {
                stopWalking();
                pr.duration = tt;
                pr.carLeft = !atBow;
                probes.push(pr);
                carRide.probe = null;
                log("rail probe at the bow station: pushed " + tt.toFixed(1) + " s" + (atBow ? "" : " (the car left)") + ", closest to the car's centre " +
                    pr.minDz.toFixed(3) + " m (the rail's far face at " + (0.5 * Airship.CAR.dimensions.z).toFixed(2) + "), local y " +
                    pr.minDy.toFixed(3) + " to " + pr.maxDy.toFixed(3) + ", x " + pr.minX.toFixed(2) + " to " + pr.maxX.toFixed(2) + ", parent " + JSON.stringify(pr.parents));
                setRide("probeBack");
            }
        } else if (carRide.state === "probeBack") {
            probeLeg(offX, probeZ, "probeSide", tt);
        } else if (carRide.state === "probeSide") {
            probeLeg(offX, stationZ, "waiting", tt);
        }
    }
    function dropBall() {
        // on the deck beside the crew, or in the car when the crew rides it (its parent is then the car, a frame in a frame)
        var inCar = car && parentNameOf(MyAvatar.parentID) === "car";
        ball = Entities.addEntity({
            type: "Sphere", name: "airship-ball", parentID: inCar ? car : hull, lifetime: LIFETIME,
            localPosition: inCar ? { x: 0, y: 0.6, z: 0.5 } : { x: 0.8, y: 1.2, z: 2.5 }, dimensions: { x: 0.3, y: 0.3, z: 0.3 },
            color: { red: 220, green: 40, blue: 40 }, dynamic: true, collisionless: false,
            gravity: { x: 0, y: -9.8, z: 0 }, damping: 0.1, angularDamping: 0.1, friction: 0.5, restitution: 0.2,
            grab: { grabbable: false }
        });
        log("dropped a ball " + (inCar ? "in the car: " : "on the deck: ") + ball);
    }
    function teleportShip() {
        var p = Entities.getEntityProperties(hull, ["position"]);
        var to = Vec3.sum(p.position, { x: 30, y: 20, z: 0 });
        var before = { ship: p.position, me: MyAvatar.position, local: MyAvatar.localPosition };
        Entities.editEntity(hull, { position: to });
        teleportDone = true;
        events.push({ t: now(), teleport: { from: p.position, to: to, meBefore: before.me, localBefore: before.local } });
        log("teleported the ship by an edit from " + fmtV(p.position) + " to " + fmtV(to) + "; I was at " + fmtV(before.me) + ", local " + fmtV(before.local));
        Script.setTimeout(function () {
            var q = Entities.getEntityProperties(hull, ["position"]);
            log("3 s after the teleport: the ship is at " + fmtV(q.position) + " (" + Vec3.distance(q.position, to).toFixed(2) + " m from where it was put), I am at " +
                fmtV(MyAvatar.position) + ", local " + fmtV(MyAvatar.localPosition) + ", parent " + (MyAvatar.parentID == hull ? "the ship" : MyAvatar.parentID));
            events.push({ t: now(), afterTeleport: { ship: q.position, me: MyAvatar.position, local: MyAvatar.localPosition, aboard: MyAvatar.parentID == hull } });
        }, 3000);
    }
    function driveCrew() {
        var tt = now() - modeT0;
        if (mode === "boot") {
            if (tt > 2) { standBesidePad(6); setMode("find"); }
        } else if (mode === "find") {
            if (tt > 3) { standBesidePad(6); modeT0 = now(); }
            var id = findShip();
            if (id) {
                hull = id;
                var hp = Entities.getEntityProperties(hull, ["position", "rotation"]);
                log("found the airship " + hull + " at " + fmtV(hp.position) + " after " + tt.toFixed(1) + " s");
                padGround = LandRover.groundHeight(pad().x, pad().z);
                // beside the deck's starboard rail, facing the bow... no: facing the deck, to walk aboard across it
                var side = Vec3.sum(hp.position, Vec3.multiplyQbyV(hp.rotation, { x: 3.2, y: 0, z: 3.0 }));
                MyAvatar.position = { x: side.x, y: LandRover.groundHeight(side.x, side.z) + 1.0, z: side.z };
                MyAvatar.orientation = Quat.multiply(hp.rotation, Quat.fromPitchYawRollDegrees(0, 90, 0));   // facing -x of the ship: across the deck
                // the crew's facing is the script's: a look-at camera (the default) turns the avatar toward where the
                // camera looks, and a bot has nobody at the mouse to turn the camera with it
                Camera.mode = "third person";
                setMode("board");
            } else if (tt > 90) { finish("no airship"); }
        } else if (mode === "board") {
            // the rails stand at the deck's edges: the crew steps over the rail's gap at the stern... the rails have no gap,
            // so the crew climbs aboard where the rail ends at the stern corner, then walks to the middle
            if (tt < 0.5) {
                var hp2 = Entities.getEntityProperties(hull, ["position", "rotation"]);
                var corner = Vec3.sum(hp2.position, Vec3.multiplyQbyV(hp2.rotation, { x: 0.0, y: 1.2, z: 5.5 }));
                MyAvatar.position = corner;
                MyAvatar.orientation = hp2.rotation;
            } else if (tt > 2 && MyAvatar.parentID == hull) {
                log("aboard: local " + fmtV(MyAvatar.localPosition) + " (stored), world " + fmtV(MyAvatar.position));
                if (DECK_CAR) {
                    if (findCar()) {
                        log("the deck car " + car + " on track " + track);
                        setRide("toCar");
                    } else {
                        log("no deck car found; riding the deck instead");
                        dropBall();
                    }
                } else {
                    dropBall();
                }
                setMode("ride");
                crewLeg.t = now();
            } else if (tt > 10) {
                log("not aboard after " + tt.toFixed(0) + " s: parent " + MyAvatar.parentID + ", world " + fmtV(MyAvatar.position));
                finish("not aboard");
            }
        } else if (mode === "ride") {
            var c = Entities.getEntityProperties(hull, ["description"]);
            var p = c ? (c.description || "") : "";
            if (p && courseT0 < 0) { courseT0 = now(); }   // the crew's clock for the timeout runs from the pilot's first phase
            if (p !== phase) {
                setPhase(p);
                if (p === "teleport" && !teleportDone) { stopWalking(); Script.setTimeout(teleportShip, 1500); }
            }
            if (car && carRide.state !== "none") {
                driveCar();
            } else {
                // by turns: 5 s standing, 5 s walking forward, 5 s standing, 5 s walking back, while the ship flies
                var flying = ["climb", "cruise", "turn-right", "turn-left", "nose-up", "nose-down", "return", "descend"].indexOf(p) >= 0;
                var leg = Math.floor((now() - crewLeg.t) / 5) % 4;
                if (flying && leg === 1 && !crewLeg.walking) { walk(-1); }
                else if (flying && leg === 3 && !crewLeg.walking) { walk(1); }
                else if ((!flying || leg === 0 || leg === 2) && crewLeg.walking) { stopWalking(); }
                // the rails keep the crew aboard; should it be walking into a rail, turn back early
                if (crewLeg.walking && MyAvatar.parentID == hull) {
                    var lz = MyAvatar.localPosition.z;
                    if ((crewLeg.direction < 0 && lz < -4.0) || (crewLeg.direction > 0 && lz > 4.5)) { stopWalking(); }
                }
            }
            if (p === "done" || tt > 240) { stopWalking(); setMode("leave"); }
        } else if (mode === "leave") {
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
        var out = { n: 0, rmsMm: 0, maxMm: 0, driftMm: 0 };
        var first = null, last = null, sum = 0;
        for (var i = 1; i < rows.length; i++) {
            if (rows[i].index === rows[i - 1].index + 1 && rows[i][key] && rows[i - 1][key] && rows[i].parentName === rows[i - 1].parentName) {
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
    function phaseStats(name, rows) {
        if (rows.length < 3) { return null; }
        var first = rows[0], last = rows[rows.length - 1];
        var hMin = Infinity, hMax = -Infinity, climbMax = -Infinity, climbMin = Infinity, speedMax = 0, heading = 0, distance = 0;
        var pitchMax = 0, rollMax = 0, airspeedMax = 0, aboard = 0, ballOn = 0, ballN = 0, ballX = 0, ballZ = 0, ballYMin = Infinity, ballYMax = -Infinity;
        var tiltMin = Infinity, tiltMax = -Infinity, aoaMin = Infinity, aoaMax = -Infinity, liftMax = 0, thrustMax = 0;
        var standing = [], walking = [], pilotAboard = 0, pilotN = 0;
        var inCar = [], carOn = 0, carN = 0, carX = 0, carY = 0, carZMin = Infinity, carZMax = -Infinity, carParentTrack = 0, carStops = 0;
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i];
            if (r.height < hMin) { hMin = r.height; } if (r.height > hMax) { hMax = r.height; }
            if (r.climbRate > climbMax) { climbMax = r.climbRate; } if (r.climbRate < climbMin) { climbMin = r.climbRate; }
            if (r.speed > speedMax) { speedMax = r.speed; }
            if (Math.abs(r.pitch) > pitchMax) { pitchMax = Math.abs(r.pitch); }
            if (Math.abs(r.roll) > rollMax) { rollMax = Math.abs(r.roll); }
            if (r.live && r.live.airspeed > airspeedMax) { airspeedMax = r.live.airspeed; }
            if (r.live && r.live.tilt !== undefined) {
                tiltMin = Math.min(tiltMin, r.live.tilt); tiltMax = Math.max(tiltMax, r.live.tilt);
                aoaMin = Math.min(aoaMin, r.live.angleOfAttack || 0); aoaMax = Math.max(aoaMax, r.live.angleOfAttack || 0);
                liftMax = Math.max(liftMax, r.live.wingLift || 0); thrustMax = Math.max(thrustMax, r.live.thrust || 0);
            }
            if (i > 0 && r.index === rows[i - 1].index + 1) { heading += wrap(r.heading - rows[i - 1].heading); distance += Vec3.distance(rows[i - 1].pos, r.pos); }
            if (r.parented) { aboard++; }
            if (r.parentName === "car") { inCar.push(r); }
            if (r.parentName === "car" && !r.walking) { carOn++; }
            (r.walking ? walking : standing).push(r);
            if (r.carLocal) {
                carN++;
                carX = Math.max(carX, Math.abs(r.carLocal.x)); carY = Math.max(carY, Math.abs(r.carLocal.y));
                carZMin = Math.min(carZMin, r.carLocal.z); carZMax = Math.max(carZMax, r.carLocal.z);
                if (r.carLocal.parentIsTrack) { carParentTrack++; }
                if (i > 0 && rows[i - 1].carLocal && Math.abs(r.carLocal.speed) < 0.01 && Math.abs(rows[i - 1].carLocal.speed) >= 0.01) { carStops++; }
            }
            if (r.ballLocal) {
                ballN++;
                var on = r.ballParent === "car"
                    ? (Math.abs(r.ballLocal.x) < 0.9 && Math.abs(r.ballLocal.z) < 1.2 && r.ballLocal.y > 0.05 && r.ballLocal.y < 1.2)
                    : (Math.abs(r.ballLocal.x) < 2.0 && Math.abs(r.ballLocal.z) < 7.0 && r.ballLocal.y > 0.1 && r.ballLocal.y < 1.5);
                if (on) { ballOn++; }
                ballX = Math.max(ballX, Math.abs(r.ballLocal.x)); ballZ = Math.max(ballZ, Math.abs(r.ballLocal.z));
                ballYMin = Math.min(ballYMin, r.ballLocal.y); ballYMax = Math.max(ballYMax, r.ballLocal.y);
            }
            if (r.pilotAboard !== undefined) { pilotN++; if (r.pilotAboard) { pilotAboard++; } }
        }
        var s0 = first.stats || {}, s1 = last.stats || {};
        var l1 = last.live || {};
        return {
            phase: name, frames: rows.length, seconds: last.t - first.t, aboardFrames: aboard,
            heightMin: hMin, heightMax: hMax, heightEnd: last.height, climbMax: climbMax, climbMin: climbMin, speedMax: speedMax,
            headingDeg: heading, distanceM: distance, pitchMaxDeg: pitchMax, rollMaxDeg: rollMax, airspeedMax: airspeedMax,
            padDistanceEnd: last.padDistance,
            buoyancy: l1.buoyancy, ballast: l1.ballast, fill: l1.fill, airDensity: l1.airDensity,
            stateBallast: last.state.ballast, stateFill: last.state.fill,
            aircraft: KIND === "aircraft" ? { tiltMin: tiltMin, tiltMax: tiltMax, aoaMin: aoaMin, aoaMax: aoaMax, liftMax: liftMax, thrustMax: thrustMax, stateTilt: last.state.tilt } : null,
            myLocalStanding: jitter(standing, "myLocal"), myLocalWalking: jitter(walking, "myLocal"), walkingFrames: walking.length,
            inCarFrames: inCar.length, carStandingFrames: carOn, myLocalInCar: jitter(inCar.filter(function (r) { return !r.walking; }), "myLocal"),
            car: carN ? { frames: carN, parentIsTrack: carParentTrack, maxX: carX, maxY: carY, zMin: carZMin, zMax: carZMax, stops: carStops } : null,
            facing: facingRange(rows),   // the crew's heading on the deck: a camera that stays in the world turns it as the ship turns
            pilotLocal: jitter(rows, "pilotLocal"), pilotAboardFrames: pilotAboard, pilotFrames: pilotN,
            ball: ballN ? { frames: ballN, onDeck: ballOn, maxX: ballX, maxZ: ballZ, yMin: ballYMin, yMax: ballYMax } : null,
            hullStep: steps(rows, "pos"),
            corrections: (s1.corrections || 0) - (s0.corrections || 0), replaySteps: (s1.replaySteps || 0) - (s0.replaySteps || 0),
            maxError: s1.maxError, inputsSent: (s1.inputsSent || 0) - (s0.inputsSent || 0),
            sequenceFrom: first.state.sequence, sequenceTo: last.state.sequence, stateUpdates: countStateUpdates(rows)
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

    function finish(reason) {
        if (finished) { return; }
        finished = true;
        Script.update.disconnect(onUpdate);
        if (ROLE === "pilot") { leaveHelm(); } else { stopWalking(); }
        var summary = [];
        var seen = [];
        samples.forEach(function (r) { if (r.phase && seen.indexOf(r.phase) < 0) { seen.push(r.phase); } });
        seen.forEach(function (name) {
            var s = phaseStats(name, samples.filter(function (r) { return r.phase === name; }));
            if (s) { summary.push(s); }
        });
        summary.forEach(function (s) {
            log(s.phase + ": " + s.frames + " frames in " + s.seconds.toFixed(1) + " s; aboard " + s.aboardFrames + "/" + s.frames +
                "; height " + s.heightMin.toFixed(1) + " to " + s.heightMax.toFixed(1) + " m (end " + s.heightEnd.toFixed(1) + ")" +
                "; climb " + s.climbMin.toFixed(2) + " to " + s.climbMax.toFixed(2) + " m/s; speed up to " + s.speedMax.toFixed(2) + " m/s (airspeed " + s.airspeedMax.toFixed(1) + ")" +
                "; " + s.distanceM.toFixed(0) + " m; heading " + s.headingDeg.toFixed(0) + " deg; pitch up to " + s.pitchMaxDeg.toFixed(1) + " deg, roll " + s.rollMaxDeg.toFixed(1) +
                "; " + s.padDistanceEnd.toFixed(0) + " m from the pad at the end" +
                (s.aircraft ? "; tilt " + s.aircraft.tiltMin.toFixed(2) + " to " + s.aircraft.tiltMax.toFixed(2) + " (state " + (s.aircraft.stateTilt || 0).toFixed(2) +
                    "), angle of attack " + s.aircraft.aoaMin.toFixed(1) + " to " + s.aircraft.aoaMax.toFixed(1) + " deg, wing lift up to " + s.aircraft.liftMax.toFixed(0) +
                    " N, thrust up to " + s.aircraft.thrustMax.toFixed(0) + " N"
                    : "; buoyancy " + (s.buoyancy || 0).toFixed(0) + " N, ballast " + (s.ballast || 0).toFixed(1) + " kg, fill " + (s.fill || 0).toFixed(3) +
                    " (state " + (s.stateBallast || 0).toFixed(1) + " kg, " + (s.stateFill || 0).toFixed(3) + ")") +
                "; my stored local standing " + fmtJitter(s.myLocalStanding) + (s.walkingFrames ? ", walking " + fmtJitter(s.myLocalWalking) : "") +
                (s.facing ? "; facing on the deck " + s.facing.min.toFixed(1) + " to " + s.facing.max.toFixed(1) + " deg" : "") +
                (s.car ? "; the car on its track " + s.car.parentIsTrack + "/" + s.car.frames + " (|x| to " + (1000 * s.car.maxX).toFixed(1) + " mm, |y| to " +
                    (1000 * s.car.maxY).toFixed(1) + " mm, z " + s.car.zMin.toFixed(2) + " to " + s.car.zMax.toFixed(2) + ", " + s.car.stops + " stops)" : "") +
                (s.inCarFrames ? "; in the car " + s.inCarFrames + "/" + s.frames + ", standing there " + fmtJitter(s.myLocalInCar) : "") +
                (ROLE === "crew" ? "; the pilot's stored local " + fmtJitter(s.pilotLocal) + ", aboard " + s.pilotAboardFrames + "/" + s.pilotFrames +
                    (s.ball ? "; the ball on the deck " + s.ball.onDeck + "/" + s.ball.frames + " (|x| to " + s.ball.maxX.toFixed(2) + ", |z| to " + s.ball.maxZ.toFixed(2) +
                        ", y " + s.ball.yMin.toFixed(2) + " to " + s.ball.yMax.toFixed(2) + ")" : "") : "") +
                "; hull world step " + fmtSteps(s.hullStep) +
                "; corrections " + s.corrections + " (" + s.replaySteps + " substeps replayed), inputs sent " + s.inputsSent +
                ", state sequence " + s.sequenceFrom + " to " + s.sequenceTo + ", " + s.stateUpdates + " state updates");
        });
        // each crossing between the deck and the car: the world step at the parent change against the step before it
        crossings.forEach(function (c) {
            var i = c.index;
            if (i >= 2 && i < samples.length && samples[i].parentName === c.to) {
                // my world step at the crossing against the hull's own at the same frame, split into the horizontal and
                // the vertical (a step up onto a floor is a climb, not a pop)
                var d = Vec3.subtract(samples[i].myPos, samples[i - 1].myPos);
                var h = Vec3.subtract(samples[i].pos, samples[i - 1].pos);
                var rel = Vec3.subtract(d, h);
                var before = 1000 * Vec3.distance(samples[i - 1].myPos, samples[i - 2].myPos);
                var after = i + 1 < samples.length ? 1000 * Vec3.distance(samples[i + 1].myPos, samples[i].myPos) : -1;
                c.stepMm = 1000 * Vec3.length(d); c.hullStepMm = 1000 * Vec3.length(h); c.relativeHorizontalMm = 1000 * Math.sqrt(rel.x * rel.x + rel.z * rel.z);
                c.relativeVerticalMm = 1000 * rel.y; c.stepBeforeMm = before; c.stepAfterMm = after;
                log("crossing " + c.from + " to " + c.to + " in " + c.phase + " at " + c.t.toFixed(2) + " s: world step " + c.stepMm.toFixed(1) +
                    " mm (the hull's " + c.hullStepMm.toFixed(1) + "); relative to the hull " + c.relativeHorizontalMm.toFixed(1) + " mm across and " +
                    c.relativeVerticalMm.toFixed(1) + " mm up; " + before.toFixed(1) + " mm before and " + (after >= 0 ? after.toFixed(1) : "n/a") + " mm after");
            }
        });
        if (car) {
            log("the ride: " + carRide.boardings + " boardings, " + carRide.stops + " stops seen, " + carRide.failures + " failures");
            if (probes.length) {
                var dzs = probes.map(function (p) { return p.minDz; }), ys = [], through = 0, onShip = 0, total = 0;
                probes.forEach(function (p) { ys.push(p.minDy, p.maxDy); if (p.boardedThroughRail) { through++; } onShip += p.parents.ship || 0;
                                              Object.keys(p.parents).forEach(function (k) { total += p.parents[k]; }); });
                log("rail probe: " + probes.length + " pushes into the car's end rail from the deck at the bow station; closest to the car's centre " +
                    Math.min.apply(null, dzs).toFixed(3) + " to " + Math.max.apply(null, dzs).toFixed(3) + " m (the rail's far face at " +
                    (0.5 * Airship.CAR.dimensions.z).toFixed(2) + " m); the ship my parent in " + onShip + " of " + total + " frames; local y " +
                    Math.min.apply(null, ys).toFixed(3) + " to " + Math.max.apply(null, ys).toFixed(3) + "; through the rail " + through);
            }
        }
        var last = samples.length ? samples[samples.length - 1].stats : {};
        log("totals: " + samples.length + " samples; corrections " + (last.corrections || 0) + ", replay substeps " + (last.replaySteps || 0) +
            ", max error " + (1000 * (last.maxError || 0)).toFixed(0) + " mm, inputs sent " + (last.inputsSent || 0) + ", received " + (last.inputsReceived || 0) +
            ", dropped " + (last.inputsDropped || 0) + ", starved " + (last.inputsStarved || 0) + ", vehicles " + (last.vehicles || 0) +
            "; stream: states received " + (last.streamStatesReceived || 0) + ", extrapolated substeps " + (last.streamExtrapolations || 0) +
            ", rtt " + (last.rttMs || 0).toFixed(1) + " ms, delay " + (last.delayMs || 0).toFixed(1) + " ms, interval " + (last.streamIntervalMs || 0).toFixed(1) +
            " ms, jitter " + (last.streamJitterMs || 0).toFixed(1) + " ms" +
            "; input buffer: margin " + (last.bufferMargin || 0) + ", lead shifts " + (last.leadShifts || 0) + " (jumps " + (last.leadJumps || 0) +
            "), late " + (last.inputsLate || 0) + ", early " + (last.inputsEarly || 0) +
            "; crossings " + crossings.length);
        Test.saveObject({ role: ROLE, summary: summary, events: events, crossings: crossings, probes: probes, samples: samples }, "physics-airship.json");
        log("done (" + reason + "), " + samples.length + " samples");
        if (ROLE === "pilot" && hull) {
            Rig.deleteAll(LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE);
        }
        Script.setTimeout(function () { Test.quit(); }, 1500);
    }

    function onUpdate(dt) {
        if (now() > TIMEOUT + (courseT0 >= 0 ? courseT0 : 0)) { finish("timeout"); return; }

        if (hull && !sample(dt)) { finish("airship gone"); return; }
        if (ROLE === "pilot") { drivePilot(); } else { driveCrew(); }
    }

    Script.update.connect(onUpdate);
    Script.scriptEnding.connect(function () { if (atHelm) { leaveHelm(); } });
    log("started as " + ROLE + (TERRAIN_URL ? ", terrain " + TERRAIN_URL : ""));
}());
