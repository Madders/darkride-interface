//
//  hop.js
//  tests-manual/world-loading
//
//  A hop as Explore makes one: from the world the client starts in, it says on "Darkride-Hop" that a hop to Tivoli
//  Town has started, waits 3 s (a world's instance starting), then goes to the test domain (HOP_URL, prepended by
//  run.sh). Every 100 ms it notes whether the loading screen is up, whether the domain has changed, and whether physics
//  is on. It must be up within a moment of the "start", stay up through the hop, and come down only once the new
//  world is connected with its physics on (and, with whole-world loading, its loading done). Prints WORLD-LOADING-HOP lines and takes
//  a snapshot while hopping.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var URL = typeof HOP_URL !== "undefined" ? HOP_URL : "hifi://localhost:42102";
    var start = 0, events = {}, wasUp = false, gaps = 0, domainChanged = false, finished = false;

    function t() { return (Date.now() - start) / 1000; }
    function mark(name) {
        if (events[name] === undefined) {
            events[name] = t();
            print("WORLD-LOADING-HOP " + name + " at " + events[name].toFixed(2) + " s");
        }
    }
    // the screen is local entities, which Entities' searches skip: it says when it comes up and goes
    var screenIsUp = false;
    var lastScreen = null;
    Messages.subscribe("Darkride-Hop");
    Messages.messageReceived.connect(function (channel, message) {
        var data = channel === "Darkride-Hop" ? JSON.parse(message) : {};
        if (data.type === "screen") {
            var buttonsChanged = !lastScreen || lastScreen.canCancel !== data.canCancel ||
                lastScreen.canShowNow !== data.canShowNow;
            if (!lastScreen || lastScreen.up !== data.up || lastScreen.status !== data.status ||
                    lastScreen.world !== data.world || buttonsChanged) {
                print("WORLD-LOADING-HOP screen " + (data.up ? "up" : "down") + " \"" + data.status + "\" world \"" +
                      data.world + "\" by \"" + data.author + "\" picture \"" + data.picture + "\" buttons " +
                      (data.canCancel ? "[Cancel]" : "") + (data.canShowNow ? "[Show me now]" : "") + " in \"" +
                      location.hostname + "\" at " + (start ? t().toFixed(2) + " s" : "start-up"));
            }
            screenIsUp = data.up;
            lastScreen = data;
        }
    });
    function screenUp() {
        return screenIsUp;
    }

    Window.domainChanged.connect(function () { if (start) { domainChanged = true; mark("domainChanged"); } });

    function sample() {
        if (finished) {
            return;
        }
        // the old world gone (its physics or its connection dropped), then the new one connected with physics on
        if (domainChanged && (!Window.isPhysicsEnabled() || !location.isConnected)) {
            mark("leftOldWorld");
        }
        if (events.leftOldWorld !== undefined && location.isConnected && Window.isPhysicsEnabled()) {
            if (events.landed === undefined && typeof HOP_PICTURES !== "undefined") {
                // and what the world looks like once the screen has gone
                Script.setTimeout(function () { Window.takeSnapshot(false, false, 0); print("WORLD-LOADING-HOP snapshot 3 s after landing"); }, 3000);
                // and an overview from high above the arrival, looking at the world's middle (the ground and the
                // water, which the arrival's own view may not take in), 10 s after landing
                Script.setTimeout(function () {
                    var target = { x: 0, y: 0, z: 0 };
                    var eye = { x: MyAvatar.position.x + 60, y: MyAvatar.position.y + 140, z: MyAvatar.position.z + 160 };
                    Camera.mode = "independent";
                    Camera.position = eye;
                    Camera.orientation = Quat.lookAtSimple(eye, target);
                    Script.setTimeout(function () {
                        Window.takeSnapshot(false, false, 0);
                        print("WORLD-LOADING-HOP overview snapshot 10 s after landing (render method " + Render.renderMethod + ")");
                    }, 1500);
                }, 8500);
            }
            // RENDER_METHOD (run.sh): the method to draw with from here on, as the presets set forward at start-up
            if (events.landed === undefined && typeof HOP_RENDER_METHOD !== "undefined") {
                Render.renderMethod = HOP_RENDER_METHOD;
            }
            // HOP_CHECK_SCRIPTS (run.sh): 15 s after landing, whether the world's entity scripts run: Tivoli Town's
            // hideByDistance.js hides its far plants (locallyVisible false) once it runs
            if (events.landed === undefined && typeof HOP_CHECK_SCRIPTS !== "undefined") {
                Script.setTimeout(function () {
                    var withScript = 0, hidden = 0;
                    Entities.findEntities(MyAvatar.position, 2000).forEach(function (id) {
                        var p = Entities.getEntityProperties(id, ["script", "locallyVisible"]);
                        if (p.script && p.script.indexOf("hidebyDistance") >= 0) {
                            withScript++;
                            hidden += p.locallyVisible === false ? 1 : 0;
                        }
                    });
                    print("WORLD-LOADING-HOP world scripts: " + withScript + " entities run hideByDistance, " + hidden +
                          " of them hidden by it");
                }, 15000);
            }
            // HOP_AGAIN=<s> (run.sh): that long after landing, the same hop again, to the world it is already in (as
            // Explore picking the world you're in): the screen must lift after its minimum, not at its time limit
            if (events.landed === undefined && typeof HOP_AGAIN !== "undefined") {
                Script.setTimeout(function () {
                    print("WORLD-LOADING-HOP hop again to the same world at " + t().toFixed(2) + " s");
                    Messages.sendLocalMessage("Darkride-Hop", JSON.stringify({ type: "start", world: "Tivoli Town",
                        author: "Caitlyn", picture: "https://vr.darkride.ai/api/domain/test/image" }));
                    location.handleLookupString(URL);
                }, HOP_AGAIN * 1000);
            }
            mark("landed");
        }
        var up = screenUp();
        if (up) {
            mark("screenUp");
        }
        if (wasUp && !up) {
            mark("screenDown");
            if (!(events.landed <= events.screenDown + 0.2)) {
                gaps++;   // down before the new world's physics: a gap in the screen
            }
        }
        if (events.screenUp !== undefined && !wasUp && up && events.screenDown !== undefined) {
            gaps++;       // up again after coming down
        }
        wasUp = up;
        if (Window.getWorldLoading && Window.getWorldLoading().active) {   // only in clients with whole-world loading
            mark("wholeWorldLoading");
        }
        // (with pictures, long enough for the overview 10 s after landing)
        var linger = typeof HOP_AGAIN !== "undefined" ? HOP_AGAIN + 8 :
            (typeof HOP_PICTURES !== "undefined" || typeof HOP_CHECK_SCRIPTS !== "undefined" ? 17 : 5);
        if ((events.screenDown !== undefined && t() > events.screenDown + linger) || t() > 120) {
            finished = true;
            var summary = { events: events, gaps: gaps, host: location.hostname };
            // down once landed (and the avatar has loaded: at most 10 s more), never before
            summary.pass = events.screenUp !== undefined && events.screenUp < 0.5 && events.screenDown !== undefined &&
                events.landed !== undefined && events.screenDown >= events.landed - 0.2 &&
                events.screenDown <= events.landed + 11 && gaps === 0;
            print("WORLD-LOADING-HOP summary " + JSON.stringify(summary));
            Script.setTimeout(function () { Test.quit(); }, 1500);
            return;
        }
        Script.setTimeout(sample, 100);
    }

    Script.setTimeout(function () {
        start = Date.now();
        Messages.sendLocalMessage("Darkride-Hop", JSON.stringify({ type: "start", world: "Tivoli Town", author: "Caitlyn",
            picture: "https://vr.darkride.ai/api/domain/test/image" }));
        // HOP_DELAY (ms, default 3000): how long after the hop's start the client goes; Explore goes at once (0), the
        // 3 s are to see the screen cover the old world first
        var delay = typeof HOP_DELAY !== "undefined" ? HOP_DELAY : 3000;
        print("WORLD-LOADING-HOP hop to Tivoli Town started; going to " + URL + " in " + (delay / 1000) + " s");
        Script.setTimeout(function () { Window.takeSnapshot(false, false, 0); }, Math.min(1500, delay));
        Script.setTimeout(function () { mark("goTo"); location.handleLookupString(URL); }, delay);
        // PICTURES=1 (run.sh): snapshots of the loading screen at 2.5 s and 12 s after the hop, into the run's folder
        // (a snapshot takes in the desktop screen; Test.saveWindowPicture comes out white, the window being the GPU's)
        if (typeof HOP_PICTURES !== "undefined") {
            [2500, 12000].forEach(function (ms) {
                Script.setTimeout(function () {
                    Window.takeSnapshot(false, false, 0);
                    print("WORLD-LOADING-HOP snapshot at " + (ms / 1000) + " s after the hop");
                }, delay + ms);
            });
        }
        // HOP_PRESS="<button>@<s>" (run.sh): press a loading screen button that long after the hop, as a click would
        // (Window.pressLoadingScreenButton: "cancel" goes back where the hop came from, "showNow" stops waiting for the
        // whole world); what the screen offered then is printed, and where the client is when the screen lifts
        if (typeof HOP_PRESS !== "undefined") {
            var press = HOP_PRESS.split("@");
            Script.setTimeout(function () {
                print("WORLD-LOADING-HOP pressing " + press[0] + " at " + t().toFixed(2) + " s; the screen offers " +
                    JSON.stringify(lastScreen && { canCancel: lastScreen.canCancel, canShowNow: lastScreen.canShowNow }));
                Window.pressLoadingScreenButton(press[0]);
            }, delay + Number(press[1]) * 1000);
        }
        sample();
    }, 10000);
}());
