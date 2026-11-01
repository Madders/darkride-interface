//
//  airship-playground.js
//  tests-manual/physics-baseline
//
//  Fly the airship yourself (PHYSICS.md round 7), or the tilt-thrust aircraft (round 8) when AIRSHIP_CONFIG.vehicle is
//  "aircraft" (fly-aircraft.sh): then W/S set the throttle, A/D roll and yaw, up/down the elevator, Space tilts the
//  nacelles up toward hover while held and C tilts them down toward forward flight, F leaves the stick. Makes the terrain and the ship on its pad (or finds the ones already
//  there), stands you beside it, and leaves the rest to scripts/system/vehicles.js: walk aboard (the deck is a step up
//  from the pad at the stern; its top is 0.4 m above the ground) and click the ship to take the helm: W/S set the throttle
//  (X cuts it), A/D or left/right the rudder, up/down the elevator, Space and C the vertical thruster, B drops ballast, V
//  vents gas, F leaves the helm. R puts a fresh ship on the pad; T takes you back to the pad. The terrain's URL comes from
//  AIRSHIP_CONFIG (fly-airship.sh writes it, with landrover.js and airship-rig.js, into one script).
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var config = (typeof AIRSHIP_CONFIG !== "undefined" && AIRSHIP_CONFIG) ? AIRSHIP_CONFIG : {};
    var TERRAIN_URL = config.terrainURL || "";
    var LIFETIME = config.lifetime || 3600;
    var KIND = config.vehicle || "airship";
    var Rig = KIND === "aircraft" ? Aircraft : Airship;
    var HULL_NAME = Rig.HULL_NAME || "airship-hull";
    var terrain = null;
    var ship = null;
    var tries = 0;

    function log(s) { print((KIND === "aircraft" ? "AIRCRAFT-PLAYGROUND " : "AIRSHIP-PLAYGROUND ") + s); }

    function toPad() {
        var c = LandRover.TERRAIN_CENTER;
        MyAvatar.position = { x: c.x + 4, y: LandRover.groundHeight(c.x + 4, c.z) + 1.2, z: c.z + 6 };
        MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, 90, 0);
    }

    function existingShip() {
        var found = Entities.findEntitiesByName(HULL_NAME, LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false);
        return found.length ? found[0] : null;
    }

    function newShip() {
        Rig.deleteAll(LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE);
        LandRover.clearVehicles();
        var c = LandRover.TERRAIN_CENTER;
        ship = Rig.create(c.x, c.z, 0, LIFETIME).hull;
        if (KIND === "airship") {
            var deck = Airship.createDeckTrack(ship, LIFETIME);
            log("a fresh airship on the pad: " + ship + ", with a trolley " + deck.car + " on a track along its deck");
        } else {
            log("a fresh aircraft on the pad: " + ship);
        }
    }

    // An aircraft crosses the 192 m terrain in seconds: a flat plain 4 km across round it (four slabs framing the
    // terrain, their tops at its edges' height) gives it somewhere to fly over and land on. Not named like a vehicle's
    // part, so the playgrounds' clearing of the pad leaves it.
    function createPlain() {
        if (Entities.findEntitiesByName("airfield-plain", LandRover.TERRAIN_CENTER, 4000, false).length > 0) {
            return;
        }
        var c = LandRover.TERRAIN_CENTER, half = LandRover.TERRAIN_SIZE / 2, reach = 2000, top = -8, depth = 4;
        var side = reach - half;
        var slabs = [
            { x: c.x, z: c.z - half - side / 2, w: 2 * reach, d: side },
            { x: c.x, z: c.z + half + side / 2, w: 2 * reach, d: side },
            { x: c.x - half - side / 2, z: c.z, w: side, d: 2 * half },
            { x: c.x + half + side / 2, z: c.z, w: side, d: 2 * half }
        ];
        slabs.forEach(function (slab) {
            Entities.addEntity({
                type: "Box", name: "airfield-plain", position: { x: slab.x, y: top - depth / 2, z: slab.z },
                dimensions: { x: slab.w, y: depth, z: slab.d }, color: { red: 120, green: 150, blue: 95 },
                dynamic: false, collisionless: false, grab: { grabbable: false }, lifetime: LIFETIME
            });
        });
        log("a flat plain 4 km across round the terrain");
    }

    function setUp() {
        terrain = LandRover.createTerrain(TERRAIN_URL, LIFETIME);
        if (!terrain || terrain === Uuid.NULL) {
            terrain = null;
            if (++tries < 60) { Script.setTimeout(setUp, 1000); } else { log("no entity server; giving up"); }
            return;
        }
        toPad();
        if (KIND === "aircraft") {
            createPlain();
        }
        Script.setTimeout(function () {
            ship = existingShip();
            if (ship) {
                LandRover.clearVehicles(KIND + "-");  // a car or the other craft left on the pad
                log("the " + KIND + " is already here: " + ship);
            } else {
                newShip();
            }
            if (KIND === "aircraft") {
                log("climb aboard and click the craft for the stick (the flight assistant): W/S faster and slower, X stop, A/D bank (turn in a hover), Up/Down or Space/C climb and descend, F leave; R fresh craft; T to the pad");
            } else {
                log("walk aboard and click the ship for the helm; W/S throttle, X cut, A/D rudder, up/down elevator, Space/C up and down, B ballast, V vent, F leave; R fresh ship; T to the pad");
            }
        }, 3000);
    }

    function onKey(event) {
        if (!event.text || event.isControl || event.isAlt) { return; }
        var key = event.text.toLowerCase();
        if (key === "r" && MyAvatar.parentID == Uuid.NULL) { newShip(); }
        if (key === "t" && MyAvatar.parentID == Uuid.NULL) { toPad(); }
    }

    Controller.keyPressEvent.connect(onKey);
    Script.scriptEnding.connect(function () { Controller.keyPressEvent.disconnect(onKey); });
    Script.setTimeout(setUp, 4000);
    log("started; terrain " + TERRAIN_URL);
}());
