//
//  dump-draws.js
//  tests-manual/instancing
//
//  In a serverless world (run.sh): waits for the world, faces HEADING degrees from the arrival point, then logs every
//  main-view draw for three frames with GPU instancing on and three with it off (Test.dumpDraws), and quits. The
//  client log's INSTANCING-DUMP lines are diffed by diff-dump.py: what one mode drew that the other did not.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var HEADING = typeof DUMP_HEADING !== "undefined" ? DUMP_HEADING : 90;
    var QUIET = typeof DUMP_QUIET !== "undefined" ? DUMP_QUIET : 8;       // s with nothing downloading: the world is in
    var LIMIT = typeof DUMP_LIMIT !== "undefined" ? DUMP_LIMIT : 240;     // s: dump anyway
    var start = Date.now();
    function say(text) { print("INSTANCING-TEST " + text + " at " + ((Date.now() - start) / 1000).toFixed(1) + " s"); }

    var faced = false;
    var quietSince = null;
    var timer = Script.setInterval(function () {
        if (!Window.isPhysicsEnabled()) {
            return;
        }
        if (!faced) {
            faced = true;
            say("physics on; facing " + HEADING + " and waiting for the downloads to end");
            MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, HEADING, 0);
        }
        Stats.forceUpdateStats();
        var busy = (Stats.processing || 0) + (Stats.processingPending || 0);
        var now = Date.now();
        if (busy > 0) {
            quietSince = null;
        } else if (quietSince === null) {
            quietSince = now;
        }
        var quiet = quietSince !== null && now - quietSince > QUIET * 1000;
        if (!quiet && now - start < LIMIT * 1000) {
            return;
        }
        Script.clearInterval(timer);
        say((quiet ? "downloads quiet" : "time limit") + "; triangles " + Stats.triangles);
        Script.setTimeout(function () {
            say("dump with instancing " + (Menu.isOptionChecked("GPU Instancing") ? "on" : "off") + ", draws " + Stats.apiDrawcalls + " of " + Stats.drawcalls);
            Test.dumpDraws(3);
            Script.setTimeout(function () {
                Menu.setIsOptionChecked("GPU Instancing", false);
                Script.setTimeout(function () {
                    say("dump with instancing " + (Menu.isOptionChecked("GPU Instancing") ? "on" : "off") + ", draws " + Stats.apiDrawcalls + " of " + Stats.drawcalls);
                    Test.dumpDraws(3);
                    Script.setTimeout(function () {
                        say("done; quitting");
                        Test.quit();
                    }, 2000);
                }, 2000);
            }, 2000);
        }, 2000);
    }, 250);
}());
