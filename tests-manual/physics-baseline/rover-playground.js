//
//  rover-playground.js
//  tests-manual/physics-baseline
//
//  Drive the Land Rover yourself (PHYSICS.md round 6). Makes the terrain and the car on its pad (or finds the ones already
//  there), stands you beside it, and leaves the rest to scripts/system/vehicles.js: click the car to sit (the driver's
//  seat if free), W/S throttle and brake, A/D steer, Space hand brake, F stands up. R puts a fresh car on the pad when
//  you have rolled it; T takes you back to the pad. The terrain's URL comes from ROVER_CONFIG (drive-rover.sh writes it,
//  with landrover.js, into one script).
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var config = (typeof ROVER_CONFIG !== "undefined" && ROVER_CONFIG) ? ROVER_CONFIG : {};
    var TERRAIN_URL = config.terrainURL || "";
    var LIFETIME = config.lifetime || 3600;
    var terrain = null;
    var car = null;
    var tries = 0;

    function log(s) { print("ROVER-PLAYGROUND " + s); }

    function toPad() {
        var c = LandRover.TERRAIN_CENTER;
        MyAvatar.position = { x: c.x + 3, y: LandRover.groundHeight(c.x + 3, c.z) + 1.2, z: c.z };
        MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, 90, 0);
    }

    function existingCar() {
        var found = Entities.findEntitiesByName("rover-chassis", LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false);
        return found.length ? found[0] : null;
    }

    function newCar() {
        LandRover.clearVehicles();
        var c = LandRover.TERRAIN_CENTER;
        car = LandRover.create(c.x, c.z, 0, LIFETIME).chassis;
        log("a fresh Land Rover on the pad: " + car);
    }

    function setUp() {
        terrain = LandRover.createTerrain(TERRAIN_URL, LIFETIME);
        if (!terrain || terrain === Uuid.NULL) {
            terrain = null;
            if (++tries < 60) { Script.setTimeout(setUp, 1000); } else { log("no entity server; giving up"); }
            return;
        }
        toPad();
        // the ground takes a moment; the car a moment after it
        Script.setTimeout(function () {
            car = existingCar();
            if (car) {
                LandRover.clearVehicles("rover-");  // an airship or aircraft left on the pad
                log("the Land Rover is already here: " + car);
            } else {
                newCar();
            }
            log("click the car to get in; W/S A/D Space; F to get out; R for a fresh car; T back to the pad");
        }, 3000);
    }

    function onKey(event) {
        if (!event.text || event.isControl || event.isAlt) { return; }
        var key = event.text.toLowerCase();
        if (key === "r" && MyAvatar.parentID == Uuid.NULL) { newCar(); }
        if (key === "t" && MyAvatar.parentID == Uuid.NULL) { toPad(); }
    }

    Controller.keyPressEvent.connect(onKey);
    Script.scriptEnding.connect(function () { Controller.keyPressEvent.disconnect(onKey); });
    Script.setTimeout(setUp, 4000);
    log("started; terrain " + TERRAIN_URL);
}());
