//
//  caity-seq.js
//  tests-manual/world-loading
//
//  Caity's retest of 10-22 ~17:56, step for step in one client (her hop keys: Darkride-Hop "start", then
//  location.handleLookupString): into Tivoli Town and land; back to the starting world by its address; into Tivoli Town
//  and Cancel as soon as the buttons show; into Tivoli Town, wait CANCEL_LATE_AT s, Cancel; then 20 s in the starting
//  world, while the client log shows whether a world entity script still throws. HOP_URL is prepended by run.sh
//  (HOP_SCRIPT=caity-seq.js).
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var URL = typeof HOP_URL !== "undefined" ? HOP_URL : "hifi://localhost:42102";
    var LATE = typeof CANCEL_LATE_AT !== "undefined" ? CANCEL_LATE_AT : 12;
    var start = Date.now();
    var home = "";
    function t() { return ((Date.now() - start) / 1000).toFixed(2); }
    function step(text) { print("WORLD-LOADING-SEQ " + text + " at " + t() + " s"); }
    function hop(address) {
        Messages.sendLocalMessage("Darkride-Hop", JSON.stringify({ type: "start", world: "Tivoli Town", author: "Caitlyn",
            picture: "https://vr.darkride.ai/api/domain/test/image" }));
        location.handleLookupString(address);
    }
    // calls back once the whole-world load has begun and ended with physics on
    function whenLanded(callback) {
        var saw = false;
        var timer = Script.setInterval(function () {
            var loading = Window.getWorldLoading ? Window.getWorldLoading() : { active: false };
            saw = saw || loading.active;
            if (saw && !loading.active && Window.isPhysicsEnabled() && location.hostname) {
                Script.clearInterval(timer);
                callback();
            }
        }, 250);
    }

    Script.setTimeout(function () {
        home = location.href;
        step("1: into Tivoli Town");
        hop(URL);
        whenLanded(function () {
            step("1: landed");
            Script.setTimeout(function () {
                step("2: back to the starting world by its address");
                hop(home);
                Script.setTimeout(function () {
                    step("3: into Tivoli Town, Cancel as the buttons show");
                    hop(URL);
                    Script.setTimeout(function () {
                        Window.pressLoadingScreenButton("cancel");
                        step("3: Cancel");
                        Script.setTimeout(function () {
                            step("4: into Tivoli Town, Cancel after " + LATE + " s");
                            hop(URL);
                            Script.setTimeout(function () {
                                Window.pressLoadingScreenButton("cancel");
                                step("4: Cancel");
                                Script.setTimeout(function () {
                                    step("done, host \"" + location.hostname + "\"; quitting");
                                    Test.quit();
                                }, 20000);
                            }, LATE * 1000);
                        }, 6000);
                    }, 3200);
                }, 6000);
            }, 8000);
        });
    }, 10000);
}());
