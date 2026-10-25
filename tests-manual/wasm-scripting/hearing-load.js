//
//  hearing-load.js
//  tests-manual/wasm-scripting
//
//  The stand-in agent of hearing-load.sh: an AI resident's body's network load without the resident. It takes the
//  heard audio and posts it fire-and-forget by XMLHttpRequest (as body.js's hearing does, its rate times RATE),
//  exchanges a JSON perception with a reply handler every second, and looks around (findEntities and
//  getMultipleEntityProperties) every two seconds. Every 30 s a LOAD line counts the requests and, on a guest that
//  has __guestMemory, the guest's heap. The posts go to http-echo.py on 127.0.0.1:43110, nobody's bridge.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var ECHO = "http://127.0.0.1:43110/echo";
    var RATE = 10;  // times body.js's rate (a take about 3 times a second), until hearing-load.sh's rate file says
    var HEAR_INTERVAL = 1 / (3 * RATE);
    var rateRequest = new XMLHttpRequest();
    rateRequest.onreadystatechange = function () {
        if (rateRequest.readyState === rateRequest.DONE && rateRequest.status === 200) {
            var rate = parseFloat(rateRequest.responseText);
            if (rate > 0) {
                RATE = rate;
                HEAR_INTERVAL = 1 / (3 * RATE);
            }
            print("LOAD rate " + RATE);
        }
    };
    rateRequest.open("GET", Script.resolvePath("results/hearing-load-rate.txt"), true);
    rateRequest.send();
    var SYNTHETIC = new ArrayBuffer(16000);  // a take's size, posted when nothing was heard
    var started = Date.now();
    var hearTimer = 0, exchangeTimer = 0, lookTimer = 0, reportTimer = 0;
    var posts = 0, exchanges = 0, replies = 0, looks = 0, lookFailures = 0, postedBytes = 0;

    Agent.isAvatar = true;
    Agent.isListeningToAudioStream = true;
    Avatar.displayName = "hearing-load agent";
    Avatar.position = { x: 0, y: 1, z: 0 };
    EntityViewer.setPosition({ x: 0, y: 0, z: 0 });
    EntityViewer.setCenterRadius(1000);
    EntityViewer.queryOctree();

    function report() {
        var line = "LOAD t=" + Math.round((Date.now() - started) / 1000) + "s posts=" + posts + " postedMB=" +
            Math.round(postedBytes / 1e6) + " exchanges=" + exchanges + " replies=" + replies + " looks=" + looks +
            " lookFailures=" + lookFailures;
        if (typeof __guestMemory === "function") {
            if (typeof gc === "function") { gc(); }
            var m = __guestMemory();
            line += " guestUsedMB=" + Math.round(m.memoryUsedSize / 1e6) + " guestMallocMB=" + Math.round(m.mallocSize / 1e6) +
                " objects=" + m.objects;
        }
        print(line);
    }

    Script.update.connect(function (dt) {
        hearTimer += dt;
        exchangeTimer += dt;
        lookTimer += dt;
        reportTimer += dt;
        // hear, as body.js does: take, and post what was taken with no handler
        while (hearTimer >= HEAR_INTERVAL) {
            hearTimer -= HEAR_INTERVAL;
            var pcm = Agent.takeHeardAudio();
            var body = pcm && pcm.byteLength > 0 ? pcm : SYNTHETIC;
            var hreq = new XMLHttpRequest();
            hreq.open("POST", ECHO, true);
            hreq.setRequestHeader("Content-Type", "application/octet-stream");
            hreq.send(body);
            posts++;
            postedBytes += body.byteLength;
        }
        // exchange, as body.js does: a JSON body and a reply handler
        if (exchangeTimer >= 1) {
            exchangeTimer = 0;
            var req = new XMLHttpRequest();
            req.onreadystatechange = function () {
                if (req.readyState !== req.DONE) { return; }
                try { JSON.parse(req.responseText); replies++; } catch (e) { }
            };
            req.open("POST", ECHO, true);
            req.setRequestHeader("Content-Type", "application/json");
            req.send(JSON.stringify({ heard: posts, at: Date.now(), avatar: Avatar.position }));
            exchanges++;
        }
        // look: the entities around, by name and position
        if (lookTimer >= 2) {
            lookTimer = 0;
            try {
                var ids = Entities.findEntities({ x: 0, y: 0, z: 0 }, 1000);
                Entities.getMultipleEntityProperties(ids, ["name", "position", "type", "parentID"]);
                looks++;
            } catch (e) {
                lookFailures++;
                print("LOAD the look failed: " + e);
            }
            EntityViewer.queryOctree();
        }
        if (reportTimer >= 30) {
            reportTimer = 0;
            report();
        }
    });
    report();
}());
