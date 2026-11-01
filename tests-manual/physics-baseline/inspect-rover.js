//
//  inspect-rover.js
//  tests-manual/physics-baseline
//
//  Pictures of the native Inspector's Vehicle section (PHYSICS.md 4.8): with the Land Rover on its pad (landrover.js and
//  rover-playground.js, concatenated before this file by inspect-rover.sh), turns build mode on, selects the chassis and
//  saves the Inspector's content and the viewport (the wheel and seat gizmos), then one wheel entity, then the chassis
//  again. The pictures go to INSPECT_DIR. Lines starting with INSPECT-ROVER say what was saved.
//
//  Distributed under the Apache License, Version 2.0.
//
(function () {
    var DIR = (typeof INSPECT_DIR !== "undefined") ? INSPECT_DIR : "/tmp";
    var tries = 0;
    var shot = null;
    function log(s) { print("INSPECT-ROVER " + s); }
    function byName(name) {
        var found = Entities.findEntitiesByName(name, LandRover.TERRAIN_CENTER, 2 * LandRover.TERRAIN_SIZE, false);
        return found.length ? found[0] : null;
    }
    function pictures(name, next) {
        Script.setTimeout(function () {
            var ok = Test.saveInspectorPicture(DIR + "/" + name + "-inspector.png");
            log(name + "-inspector.png " + (ok ? "saved" : "NOT saved"));
            shot = name;
            Window.takeSnapshot(false, false, 0, name + "-viewport.png");
            Script.setTimeout(next, 4000);
        }, 3000);
    }
    function onSnapshot(path) {
        log((shot || "?") + "-viewport -> " + path);
    }
    function finish() {
        Window.stillSnapshotTaken.disconnect(onSnapshot);
        log("done");
        Script.setTimeout(function () { Test.quit(); }, 1000);   // the client quits with the script, as the runners' do
    }
    function start() {
        var chassis = byName("rover-chassis");
        if (!chassis) {
            if (++tries < 90) { Script.setTimeout(start, 1000); } else { log("no car"); Test.quit(); }
            return;
        }
        var vehicle = Entities.getEntityProperties(chassis, ["vehicle"]).vehicle || {};
        log("car " + chassis + ": type " + vehicle.type + ", mass " + vehicle.mass + ", " +
            (JSON.parse(vehicle.wheels || "[]").length) + " wheels, " + (JSON.parse(vehicle.seats || "[]").length) + " seats");
        Window.stillSnapshotTaken.connect(onSnapshot);
        Test.selectInEditor([chassis]);
        Test.frameEditorSelection();
        pictures("01-chassis", function () {
            var wheel = byName("rover-wheel-FL");
            log("wheel entity " + wheel);
            if (wheel) { Test.selectInEditor([wheel]); }
            pictures("02-wheel", function () {
                Test.selectInEditor([chassis]);
                pictures("03-chassis-again", finish);
            });
        });
    }
    Script.setTimeout(start, 8000);
    Script.setTimeout(function () { log("watchdog: quitting"); Test.quit(); }, 150000);
})();
