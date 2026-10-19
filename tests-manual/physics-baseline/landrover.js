//
//  landrover.js
//  tests-manual/physics-baseline
//
//  The Land Rover Series III 88" from its spec sheet (PHYSICS.md round 6), as entities: a box chassis with the vehicle
//  group (mass, centre of mass, four wheels at the wheelbase and track, the 2.25 litre petrol engine, the four-speed box
//  with the high-range transfer and the 4.7 axle, seats), four Cylinder wheels that follow the wheels, and the terrain
//  the test drives on. Script.include this; it defines LandRover.
//
//  The numbers (an 88 inch Series III, 2.25 petrol, as the workshop manual gives them; correct here if a number is off):
//  kerb weight 1,320 kg, wheelbase 2.235 m, track 1.308 m, 6.00-16 tyres (0.37 m radius), 70 bhp at 4,000 rpm and
//  163 N m at 1,500 to 2,000 rpm, gears 3.68 2.22 1.50 1.00 and reverse 4.02, transfer box high 1.148, axle 4.7, rear
//  drive in high range, leaf springs with about 0.18 m of travel. Top speed about 100 km/h.
//
//  Distributed under the Apache License, Version 2.0.
//

var LandRover = (function () {
    var TERRAIN_CENTER = { x: 400, y: 0, z: 400 };
    var TERRAIN_SIZE = 192;        // m, square (make-terrain.py is given the same)
    var TERRAIN_HEIGHT = 32;       // m
    var TERRAIN_VOXEL = 0.5;       // m
    var TERRAIN_BASE = 8.0;        // m of ground above the box's bottom at the start pad

    // the ground's height above the box's bottom: the same function as make-terrain.py
    function groundAboveBottom(localX, localZ) {
        var cx = localX - TERRAIN_SIZE * 0.5;
        var cz = localZ - TERRAIN_SIZE * 0.5;
        var hills = 2.5 * Math.sin(cx / 9.0) * Math.cos(cz / 11.0) + 1.5 * Math.sin((cx + cz) / 7.0);
        var r = Math.sqrt(cx * cx + cz * cz);
        var pad = 14.0, blend = 6.0;
        if (r < pad) {
            return TERRAIN_BASE;
        }
        if (r < pad + blend) {
            var t = (r - pad) / blend;
            t = t * t * (3 - 2 * t);
            return TERRAIN_BASE + t * hills;
        }
        return TERRAIN_BASE + hills;
    }

    // the ground's world height at world (x, z)
    function groundHeight(x, z) {
        var localX = x - (TERRAIN_CENTER.x - TERRAIN_SIZE * 0.5);
        var localZ = z - (TERRAIN_CENTER.z - TERRAIN_SIZE * 0.5);
        return TERRAIN_CENTER.y - TERRAIN_HEIGHT * 0.5 + groundAboveBottom(localX, localZ);
    }

    // the spec sheet, in the vehicle group's terms
    var WHEEL_RADIUS = 0.37;
    var WHEEL_WIDTH = 0.16;
    var WHEELBASE = 2.235;
    var TRACK = 1.308;
    var TRAVEL = 0.18;
    var CHASSIS = { x: 1.68, y: 1.1, z: 3.62 };   // the body without the roof, a box
    var CHASSIS_ABOVE_GROUND = 1.0;               // the box's centre, when standing
    var WHEEL_TOP = (WHEEL_RADIUS + TRAVEL * 0.5) - CHASSIS_ABOVE_GROUND;   // the suspension's top, relative to the centre

    function wheels() {
        var list = [];
        var names = [["FL", -1, -1], ["FR", 1, -1], ["RL", -1, 1], ["RR", 1, 1]];
        for (var i = 0; i < names.length; i++) {
            var front = names[i][2] < 0;
            list.push({
                name: names[i][0],
                center: [names[i][1] * TRACK * 0.5, WHEEL_TOP, names[i][2] * WHEELBASE * 0.5],
                radius: WHEEL_RADIUS,
                width: WHEEL_WIDTH,
                mass: 25,
                suspensionDistance: TRAVEL,
                targetPosition: 0.5,
                steer: front ? 1 : 0,
                brake: 1,
                handBrake: front ? 0 : 1
            });
        }
        return list;
    }

    var SEATS = [
        { name: "driver", position: [0.42, -0.05, -0.35], yaw: 0, driver: true },      // right-hand drive
        { name: "passenger", position: [-0.42, -0.05, -0.35], yaw: 0 },
        { name: "rear left", position: [-0.42, -0.05, 0.75], yaw: 0 },
        { name: "rear right", position: [0.42, -0.05, 0.75], yaw: 0 }
    ];

    var SPEC = {
        type: "wheeled",
        mass: 1320,
        centerOfMass: { x: 0, y: -0.3, z: 0 },
        wheels: JSON.stringify(wheels()),
        seats: JSON.stringify(SEATS),
        maxSteerAngle: 32,
        maxTorque: 163,
        minRPM: 700,
        maxRPM: 4500,
        gearRatios: JSON.stringify([3.68, 2.22, 1.5, 1.0]),
        reverseGearRatio: 4.02,
        finalDriveRatio: 4.7 * 1.148,
        maxBrakeTorque: 1500,
        maxHandBrakeTorque: 2500,
        driveType: "rear",
        suspensionFrequency: 1.4,
        suspensionDamping: 0.6
    };

    // The terrain entity, unless one by this name exists already; returns its id.
    function createTerrain(url, lifetime) {
        var existing = Entities.findEntitiesByName("rover-terrain", TERRAIN_CENTER, TERRAIN_SIZE, false);
        if (existing.length > 0) {
            return existing[0];
        }
        return Entities.addEntity({
            type: "Terrain",
            name: "rover-terrain",
            position: TERRAIN_CENTER,
            dimensions: { x: TERRAIN_SIZE, y: TERRAIN_HEIGHT, z: TERRAIN_SIZE },
            voxelSize: TERRAIN_VOXEL,
            terrainURL: url,
            terrainVersion: 1,
            terrainLayers: JSON.stringify([{ albedo: "", tiling: 4 }]),
            lifetime: lifetime || -1
        });
    }

    // The Land Rover standing on the ground at (x, z), facing yaw degrees; returns { chassis, wheels: [ids] }.
    function create(x, z, yawDegrees, lifetime, extra) {
        var y = groundHeight(x, z) + CHASSIS_ABOVE_GROUND + 0.05;
        var rotation = Quat.fromPitchYawRollDegrees(0, yawDegrees || 0, 0);
        var properties = {
            type: "Box",
            name: "rover-chassis",
            position: { x: x, y: y, z: z },
            rotation: rotation,
            dimensions: CHASSIS,
            color: { red: 96, green: 128, blue: 80 },
            dynamic: true,
            collisionless: false,
            gravity: { x: 0, y: -9.8, z: 0 },
            damping: 0.02,
            angularDamping: 0.1,
            friction: 0.6,
            restitution: 0.0,
            lifetime: lifetime || -1,
            vehicle: SPEC
        };
        if (extra) {
            for (var key in extra) {
                if (extra.hasOwnProperty(key)) {
                    properties[key] = extra[key];
                }
            }
        }
        var chassis = Entities.addEntity(properties);
        var wheelIDs = [];
        var list = wheels();
        for (var i = 0; i < list.length; i++) {
            var wheel = list[i];
            wheelIDs.push(Entities.addEntity({
                type: "Shape",
                shape: "Cylinder",
                name: "rover-wheel-" + wheel.name,
                parentID: chassis,
                localPosition: { x: wheel.center[0], y: wheel.center[1] - TRAVEL * 0.5, z: wheel.center[2] },
                localRotation: Quat.fromPitchYawRollDegrees(0, 0, 90),
                dimensions: { x: 2 * wheel.radius, y: wheel.width, z: 2 * wheel.radius },
                color: { red: 30, green: 30, blue: 30 },
                dynamic: false,
                collisionless: true,
                lifetime: lifetime || -1,
                vehicle: { type: "wheel", wheel: wheel.name, axle: "y" }
            }));
        }
        return { chassis: chassis, wheels: wheelIDs };
    }

    // The playgrounds (drive-rover, fly-airship, fly-aircraft) share the pad at the terrain's centre. Each clears the
    // other kinds of vehicle off it before it makes or keeps its own: a car left on the pad when an airship is built
    // there rides off in the ship's frame and slides about its deck (Caity's hand test, 10-18). A name starting with
    // keepPrefix is kept; children go with their parents.
    var VEHICLE_PREFIXES = ["rover-chassis", "rover-wheel-", "airship-", "aircraft-"];
    function clearVehicles(keepPrefix) {
        var removed = 0;
        Entities.findEntities(TERRAIN_CENTER, 2 * TERRAIN_SIZE).forEach(function (id) {
            var name = Entities.getEntityProperties(id, ["name"]).name || "";
            if (keepPrefix && name.indexOf(keepPrefix) === 0) {
                return;
            }
            for (var i = 0; i < VEHICLE_PREFIXES.length; i++) {
                if (name.indexOf(VEHICLE_PREFIXES[i]) === 0) {
                    Entities.deleteEntity(id);
                    removed++;
                    return;
                }
            }
        });
        return removed;
    }

    return {
        TERRAIN_CENTER: TERRAIN_CENTER,
        TERRAIN_SIZE: TERRAIN_SIZE,
        TERRAIN_HEIGHT: TERRAIN_HEIGHT,
        TERRAIN_VOXEL: TERRAIN_VOXEL,
        CHASSIS: CHASSIS,
        CHASSIS_ABOVE_GROUND: CHASSIS_ABOVE_GROUND,
        SEATS: SEATS,
        SPEC: SPEC,
        groundHeight: groundHeight,
        createTerrain: createTerrain,
        create: create,
        clearVehicles: clearVehicles
    };
}());
