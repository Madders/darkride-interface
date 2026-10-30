//
//  kart-rig.js
//  tests-manual/physics-baseline
//
//  The go-kart (Caity, 2026-10-25: "small as possible while counting as a vehicle ... mario kart"), as entities: a Box
//  chassis with the vehicle group and four Cylinder wheels as children, one seat. Small enough for a 1.2 m corridor and
//  a 1.4 m door (0.86 m over the wheels, 1.45 m long), a 1.0 m wheelbase and 50 degrees of steering (about 1.5 m round
//  at walking pace; the Land Rover's is 3.6 m), one gear, four-wheel drive, about 16.8 m/s at the top of its revs, and
//  its centre of mass under the axles so a sharp turn doesn't tip it. The test track's kart pad and the Create menu's
//  go-kart use these numbers. Script.include this; it defines Kart.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

var Kart = (function () {
    var WHEEL_RADIUS = 0.14, WHEEL_WIDTH = 0.12, WHEELBASE = 1.0, TRACK = 0.74, TRAVEL = 0.06;
    var CHASSIS = { x: 0.7, y: 0.16, z: 1.45 };
    var CHASSIS_ABOVE_GROUND = 0.2;   // m, the chassis's centre at rest
    var WHEEL_TOP = (WHEEL_RADIUS + TRAVEL * 0.5) - CHASSIS_ABOVE_GROUND;

    function wheels() {
        var list = [];
        var names = [["FL", -1, -1], ["FR", 1, -1], ["RL", -1, 1], ["RR", 1, 1]];
        for (var i = 0; i < names.length; i++) {
            var front = names[i][2] < 0;
            list.push({ name: names[i][0], center: [names[i][1] * TRACK * 0.5, WHEEL_TOP, names[i][2] * WHEELBASE * 0.5],
                        radius: WHEEL_RADIUS, width: WHEEL_WIDTH, mass: 3, suspensionDistance: TRAVEL, targetPosition: 0.5,
                        steer: front ? 1 : 0, brake: 1, handBrake: front ? 0 : 1 });
        }
        return list;
    }

    var SEATS = [{ name: "driver", position: [0, 0.06, 0.12], yaw: 0, driver: true }];

    // the vehicle group, "kart 2" (Caity, 10-25: "punchier and make tighter turns, accelerate faster, go faster"): 150 kg;
    // 100 N m at the engine through 2.8 to all four wheels, 1,000 to 3,200 rpm (about 16.8 m/s), a third of that in
    // reverse; 50 degrees of lock; 250 N m brakes on every wheel and a 250 N m hand brake on the rear; a stiff, short
    // suspension (6 cm, 3.5 Hz). The first kart (60 N m through 3.66 to the rear wheels, 3,000 rpm, 45 degrees) took
    // 4.4 s to 10 m/s and topped out at 11.7: its launch was the two rear wheels spinning (the tyres' grip is their
    // curve times the ground's friction, Jolt's default combine), its top speed the rev limit.
    // A small engine needs the torque: the engine's own friction (Jolt's default inertia 0.5 kg m^2 and damping 0.2)
    // costs 0.1 N m per rad/s of its speed, 34 N m at 3,200 rpm; 14 N m at 6,500 rpm (the first try) never got past
    // 1,500 rpm, 3 m/s.
    var SPEC = {
        type: "wheeled", mass: 150, centerOfMass: { x: 0, y: -0.12, z: 0 },
        wheels: JSON.stringify(wheels()), seats: JSON.stringify(SEATS),
        maxSteerAngle: 50, maxTorque: 100, minRPM: 1000, maxRPM: 3200,
        gearRatios: JSON.stringify([1.0]), reverseGearRatio: 3.0, finalDriveRatio: 2.8,
        maxBrakeTorque: 250, maxHandBrakeTorque: 250, driveType: "all",
        suspensionFrequency: 3.5, suspensionDamping: 0.7
    };

    // a kart standing on the ground at height groundY, facing yawDegrees (0 is -z); made static and of vehicle type
    // "none", and a vehicle 1.5 s later, so whoever builds its model sees the whole entity (as the vehicle pads do)
    function create(x, groundY, z, yawDegrees, lifetime) {
        var spec = JSON.parse(JSON.stringify(SPEC));
        spec.type = "none";
        var chassis = Entities.addEntity({
            type: "Box", shape: "Cube", name: "kart-chassis",
            position: { x: x, y: groundY + CHASSIS_ABOVE_GROUND + 0.05, z: z },
            rotation: Quat.fromPitchYawRollDegrees(0, yawDegrees || 0, 0), dimensions: CHASSIS,
            color: { red: 220, green: 40, blue: 40 }, dynamic: false, collisionless: false, gravity: { x: 0, y: -9.8, z: 0 },
            damping: 0.02, angularDamping: 0.3, friction: 0.6, restitution: 0.0, lifetime: lifetime || -1,
            grab: { grabbable: false }, vehicle: spec
        });
        var wheelIDs = wheels().map(function (wheel) {
            return Entities.addEntity({
                type: "Shape", shape: "Cylinder", name: "kart-wheel-" + wheel.name, parentID: chassis,
                localPosition: { x: wheel.center[0], y: wheel.center[1] - wheel.suspensionDistance * 0.5, z: wheel.center[2] },
                localRotation: Quat.fromPitchYawRollDegrees(0, 0, 90),
                dimensions: { x: 2 * wheel.radius, y: wheel.width, z: 2 * wheel.radius },
                color: { red: 16, green: 16, blue: 22 }, dynamic: false, collisionless: true, lifetime: lifetime || -1,
                vehicle: { type: "wheel", wheel: wheel.name, axle: "y" }
            });
        });
        Script.setTimeout(function () {
            Entities.editEntity(chassis, { dynamic: true, vehicle: { type: SPEC.type } });
        }, 1500);
        return { chassis: chassis, wheels: wheelIDs };
    }

    return {
        SPEC: SPEC,
        CHASSIS: CHASSIS,
        CHASSIS_ABOVE_GROUND: CHASSIS_ABOVE_GROUND,
        wheels: wheels,
        create: create
    };
}());
