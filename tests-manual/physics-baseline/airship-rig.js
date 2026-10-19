//
//  airship-rig.js
//  tests-manual/physics-baseline
//
//  A small airship (PHYSICS.md round 7) as entities: a Box deck that is the hull the physics flies (the vehicle group with
//  type "airship": the envelope, the propellers, the fins, the ballast) and a moving reference frame (frame true) whose top
//  the crew walks on; an envelope, fins and rails as its children (the envelope and fins for the look, collisionless; the
//  rails and the helm post collidable, so they stand in the ship's own world and the crew bumps into them). Script.include
//  this after landrover.js (it uses the terrain); it defines Airship.
//
//  The numbers: a 36 m envelope of 1,500 m3 of helium (about 1,570 kg of lift at sea level, the gas's weight taken off),
//  a 1,350 kg hull with 250 kg of ballast aboard, so it sits 30 kg heavy on the ground until the pilot drops ballast; two
//  propellers of 4 kN together (about 23 m/s flat out against the envelope's drag), a 3 kN vertical thruster, fins of
//  12 m2 each 17 m aft of the centre, 450 m2 of drag area across the hull (what lets the velocity follow the nose in a
//  turn: the first flight with 160 m2 turned at a degree a second, with 300 at five). A ship of this size in life: a
//  Goodyear blimp is four times the volume.
//
//  Distributed under the Apache License, Version 2.0.
//

var Airship = (function () {
    var DECK = { x: 4, y: 0.4, z: 14 };            // the hull entity: its top is the deck
    var DECK_CENTER_ABOVE_GROUND = 0.2;            // the box's centre, landed
    var ENVELOPE = { x: 9, y: 9, z: 36 };
    var ENVELOPE_CENTER = { x: 0, y: 7, z: 0 };
    var FIN_POINT = { x: 0, y: 7, z: 17 };
    var HELM = { name: "helm", position: [0, 0.2, -4.6], yaw: 0, driver: true };
    var CENTER_OF_MASS = { x: 0, y: -0.3, z: 0 };

    var SPEC = {
        type: "airship",
        mass: 1350,
        centerOfMass: CENTER_OF_MASS,
        seats: JSON.stringify([HELM]),
        envelopeVolume: 1500,
        centerOfBuoyancy: ENVELOPE_CENTER,
        gasDensity: 0.1786,
        maxThrust: 4000,
        maxVerticalThrust: 3000,
        thrustPoint: { x: 0, y: 0.5, z: 2 },
        dragArea: { x: 450, y: 300, z: 12 },
        angularDragArea: { x: 400000, y: 400000, z: 20000 },
        rudderArea: 12,
        elevatorArea: 12,
        finPoint: FIN_POINT,
        maxFinAngle: 25,
        ballastRate: 20,
        ventRate: 0.01,
        ballast: 250,
        fill: 1
    };

    var PART_NAMES = ["airship-hull", "airship-envelope", "airship-fin", "airship-rail", "airship-helm", "airship-ball",
                      "airship-track", "airship-car", "airship-car-rail"];
    // The deck trolley (round 8, nesting): a straight Track along the starboard half of the deck, a child of the hull,
    // with two stations, and a flat car on it that is a frame of its own and the track's child: its pose is the
    // track's, the track's is the ship's. A rider steps on from the deck at a station, rides along the deck and steps
    // off again while the ship flies.
    var CAR = {
        x: 1.0,                                  // the track's line, starboard of the centre line
        stations: [-2.0, 5.0],                   // z of the bow and stern stations on the deck
        dimensions: { x: 1.6, y: 0.3, z: 2.2 },
        speed: 1.5, acceleration: 1.0, dwell: 4.0
    };

    function part(hull, name, properties, lifetime) {
        var p = {
            type: "Box", name: name, parentID: hull, lifetime: lifetime || -1,
            dynamic: false, collisionless: true, grab: { grabbable: false }
        };
        for (var key in properties) {
            if (properties.hasOwnProperty(key)) {
                p[key] = properties[key];
            }
        }
        return Entities.addEntity(p);
    }

    // The ship landed on the ground at (x, z), facing yaw degrees; returns { hull, parts: [ids] }.
    function create(x, z, yawDegrees, lifetime, extra) {
        var y = LandRover.groundHeight(x, z) + DECK_CENTER_ABOVE_GROUND + 0.05;
        var rotation = Quat.fromPitchYawRollDegrees(0, yawDegrees || 0, 0);
        var properties = {
            type: "Box",
            name: "airship-hull",
            position: { x: x, y: y, z: z },
            rotation: rotation,
            dimensions: DECK,
            color: { red: 120, green: 90, blue: 50 },
            dynamic: true,
            collisionless: false,
            gravity: { x: 0, y: -9.8, z: 0 },
            damping: 0.0,
            angularDamping: 0.05,
            friction: 0.6,
            restitution: 0.0,
            lifetime: lifetime || -1,
            frame: true,
            grab: { grabbable: false },
            vehicle: SPEC
        };
        if (extra) {
            for (var key in extra) {
                if (extra.hasOwnProperty(key)) {
                    properties[key] = extra[key];
                }
            }
        }
        var hull = Entities.addEntity(properties);
        var parts = [];
        // the envelope and the fins: the look of it
        parts.push(Entities.addEntity({
            type: "Shape", shape: "Sphere", name: "airship-envelope", parentID: hull, lifetime: lifetime || -1,
            localPosition: ENVELOPE_CENTER, dimensions: ENVELOPE, color: { red: 220, green: 220, blue: 210 },
            dynamic: false, collisionless: true, grab: { grabbable: false }
        }));
        parts.push(part(hull, "airship-fin", { localPosition: FIN_POINT, dimensions: { x: 0.2, y: 6, z: 5 }, color: { red: 180, green: 40, blue: 40 } }, lifetime));
        parts.push(part(hull, "airship-fin", { localPosition: FIN_POINT, dimensions: { x: 6, y: 0.2, z: 5 }, color: { red: 180, green: 40, blue: 40 } }, lifetime));
        // the rails and the helm post: in the ship's own world, where the crew is
        var rails = [
            { pos: { x: 1.95, y: 0.7, z: 0 }, dim: { x: 0.1, y: 1.0, z: 14 } },
            { pos: { x: -1.95, y: 0.7, z: 0 }, dim: { x: 0.1, y: 1.0, z: 14 } },
            { pos: { x: 0, y: 0.7, z: 6.95 }, dim: { x: 4, y: 1.0, z: 0.1 } },
            { pos: { x: 0, y: 0.7, z: -6.95 }, dim: { x: 4, y: 1.0, z: 0.1 } }
        ];
        rails.forEach(function (r) {
            parts.push(part(hull, "airship-rail", { localPosition: r.pos, dimensions: r.dim, color: { red: 80, green: 60, blue: 30 }, collisionless: false }, lifetime));
        });
        parts.push(part(hull, "airship-helm", { localPosition: { x: 0, y: 0.75, z: -5.6 }, dimensions: { x: 0.3, y: 1.1, z: 0.3 },
            color: { red: 60, green: 40, blue: 20 }, collisionless: false }, lifetime));
        return { hull: hull, parts: parts };
    }

    function createDeckTrack(hull, lifetime) {
        var mid = 0.5 * (CAR.stations[0] + CAR.stations[1]);
        var half = 0.5 * (CAR.stations[1] - CAR.stations[0]);
        var trackY = 0.5 * DECK.y + 0.5 * CAR.dimensions.y;   // the car's centre: its underside on the deck's top
        var track = Entities.addEntity({
            type: "Track", name: "airship-track", parentID: hull, lifetime: lifetime || -1,
            localPosition: { x: CAR.x, y: trackY, z: mid }, localRotation: Quat.IDENTITY,
            dimensions: { x: 2, y: 1, z: 2 * half + 2 },
            trackPoints: [{ x: 0, y: 0, z: -half }, { x: 0, y: 0, z: 0 }, { x: 0, y: 0, z: half }],
            trackBank: [0, 0, 0], trackDwell: [CAR.dwell, 0, CAR.dwell], trackClosed: false,
            trackSpeed: CAR.speed, trackAcceleration: CAR.acceleration
        });
        var car = Entities.addEntity({
            type: "Box", name: "airship-car", shapeType: "box", parentID: track, lifetime: lifetime || -1,
            localPosition: { x: 0, y: 0, z: half }, localRotation: Quat.IDENTITY,   // at the stern station
            dimensions: CAR.dimensions, color: { red: 200, green: 160, blue: 60 },
            dynamic: false, collisionless: false, damping: 0, angularDamping: 0, friction: 0.6, restitution: 0,
            grab: { grabbable: false },
            frame: true, trackID: track, trackOffset: 0
        });
        // A rail across each end, both sides open to the deck for boarding (the car turns round at each end of its open
        // track, so a side rail would face the deck half the time: run 6 of 10-14). The car's children stand in the
        // car's own world; the car's body in the ship's world wears a copy of them (round 8, PHYSICS.md 4.1), so a
        // rider walking in from the deck meets an end rail as a wall and boards by the side. (Without the copy, runs 3
        // and 4 of 10-13: the rider stood in a rail's space and was shoved out when the car's world received them.)
        var railY = 0.5 * CAR.dimensions.y + 0.3, railH = 0.6, railT = 0.1;
        var halfZ = 0.5 * CAR.dimensions.z;
        var rails = [
            { pos: { x: 0, y: railY, z: halfZ - 0.5 * railT }, dim: { x: CAR.dimensions.x, y: railH, z: railT } },
            { pos: { x: 0, y: railY, z: -(halfZ - 0.5 * railT) }, dim: { x: CAR.dimensions.x, y: railH, z: railT } }
        ];
        var parts = [];
        rails.forEach(function (r) {
            parts.push(part(car, "airship-car-rail", { localPosition: r.pos, dimensions: r.dim, color: { red: 120, green: 90, blue: 30 }, collisionless: false }, lifetime));
        });
        return { track: track, car: car, parts: parts };
    }

    function deleteAll(center, radius) {
        var removed = 0;
        PART_NAMES.forEach(function (name) {
            Entities.findEntitiesByName(name, center, radius, false).forEach(function (id) { Entities.deleteEntity(id); removed++; });
        });
        return removed;
    }

    return {
        DECK: DECK,
        DECK_CENTER_ABOVE_GROUND: DECK_CENTER_ABOVE_GROUND,
        HELM: HELM,
        SPEC: SPEC,
        PART_NAMES: PART_NAMES,
        CAR: CAR,
        create: create,
        createDeckTrack: createDeckTrack,
        deleteAll: deleteAll
    };
}());
