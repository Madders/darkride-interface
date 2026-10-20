"use strict";
//
//  vehicles.js
//  scripts/system
//
//  Sitting in and driving vehicles (PHYSICS.md round 6). Click a vehicle (an entity with vehicle.type "wheeled", or one of
//  its wheels) within reach to take a seat: the driver's seat if nobody holds it, else the nearest other seat. Seated, the
//  avatar is parented to the vehicle at the seat; the driver's W/S or up/down (throttle, brake), A/D or left/right
//  (steering) and Space (hand brake) go to the vehicle through Entities.setVehicleControls, and a chase camera follows.
//  F stands up.
//
//  An airship (vehicle.type "airship", PHYSICS.md round 7) is a moving reference frame whose deck the crew walks on, so
//  nobody is parented to it by hand: click the ship (or anything that is part of it) within reach to take the helm (the
//  seat marked driver) if nobody holds it, which puts you at the helm standing. At the helm: W/S set the throttle (it
//  stays where you leave it; X cuts it), A/D or left/right steer with the rudder, up/down raise and lower the nose with
//  the elevator, Space and C run the vertical thruster up and down, B drops ballast, V vents gas, F leaves the helm.
//  The camera is your own.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {
    var SIT_ANIMATION_URL = "https://s3-us-west-1.amazonaws.com/hifi-content/clement/production/animations/sitting_idle.fbx";
    var SIT_ANIMATION_FPS = 30;
    var SIT_ANIMATION_FIRST_FRAME = 1;
    var SIT_ANIMATION_LAST_FRAME = 350;
    var REACH = 4.0;             // m from the avatar to the vehicle's surface, to get in
    var CAMERA_BACK = 7.0;       // m behind the vehicle
    var CAMERA_UP = 2.5;         // m above it
    var CAMERA_RATE = 6.0;       // per second: how fast the camera settles behind the vehicle
    var REVERSE_SPEED = 0.5;     // m/s: pressing against the direction of travel brakes above this speed
    var STAND_KEY = "f";
    var DRIVE_KEYS = [DriveKeys.TRANSLATE_X, DriveKeys.TRANSLATE_Y, DriveKeys.TRANSLATE_Z, DriveKeys.YAW, DriveKeys.STEP_YAW,
                      DriveKeys.STEP_TRANSLATE_X, DriveKeys.STEP_TRANSLATE_Y, DriveKeys.STEP_TRANSLATE_Z];

    var THROTTLE_RATE = 0.5;     // per second: how fast W/S move an airship's throttle setting

    var seated = null;           // { vehicleID, seat, driver, airship }
    var previousCameraMode = null;
    var cameraPosition = null;
    var throttleSetting = 0;     // an airship's

    function parseList(text) {
        if (!text) {
            return [];
        }
        try {
            var value = JSON.parse(text);
            return Array.isArray(value) ? value : [];
        } catch (e) {
            return [];
        }
    }

    function vec3Of(value) {
        if (Array.isArray(value) && value.length >= 3) {
            return { x: value[0], y: value[1], z: value[2] };
        }
        if (value && typeof value.x === "number") {
            return { x: value.x, y: value.y, z: value.z };
        }
        return { x: 0, y: 0, z: 0 };
    }

    function rolesToOverride() {
        return MyAvatar.getAnimationRoles().filter(function(role) {
            return !(role.indexOf("right") === 0 || role.indexOf("left") === 0);
        });
    }

    // the vehicle behind a clicked entity: itself, the chassis of a clicked wheel, or the airship a clicked part belongs to
    function vehicleOf(entityID) {
        var properties = Entities.getEntityProperties(entityID, ["vehicle", "parentID", "position", "rotation", "dimensions"]);
        if (!properties) {
            return null;
        }
        var hops = 0;
        while ((!properties.vehicle || properties.vehicle.type === "none" || properties.vehicle.type === "wheel") &&
                properties.parentID && properties.parentID !== Uuid.NULL && hops++ < 4) {
            entityID = properties.parentID;
            properties = Entities.getEntityProperties(entityID, ["vehicle", "parentID", "position", "rotation", "dimensions"]);
            if (!properties) {
                return null;
            }
        }
        if (!properties.vehicle || (properties.vehicle.type !== "wheeled" && properties.vehicle.type !== "airship" && properties.vehicle.type !== "aircraft")) {
            return null;
        }
        properties.id = entityID;
        return properties;
    }

    // the helm of an airship, or the stick of an aircraft (round 8, the same keys: Space and C tilt its nacelles up toward
    // hover and down toward forward flight while held): the crew stands on the deck in the ship's own world (the physics
    // makes the avatar a member of the frame by where it stands), so taking the helm is walking to it and taking the
    // keys, no parenting
    function takeHelm(vehicle, seat) {
        seated = { vehicleID: vehicle.id, seat: seat, driver: true, airship: true };
        throttleSetting = 0;
        for (var i = 0; i < DRIVE_KEYS.length; i++) {
            MyAvatar.disableDriveKey(DRIVE_KEYS[i]);
        }
        var helm = Vec3.sum(vehicle.position, Vec3.multiplyQbyV(vehicle.rotation, vec3Of(seat.position)));
        MyAvatar.position = Vec3.sum(helm, { x: 0, y: 1.0, z: 0 });
        MyAvatar.orientation = Quat.multiply(vehicle.rotation, Quat.fromPitchYawRollDegrees(0, seat.yaw || 0, 0));
        Entities.editEntity(vehicle.id, { vehicle: { driver: MyAvatar.sessionUUID } });
        Script.update.connect(update);
        print("vehicles.js: at the helm of " + vehicle.id + ": W/S throttle, X cut, A/D rudder, up/down elevator, Space/C up and down, B ballast, V vent, F leave");
    }

    function leaveHelm() {
        var vehicleID = seated.vehicleID;
        seated = null;
        Script.update.disconnect(update);
        Entities.clearVehicleControls(vehicleID);
        var properties = Entities.getEntityProperties(vehicleID, ["vehicle"]);
        if (properties && properties.vehicle && properties.vehicle.driver === MyAvatar.sessionUUID) {
            Entities.editEntity(vehicleID, { vehicle: { driver: Uuid.NULL } });
        }
        for (var i = 0; i < DRIVE_KEYS.length; i++) {
            MyAvatar.enableDriveKey(DRIVE_KEYS[i]);
        }
        print("vehicles.js: left the helm of " + vehicleID);
    }

    function sit(vehicle, seat) {
        var isDriver = !!seat.driver;
        seated = { vehicleID: vehicle.id, seat: seat, driver: isDriver };
        MyAvatar.collisionsEnabled = false;
        MyAvatar.hmdLeanRecenterEnabled = false;
        var roles = rolesToOverride();
        for (var i = 0; i < roles.length; i++) {
            MyAvatar.overrideRoleAnimation(roles[i], SIT_ANIMATION_URL, SIT_ANIMATION_FPS, true, SIT_ANIMATION_FIRST_FRAME, SIT_ANIMATION_LAST_FRAME);
        }
        for (i = 0; i < DRIVE_KEYS.length; i++) {
            MyAvatar.disableDriveKey(DRIVE_KEYS[i]);
        }
        // the avatar rides the vehicle: parented at the seat, facing the seat's way (0 is the vehicle's forward)
        MyAvatar.parentID = vehicle.id;
        MyAvatar.parentJointIndex = 65535;
        MyAvatar.localPosition = vec3Of(seat.position);
        MyAvatar.localOrientation = Quat.fromPitchYawRollDegrees(0, seat.yaw || 0, 0);
        if (isDriver) {
            Entities.editEntity(vehicle.id, { vehicle: { driver: MyAvatar.sessionUUID } });
        }
        previousCameraMode = Camera.mode;
        Camera.mode = "independent";
        cameraPosition = null;
        Script.update.connect(update);
        print("vehicles.js: sitting in " + (seat.name || "a seat") + " of " + vehicle.id + (isDriver ? " as the driver" : " (the driver is " + vehicle.vehicle.driver + ")"));
    }

    function stand() {
        if (!seated) {
            return;
        }
        if (seated.airship) {
            leaveHelm();
            return;
        }
        var vehicleID = seated.vehicleID;
        var wasDriver = seated.driver;
        seated = null;
        Script.update.disconnect(update);
        Entities.clearVehicleControls(vehicleID);
        if (wasDriver) {
            var properties = Entities.getEntityProperties(vehicleID, ["vehicle"]);
            if (properties && properties.vehicle && properties.vehicle.driver === MyAvatar.sessionUUID) {
                Entities.editEntity(vehicleID, { vehicle: { driver: Uuid.NULL } });
            }
        }
        // step out beside the vehicle, upright whatever way up the vehicle lies: the level right of where the seat
        // faces, half a metre up. goToLocation, not setting position and orientation: those race the physics step,
        // which hands back the pose the avatar had in the seat (a tilted one in a car on its side); goToLocation is
        // applied between steps and finds a free spot if the one asked for is inside something
        var position = MyAvatar.position;
        var level = Quat.cancelOutRollAndPitch(MyAvatar.orientation);
        var right = Quat.getRight(level);
        MyAvatar.parentID = Uuid.NULL;
        MyAvatar.goToLocation(Vec3.sum(position, Vec3.sum(Vec3.multiply(1.5, right), { x: 0, y: 0.5, z: 0 })), true, level, false);
        for (var i = 0; i < DRIVE_KEYS.length; i++) {
            MyAvatar.enableDriveKey(DRIVE_KEYS[i]);
        }
        var roles = rolesToOverride();
        for (i = 0; i < roles.length; i++) {
            MyAvatar.restoreRoleAnimation(roles[i]);
        }
        MyAvatar.collisionsEnabled = true;
        MyAvatar.hmdLeanRecenterEnabled = true;
        if (previousCameraMode !== null) {
            Camera.mode = previousCameraMode;
            previousCameraMode = null;
        }
        print("vehicles.js: standing up from " + vehicleID);
    }

    var HELM_REACH = 20;  // m from the vehicle's centre: farther, the helm is left

    function update(dt) {
        if (!seated) {
            return;
        }
        var properties = Entities.getEntityProperties(seated.vehicleID, ["id", "position", "rotation", "velocity", "vehicle"]);
        if (!properties || properties.id !== seated.vehicleID) {
            stand();   // the vehicle is gone (a deleted entity's properties come back as defaults, a null id among them)
            return;
        }
        if (seated.airship && Vec3.distance(MyAvatar.position, properties.position) > HELM_REACH) {
            stand();   // at a helm without a seat, so whoever is taken away from it (a teleport) has left it
            return;
        }
        var front = Quat.getFront(properties.rotation);
        if (seated.airship) {
            var k = Controller.Hardware.Keyboard;
            throttleSetting += dt * THROTTLE_RATE * (Controller.getValue(k.W) - Controller.getValue(k.S));
            throttleSetting = Math.max(-1, Math.min(1, throttleSetting));
            if (Controller.getValue(k.X) > 0.5) {
                throttleSetting = 0;
            }
            var rudder = (Controller.getValue(k.D) || Controller.getValue(k.Right)) - (Controller.getValue(k.A) || Controller.getValue(k.Left));
            var elevator = Controller.getValue(k.Up) - Controller.getValue(k.Down);
            var lift = (Controller.getValue(k.Space) > 0.5 ? 1 : 0) - (Controller.getValue(k.C) > 0.5 ? 1 : 0);
            var trim = (Controller.getValue(k.B) > 0.5 ? 1 : 0) - (Controller.getValue(k.V) > 0.5 ? 1 : 0);
            Entities.setVehicleControls(seated.vehicleID, { throttle: throttleSetting, steering: rudder, pitch: elevator, lift: lift, trim: trim });
            return;
        }
        if (seated.driver) {
            // the keys themselves (the avatar's drive actions are off while seated, and the keyboard's own inputs are
            // always there): W/S or the up and down arrows, A/D or the left and right arrows, Space
            var keys = Controller.Hardware.Keyboard;
            var forward = (Controller.getValue(keys.W) || Controller.getValue(keys.Up)) - (Controller.getValue(keys.S) || Controller.getValue(keys.Down));
            var steering = (Controller.getValue(keys.D) || Controller.getValue(keys.Right)) - (Controller.getValue(keys.A) || Controller.getValue(keys.Left));
            var handBrake = Controller.getValue(keys.Space) > 0.5 ? 1 : 0;
            var brake = 0;
            // pressing against the way the car is going is the brake until it has stopped; then it is reverse
            var speedAlong = Vec3.dot(properties.velocity, front);
            if (forward !== 0 && forward * speedAlong < 0 && Math.abs(speedAlong) > REVERSE_SPEED) {
                brake = Math.abs(forward);
                forward = 0;
            }
            Entities.setVehicleControls(seated.vehicleID, { throttle: forward, steering: steering, brake: brake, handBrake: handBrake });
        }
        // the chase camera, behind and above, settling at a rate so a bump does not swing it
        var wanted = Vec3.sum(properties.position, Vec3.sum(Vec3.multiply(-CAMERA_BACK, front), { x: 0, y: CAMERA_UP, z: 0 }));
        if (!cameraPosition) {
            cameraPosition = wanted;
        } else {
            cameraPosition = Vec3.mix(cameraPosition, wanted, 1 - Math.exp(-dt * CAMERA_RATE));
        }
        Camera.position = cameraPosition;
        Camera.orientation = Quat.lookAtSimple(cameraPosition, Vec3.sum(properties.position, { x: 0, y: 1, z: 0 }));
    }

    function onMousePress(entityID, event) {
        if (seated || !event.isPrimaryButton || event.isAlt || event.isControl) {
            return;
        }
        var vehicle = vehicleOf(entityID);
        if (!vehicle) {
            return;
        }
        var reach = REACH + 0.5 * Math.max(vehicle.dimensions.x, vehicle.dimensions.z);
        if (Vec3.distance(MyAvatar.position, vehicle.position) > reach) {
            return;
        }
        var seats = parseList(vehicle.vehicle.seats);
        if (seats.length === 0) {
            return;
        }
        // the driver's seat is free when nobody holds it, or when whoever holds it has left the domain (a client that
        // quit in the seat leaves its session in vehicle.driver)
        var driver = vehicle.vehicle.driver;
        var driverFree = !driver || driver === Uuid.NULL || driver === MyAvatar.sessionUUID ||
            AvatarList.getAvatarIdentifiers().indexOf(driver) < 0;
        var chosen = null;
        var best = Infinity;
        for (var i = 0; i < seats.length; i++) {
            var seat = seats[i];
            if (seat.driver && driverFree) {
                chosen = seat;
                break;
            }
            if (seat.driver) {
                continue;
            }
            var world = Vec3.sum(vehicle.position, Vec3.multiplyQbyV(vehicle.rotation, vec3Of(seat.position)));
            var distance = Vec3.distance(MyAvatar.position, world);
            if (distance < best) {
                best = distance;
                chosen = seat;
            }
        }
        if (chosen) {
            if (vehicle.vehicle.type === "airship" || vehicle.vehicle.type === "aircraft") {
                if (chosen.driver) {
                    takeHelm(vehicle, chosen);
                }
                return;   // the crew just walks aboard
            }
            sit(vehicle, chosen);
        }
    }

    function onKeyPress(event) {
        if (seated && event.text && event.text.toLowerCase() === STAND_KEY && !event.isControl && !event.isAlt) {
            stand();
        }
    }

    Entities.mousePressOnEntity.connect(onMousePress);
    Controller.keyPressEvent.connect(onKeyPress);
    Script.scriptEnding.connect(function() {
        Entities.mousePressOnEntity.disconnect(onMousePress);
        Controller.keyPressEvent.disconnect(onKeyPress);
        stand();
    });
}());
