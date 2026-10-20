//
//  bury.js
//  tests-manual/physics-baseline
//
//  Feet put under a Terrain by a teleport: the character must come back out onto the ground (CharacterController::
//  liftOutOfFloor; a triangle mesh has no inside, so Jolt's own recovery leaves a buried character stuck, and judders it
//  when driven). On the rover terrain (landrover.js, make-terrain.py), serverless: the avatar stands on the flat pad
//  and on a hillside, is put 0.5 m and 1.0 m under each, and must be standing on the ground again within a second;
//  then it walks over the hills for 20 s, during which the lift must never fire (run-bury.sh counts the client's
//  "lifted onto it" lines between the walk's two marks). Prints PHYSICS-BURY lines.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var config = typeof BURY_CONFIG !== "undefined" ? BURY_CONFIG : {};
    var LIFETIME = 300;
    var c = LandRover.TERRAIN_CENTER;
    var SPOTS = [
        { name: "pad", x: c.x + 3, z: c.z + 2 },
        { name: "hillside", x: c.x + 45, z: c.z - 30 }
    ];
    var DEPTHS = [0.5, 1.0];
    var cases = [];
    SPOTS.forEach(function (spot) { DEPTHS.forEach(function (depth) { cases.push({ spot: spot, depth: depth }); }); });
    var failures = 0;

    function log(s) { print("PHYSICS-BURY " + s); }
    function under(x, z) { return LandRover.groundHeight(x, z) - MyAvatar.getWorldFeetPosition().y; }

    function stand(spot, then) {
        MyAvatar.position = { x: spot.x, y: LandRover.groundHeight(spot.x, spot.z) + 1.5, z: spot.z };
        Script.setTimeout(then, 2500);
    }

    function runCase(i) {
        if (i >= cases.length) {
            walk();
            return;
        }
        var k = cases[i];
        stand(k.spot, function () {
            var before = under(k.spot.x, k.spot.z);
            var p = MyAvatar.position;
            MyAvatar.position = { x: p.x, y: p.y - k.depth, z: p.z };
            var t0 = Date.now(), out = null, deepest = 0;
            var watch = Script.setInterval(function () {
                var u = under(k.spot.x, k.spot.z);
                deepest = Math.max(deepest, u);
                if (out === null && Date.now() - t0 > 50 && Math.abs(u - before) < 0.05) {
                    out = (Date.now() - t0) / 1000;
                }
                if (Date.now() - t0 > 1500) {
                    Script.clearInterval(watch);
                    var ok = out !== null && out <= 1.0 && Math.abs(u - before) < 0.05;
                    if (!ok) { failures++; }
                    log((ok ? "PASS " : "FAIL ") + k.spot.name + ", " + k.depth + " m under: " +
                        (out === null ? "still under" : "out after " + out.toFixed(2) + " s") + "; feet " +
                        u.toFixed(3) + " m under the ground after 1.5 s (" + before.toFixed(3) + " standing)");
                    runCase(i + 1);
                }
            }, 20);
        });
    }

    function walk() {
        var legs = [{ x: 3, z: 0 }, { x: 0, z: -3 }, { x: -3, z: 0 }, { x: 0, z: 3 }];
        stand(SPOTS[1], function () {
            log("walk start");
            var lowest = 1e9, highest = -1e9;
            var track = Script.setInterval(function () {
                var y = MyAvatar.getWorldFeetPosition().y;
                lowest = Math.min(lowest, y); highest = Math.max(highest, y);
            }, 50);
            legs.forEach(function (v, n) {
                Script.setTimeout(function () {
                    MyAvatar.motorReferenceFrame = "world"; MyAvatar.motorMode = "simple"; MyAvatar.motorTimescale = 0.25;
                    MyAvatar.motorVelocity = { x: v.x, y: 0, z: v.z };
                }, n * 5000);
            });
            Script.setTimeout(function () {
                Script.clearInterval(track);
                MyAvatar.motorVelocity = { x: 0, y: 0, z: 0 }; MyAvatar.motorTimescale = 1000000;
                log("walk end; the feet went from " + lowest.toFixed(2) + " to " + highest.toFixed(2) + " m");
                log("done, " + failures + " of " + cases.length + " burials failed");
                Script.setTimeout(function () { Test.quit(); }, 1000);
            }, legs.length * 5000);
        });
    }

    function setUp(tries) {
        var terrain = LandRover.createTerrain(config.terrainURL || "", LIFETIME);
        if (!terrain || terrain === Uuid.NULL) {
            if (tries < 60) { Script.setTimeout(function () { setUp(tries + 1); }, 1000); } else { log("no terrain; giving up"); Test.quit(); }
            return;
        }
        stand(SPOTS[0], function () { Script.setTimeout(function () { runCase(0); }, 3000); });
    }

    Script.setTimeout(function () { setUp(0); }, 3000);
}());
