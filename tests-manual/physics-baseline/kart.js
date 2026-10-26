//
//  kart.js
//  tests-manual/physics-baseline
//
//  The go-kart (kart-rig.js) driven through Entities.setVehicleControls on world.json's flat ground, serverless: the
//  avatar sits in its seat as vehicles.js would (parented to the chassis, collisions off, the driver). Phases, read every
//  frame from the chassis: full throttle (top speed, the time to 10 m/s), full brake (the stopping distance), full lock at
//  2 m/s, as indoors (the turning radius from speed over yaw rate, the most body roll), full reverse (its
//  speed). The ground's friction is set to 0.9, the test track's: on 0.5 the kart slid at about 0.5 g. Prints KART
//  lines; samples in kart.json.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var SIT_ANIMATION_URL = "https://s3-us-west-1.amazonaws.com/hifi-content/clement/production/animations/sitting_idle.fbx";
    var LIFETIME = 300;
    var samples = [];
    var kart = null;
    var failures = 0;

    function log(s) { print("KART " + s); }
    function after(seconds, f) { Script.setTimeout(f, seconds * 1000); }
    function check(ok, text) {
        if (!ok) {
            failures++;
        }
        log((ok ? "PASS " : "FAIL ") + text);
    }

    function sit() {
        var seat = JSON.parse(Kart.SPEC.seats)[0];
        MyAvatar.collisionsEnabled = false;
        MyAvatar.getAnimationRoles().filter(function (role) {
            return !(role.indexOf("right") === 0 || role.indexOf("left") === 0);
        }).forEach(function (role) { MyAvatar.overrideRoleAnimation(role, SIT_ANIMATION_URL, 30, true, 1, 350); });
        MyAvatar.parentID = kart.chassis;
        MyAvatar.parentJointIndex = 65535;
        MyAvatar.localPosition = { x: seat.position[0], y: seat.position[1], z: seat.position[2] };
        MyAvatar.localOrientation = Quat.IDENTITY;
        Entities.editEntity(kart.chassis, { vehicle: { driver: MyAvatar.sessionUUID } });
    }

    // drives with the controls for the seconds (or until done(sample) says so), recording; then(list)
    function drive(name, controls, seconds, then, done) {
        var list = [];
        var t0 = Date.now();
        var previous = null;
        function tick() {
            var t = (Date.now() - t0) / 1000;
            var p = Entities.getEntityProperties(kart.chassis, ["position", "rotation", "velocity"]);
            Entities.setVehicleControls(kart.chassis, typeof controls === "function" ?
                controls(Vec3.dot(p.velocity, Quat.getFront(p.rotation))) : controls);
            var front = Quat.getFront(p.rotation);
            var up = Quat.getUp(p.rotation);
            var yaw = Math.atan2(-front.x, -front.z);
            var s = { phase: name, t: t, position: p.position, speed: Vec3.dot(p.velocity, front),
                      roll: Math.acos(Math.max(-1, Math.min(1, up.y))) * 180 / Math.PI, yaw: yaw, yawRate: 0 };
            if (previous && t > previous.t) {
                var dYaw = yaw - previous.yaw;
                dYaw = Math.atan2(Math.sin(dYaw), Math.cos(dYaw));
                s.yawRate = dYaw / (t - previous.t);
            }
            previous = s;
            list.push(s);
            samples.push(s);
            if (t >= seconds || (done && done(s))) {
                Script.update.disconnect(tick);
                then(list);
            }
        }
        Script.update.connect(tick);
    }

    function course() {
        drive("throttle", { throttle: 1, steering: 0, brake: 0, handBrake: 0 }, 6, function (list) {
            var top = 0, tenAt = null;
            list.forEach(function (s) {
                top = Math.max(top, s.speed);
                if (tenAt === null && s.speed >= 10) {
                    tenAt = s.t;
                }
            });
            check(top >= 10 && top <= 15, "full throttle: top speed " + top.toFixed(2) + " m/s, 10 m/s after " +
                  (tenAt === null ? "never" : tenAt.toFixed(2) + " s") + " (10 to 15 m/s)");
            var from = Entities.getEntityProperties(kart.chassis, ["position"]).position;
            drive("brake", { throttle: 0, steering: 0, brake: 1, handBrake: 0 }, 5, function (list) {
                var last = list[list.length - 1];
                var distance = Vec3.distance(from, last.position);
                check(last.speed < 0.3 && distance < 14, "full brake: from " + list[0].speed.toFixed(2) + " m/s, stopped in " +
                      distance.toFixed(2) + " m and " + last.t.toFixed(2) + " s");
                // full lock with the speed held at 2 m/s, as in a room: a little throttle under it, coasting over it
                drive("full lock", function (speed) {
                    return { throttle: speed < 2.0 ? 0.3 : 0, steering: 1, brake: 0, handBrake: 0 };
                }, 7, function (list) {
                    var turning = list.filter(function (s) { return s.t > 3 && Math.abs(s.yawRate) > 0.05; });
                    var radius = 0, speed = 0, roll = 0;
                    turning.forEach(function (s) { radius += Math.abs(s.speed / s.yawRate); speed += s.speed; });
                    list.forEach(function (s) { roll = Math.max(roll, s.roll); });
                    radius /= Math.max(1, turning.length);
                    speed /= Math.max(1, turning.length);
                    check(turning.length > 0 && radius < 2.0, "full lock at 2 m/s: turning radius " + radius.toFixed(2) +
                          " m at " + speed.toFixed(2) + " m/s (under 2 m; the rover's is 3.6 m)");
                    check(roll < 25, "full lock: most body roll " + roll.toFixed(1) + " degrees (under 25: it doesn't tip)");
                    drive("stop", { throttle: 0, steering: 0, brake: 1, handBrake: 1 }, 2, function () {
                        drive("reverse", { throttle: -1, steering: 0, brake: 0, handBrake: 0 }, 4, function (list) {
                            var back = 0;
                            list.forEach(function (s) { back = Math.min(back, s.speed); });
                            check(back < -2, "full reverse: " + back.toFixed(2) + " m/s");
                            finish();
                        });
                    });
                });
            }, function (s) { return s.t > 0.5 && s.speed < 0.05; });
        });
    }

    function finish() {
        Entities.setVehicleControls(kart.chassis, { throttle: 0, steering: 0, brake: 1, handBrake: 1 });
        Test.saveObject({ failures: failures, samples: samples }, "kart.json");
        log(failures === 0 ? "ALL PASS" : failures + " FAILED");
        after(1, function () { Test.quit(); });
    }

    function start(tries) {
        if (!Window.isPhysicsEnabled()) {
            if (tries < 120) {
                after(1, function () { start(tries + 1); });
            } else {
                log("FAIL no physics after 120 s");
                Test.quit();
            }
            return;
        }
        // the ground's grip as the test track's terrain (0.9) and the camera world's floor; world.json's is 0.5
        Entities.findEntitiesByName("Ground", { x: 0, y: 0, z: 0 }, 50, true).forEach(function (id) {
            Entities.editEntity(id, { friction: 0.9 });
        });
        kart = Kart.create(0, 0, -5, 0, LIFETIME);
        MyAvatar.position = { x: 2, y: 1, z: -5 };
        after(3, function () {
            sit();
            after(1.5, course);
        });
    }

    after(2, function () { start(0); });
}());
