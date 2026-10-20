// PHYSICS.md round 10: the loading screen's length. Polls Window.isPhysicsEnabled() (the gate that lifts the loading
// screen, "Physics enabled" in the log) from the script's start, prints the moment it turns on, and quits 5 s later.
// Quits after LOADING_LIMIT s regardless. Run by run-loading.sh.
//   Darkride --url hifi://localhost:42102 --testScript loading.js
// Round 11 (WORLD-LOADING.md, the arrival gate): the script holds the arrival to the safe landing itself from its start
// (Window.setArrivalWaitsForNearby, what the loading screen does while it is up), so that both the cold and the warm
// run measure the gate as a hop does. Round 10's warm run did not: its physics came on by ghosting in the second the
// client connected, before worldLoading.js had raised the flag. ARRIVAL_WAITS = false leaves the flag to the screen.
(function () {
    var LIMIT = (typeof LOADING_LIMIT === "number") ? LOADING_LIMIT : 240;
    var WAITS = (typeof ARRIVAL_WAITS === "boolean") ? ARRIVAL_WAITS : true;
    var start = Date.now();
    var enabledAt = -1;
    var printedLater = false;
    // what the client holds within NEARBY m of the avatar: the count by type and the model names (the arrival gate
    // waits for what is near; this shows whether the near models were in before the gate lifted or only after)
    var NEARBY = 10;
    function printNearby(when) {
        var ids = Entities.findEntities(MyAvatar.position, NEARBY);
        var byType = {}, models = [];
        ids.forEach(function (id) {
            var p = Entities.getEntityProperties(id, ["type", "name", "position"]);
            byType[p.type] = (byType[p.type] || 0) + 1;
            if (p.type === "Model") {
                models.push((p.name || "?") + " " + Vec3.distance(p.position, MyAvatar.position).toFixed(1) + " m");
            }
        });
        print("PHYSICS-LOADING held within " + NEARBY + " m " + when + ": " + ids.length + " " + JSON.stringify(byType)
            + (models.length ? "; models: " + models.join(", ") : ""));
    }
    if (WAITS && Window.setArrivalWaitsForNearby) {
        Window.setArrivalWaitsForNearby(true);
    }
    print("PHYSICS-LOADING started, limit " + LIMIT + " s, arrival waits for nearby " + (WAITS && !!Window.setArrivalWaitsForNearby)
        + ", physics " + Window.isPhysicsEnabled() + ", connected " + location.isConnected);
    var timer = Script.setInterval(function () {
        var elapsed = (Date.now() - start) / 1000;
        if (enabledAt < 0 && Window.isPhysicsEnabled()) {
            enabledAt = elapsed;
            print("PHYSICS-LOADING physics enabled " + enabledAt.toFixed(1) + " s after the script started, avatar at "
                + JSON.stringify(MyAvatar.position));
            printNearby("at physics enabled");
        }
        if (enabledAt >= 0 && !printedLater && elapsed > enabledAt + 4) {
            printedLater = true;
            printNearby("4 s later");
        }
        if ((enabledAt >= 0 && elapsed > enabledAt + 5) || elapsed > LIMIT) {
            Script.clearInterval(timer);
            print("PHYSICS-LOADING done: physics " + (enabledAt >= 0 ? "enabled at " + enabledAt.toFixed(1) + " s" : "never enabled")
                + ", quitting at " + elapsed.toFixed(1) + " s");
            Test.quit();
        }
    }, 200);
}());
