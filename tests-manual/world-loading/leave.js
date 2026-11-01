//
//  leave.js
//  tests-manual/world-loading
//
//  A plain leave, as Caity's 12:32 of 10-22: from the world the client starts in, a hop to Tivoli Town; once it has
//  landed and STAY_AFTER s more have gone, back (location.goBack, as Cancel does, but with the screen down); then
//  CHECK_AFTER s in the world it went back to, while the log shows whether a world entity script still throws.
//  HOP_URL is prepended by run.sh (HOP_SCRIPT=leave.js).
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var URL = typeof HOP_URL !== "undefined" ? HOP_URL : "hifi://localhost:42102";
    var STAY = typeof STAY_AFTER !== "undefined" ? STAY_AFTER : 40;
    // LEAVE_BY_ADDRESS: leave as Caity's key did, by going to the starting world's address (otherwise location.goBack)
    var BY_ADDRESS = typeof LEAVE_BY_ADDRESS !== "undefined";
    var startAddress = "";
    var CHECK_AFTER = 15;
    var start = Date.now();
    var hopped = false, landedAt = 0, sawLoading = false;
    function t() { return ((Date.now() - start) / 1000).toFixed(2); }
    function step(text) { print("WORLD-LOADING-LEAVE " + text + " at " + t() + " s"); }

    Script.setTimeout(function () {
        startAddress = location.href;
        step("hop (from \"" + startAddress + "\")");
        Messages.sendLocalMessage("Darkride-Hop", JSON.stringify({ type: "start", world: "Tivoli Town", author: "Caitlyn",
            picture: "https://vr.darkride.ai/api/domain/test/image" }));
        location.handleLookupString(URL);
        hopped = true;
    }, 10000);

    var timer = Script.setInterval(function () {
        if (!hopped || landedAt) {
            return;
        }
        var loading = Window.getWorldLoading ? Window.getWorldLoading() : { active: false };
        sawLoading = sawLoading || loading.active;   // the whole-world load has begun (the old world's physics is on)
        if (sawLoading && location.hostname && Window.isPhysicsEnabled() && !loading.active) {
            landedAt = Date.now();
            Script.clearInterval(timer);
            step("landed in \"" + location.hostname + "\"; leaving in " + STAY + " s");
            Script.setTimeout(function () {
                step("leaving (" + (BY_ADDRESS ? "to " + startAddress : "goBack") + ")");
                if (BY_ADDRESS) {
                    location.handleLookupString(startAddress);
                } else {
                    location.goBack();
                }
                Script.setTimeout(function () {
                    step("check done, host \"" + location.hostname + "\"; quitting");
                    Test.quit();
                }, CHECK_AFTER * 1000);
            }, STAY * 1000);
        }
    }, 250);
}());
