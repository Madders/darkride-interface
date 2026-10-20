//
//  aircraft-rig.js
//  tests-manual/physics-baseline
//
//  The tilt-thrust aircraft as entities (PHYSICS.md round 8): a flat fuselage that is the body the physics flies (the
//  vehicle group with type "aircraft": 20 kN of thrust tilting from level to vertical, a 24 m^2 wing of 11 m span, a
//  tail 4.5 m aft) and a moving reference frame (frame true) whose top is the floor the pilot stands on at the stick;
//  a wing, a fin and a tailplane for the look (collisionless children), a low rail round the floor in the craft's own
//  world. The same interface as airship-rig.js's Airship: create, deleteAll, HELM, SPEC, PART_NAMES.
//
//  Distributed under the Apache License, Version 2.0.
//

var Aircraft = (function () {
    var BODY = { x: 2.4, y: 0.4, z: 5.0 };          // the fuselage: its top is the floor
    var BODY_CENTER_ABOVE_GROUND = 0.2;             // resting on its belly
    var HELM = { name: "stick", position: [0, 0.2, -1.5], yaw: 0, driver: true };
    var SPEC = {
        type: "aircraft",
        mass: 1200,
        centerOfMass: { x: 0, y: -0.1, z: 0 },
        seats: JSON.stringify([HELM]),
        maxThrust: 20000,
        thrustPoint: { x: 0, y: 0.2, z: 0.5 },
        wingArea: 24,                               // flies at 30 m/s with a lift coefficient of 0.9
        wingSpan: 11,
        wingPoint: { x: 0, y: 0.2, z: 0.3 },
        elevatorArea: 4,
        rudderArea: 2,
        finPoint: { x: 0, y: 0.5, z: 4.5 },
        maxFinAngle: 30,
        dragArea: { x: 3, y: 8, z: 0.8 },
        angularDragArea: { x: 800, y: 800, z: 600 },
        tilt: 1
    };
    var PART_NAMES = ["aircraft-body", "aircraft-wing", "aircraft-fin", "aircraft-rail", "aircraft-helm"];

    function part(body, name, properties, lifetime) {
        var p = { type: "Box", name: name, parentID: body, lifetime: lifetime || -1, dynamic: false, collisionless: true, grab: { grabbable: false } };
        for (var key in properties) { if (properties.hasOwnProperty(key)) { p[key] = properties[key]; } }
        return Entities.addEntity(p);
    }

    // The craft on its skids at (x, z), facing yaw degrees; returns { hull, parts }.
    function create(x, z, yawDegrees, lifetime, extra) {
        var y = LandRover.groundHeight(x, z) + BODY_CENTER_ABOVE_GROUND + 0.05;
        var rotation = Quat.fromPitchYawRollDegrees(0, yawDegrees || 0, 0);
        var properties = {
            type: "Box", name: "aircraft-body", position: { x: x, y: y, z: z }, rotation: rotation, dimensions: BODY,
            color: { red: 200, green: 200, blue: 210 }, dynamic: true, collisionless: false,
            gravity: { x: 0, y: -9.8, z: 0 }, damping: 0.0, angularDamping: 0.05, friction: 0.8, restitution: 0.0,
            lifetime: lifetime || -1, frame: true, grab: { grabbable: false }, vehicle: SPEC
        };
        if (extra) { for (var key in extra) { if (extra.hasOwnProperty(key)) { properties[key] = extra[key]; } } }
        var hull = Entities.addEntity(properties);
        var parts = [];
        parts.push(part(hull, "aircraft-wing", { localPosition: { x: 0, y: 0.25, z: 0.3 }, dimensions: { x: 11, y: 0.1, z: 2.2 }, color: { red: 180, green: 40, blue: 40 } }, lifetime));
        parts.push(part(hull, "aircraft-fin", { localPosition: { x: 0, y: 0.9, z: 2.3 }, dimensions: { x: 0.1, y: 1.4, z: 1.0 }, color: { red: 180, green: 40, blue: 40 } }, lifetime));
        parts.push(part(hull, "aircraft-fin", { localPosition: { x: 0, y: 0.5, z: 2.3 }, dimensions: { x: 3.0, y: 0.1, z: 0.8 }, color: { red: 180, green: 40, blue: 40 } }, lifetime));
        var rails = [
            { pos: { x: 1.15, y: 0.6, z: 0 }, dim: { x: 0.1, y: 0.8, z: 5.0 } },
            { pos: { x: -1.15, y: 0.6, z: 0 }, dim: { x: 0.1, y: 0.8, z: 5.0 } },
            { pos: { x: 0, y: 0.6, z: -2.45 }, dim: { x: 2.4, y: 0.8, z: 0.1 } },
            { pos: { x: 0, y: 0.6, z: 2.45 }, dim: { x: 2.4, y: 0.8, z: 0.1 } }
        ];
        rails.forEach(function (r) {
            parts.push(part(hull, "aircraft-rail", { localPosition: r.pos, dimensions: r.dim, color: { red: 80, green: 80, blue: 90 }, collisionless: false }, lifetime));
        });
        parts.push(part(hull, "aircraft-helm", { localPosition: { x: 0, y: 0.7, z: -2.3 }, dimensions: { x: 0.3, y: 1.0, z: 0.3 }, color: { red: 60, green: 60, blue: 70 }, collisionless: false }, lifetime));
        return { hull: hull, parts: parts };
    }

    function deleteAll(center, radius) {
        var removed = 0;
        PART_NAMES.forEach(function (name) {
            Entities.findEntitiesByName(name, center, radius, false).forEach(function (id) { Entities.deleteEntity(id); removed++; });
        });
        return removed;
    }

    return { BODY: BODY, DECK: BODY, DECK_CENTER_ABOVE_GROUND: BODY_CENTER_ABOVE_GROUND, HELM: HELM, SPEC: SPEC, PART_NAMES: PART_NAMES,
             HULL_NAME: "aircraft-body", create: create, deleteAll: deleteAll };
}());
