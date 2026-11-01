//
//  cancel-twice.js
//  tests-manual/world-loading
//
//  Caity's retest of 10-22 in one client: from the world the client starts in, a hop to Tivoli Town cancelled as soon
//  as Cancel shows (CANCEL_FIRST s), then the hop again, cancelled later (CANCEL_SECOND s), then CHECK_AFTER s in the
//  world it went back to, counting how often a world entity script still throws (it shows in the log as
//  UncaughtException lines; this prints the steps). HOP_URL is prepended by run.sh (HOP_SCRIPT=cancel-twice.js).
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var URL = typeof HOP_URL !== "undefined" ? HOP_URL : "hifi://localhost:42102";
    var CANCEL_FIRST = typeof CANCEL_FIRST_AT !== "undefined" ? CANCEL_FIRST_AT : 3.2;
    var CANCEL_SECOND = typeof CANCEL_SECOND_AT !== "undefined" ? CANCEL_SECOND_AT : 20;
    var CHECK_AFTER = 15;
    var start = Date.now();
    function t() { return ((Date.now() - start) / 1000).toFixed(2); }
    function step(text) { print("WORLD-LOADING-CANCEL2 " + text + " at " + t() + " s"); }

    function hop() {
        Messages.sendLocalMessage("Darkride-Hop", JSON.stringify({ type: "start", world: "Tivoli Town", author: "Caitlyn",
            picture: "https://vr.darkride.ai/api/domain/test/image" }));
        location.handleLookupString(URL);
    }

    // the first world settles, then: hop, Cancel, back; hop again, wait, Cancel, back; look for leaking scripts
    Script.setTimeout(function () {
        step("hop 1");
        hop();
        Script.setTimeout(function () {
            step("Cancel 1");
            Window.pressLoadingScreenButton("cancel");
            Script.setTimeout(function () {
                step("hop 2 (host now \"" + location.hostname + "\")");
                hop();
                Script.setTimeout(function () {
                    step("Cancel 2 (whole world loading: " + JSON.stringify(Window.getWorldLoading()) + ")");
                    Window.pressLoadingScreenButton("cancel");
                    Script.setTimeout(function () {
                        step("check done, host \"" + location.hostname + "\"; quitting");
                        Test.quit();
                    }, CHECK_AFTER * 1000);
                }, CANCEL_SECOND * 1000);
            }, 6000);
        }, CANCEL_FIRST * 1000);
    }, 10000);
}());
