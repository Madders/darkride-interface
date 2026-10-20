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
    Messages.subscribe("Darkride-Hop");
    Messages.messageReceived.connect(function (channel, message) {
        var data = channel === "Darkride-Hop" ? JSON.parse(message) : {};
        if (data.type === "screen") {
            screenIsUp = data.up;
            print("WORLD-LOADING-HOP screen " + (data.up ? "up" : "down") + " \"" + data.status + "\" world \"" + data.world +
                  "\" by \"" + data.author + "\" picture \"" + data.picture + "\" at " + (start ? t().toFixed(2) + " s" : "start-up"));
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
        if ((events.screenDown !== undefined && t() > events.screenDown + 5) || t() > 120) {
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
        print("WORLD-LOADING-HOP hop to Tivoli Town started; going to " + URL + " in 3 s");
        Script.setTimeout(function () { Window.takeSnapshot(false, false, 0); }, 1500);
        Script.setTimeout(function () { mark("goTo"); location.handleLookupString(URL); }, 3000);
        sample();
    }, 10000);
}());
