//
//  scripts-off-driver.js
//  tests-manual/wasm-scripting
//
//  The agent of scripts-off.sh. Once the test client (scripts-off-client.js) says where its avatar stands, it adds a
//  box round the avatar with a client entity script and a server entity script, version A. Both tick 5 times a second,
//  edit the box on every tick (a script that keeps editing its entity, whose own edits are newer than the switch) and
//  report on the "scripts-off" channel. Then it turns the box's scripts off (scriptsEnabled), swaps both scripts for
//  version B while they're off, turns them on again, tries to turn them off while the box is locked, and last has the
//  server script turn them off itself. Beside it, a second box's server script bumps its own scriptTimestamp as it
//  loads (to have clients reload), which must not reload itself. Prints SCRIPTS-OFF lines and a RESULT.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "scripts-off";
    var CONTROL = "scripts-off-control";
    var NAME = "scripts-off box";
    var events = [];
    var failures = [];
    var box = null;
    // the driver, the entity script server and the client all run on the engine scripts-off.sh picked
    var onNewEngine = typeof Symbol === "function";

    function log(text) {
        print("SCRIPTS-OFF " + text);
    }

    function check(what, ok, detail) {
        log((ok ? "ok   " : "FAIL ") + what + (detail === undefined ? "" : ": " + JSON.stringify(detail)));
        if (!ok) {
            failures.push(what);
        }
    }

    // The entity scripts: no clean-up of their own in unload, so a timer still ticking after unload is the engine's
    function entityScript(who, version) {
        var edited = who === "server" ? "description" : "userData";
        return "(function () {" +
            "  var id = null, ticks = 0;" +
            "  this.remotelyCallable = ['selfOff'];" +
            "  function say(event) { Messages.sendMessage('" + CHANNEL + "', JSON.stringify({ who: '" + who +
            "', version: '" + version + "', event: event, n: ticks })); }" +
            "  this.preload = function (entityID) {" +
            "    id = entityID;" +
            "    say('preload');" +
            "    Script.setInterval(function () { ticks++; Entities.editEntity(id, { " + edited + ": '" + who + " " +
            version + " ' + ticks }); say('tick'); }, 200);" +
            "  };" +
            "  this.enterEntity = function () { say('enter'); };" +
            "  this.leaveEntity = function () { say('leave'); };" +
            "  this.selfOff = function () { say('selfOff'); Entities.editEntity(id, { scriptsEnabled: false }); };" +
            "  this.unload = function () { say('unload'); };" +
            "})";
    }

    // One preload of a script. QtScript can run a script twice when two loads of it queue up before it has loaded
    // (an entity's arrival and a change of its script), as two objects with their own timers; the new engine's
    // loadEntityScript drops the second. Noted on QtScript, not counted against scripts off.
    function checkLoadedOnce(who, version, from) {
        var loads = seen(who, version, "preload", from).length;
        if (!onNewEngine && loads === 2) {
            log("note " + who + " " + version + " preloaded twice: QtScript's two queued loads of one script");
            return;
        }
        check(who + " " + version + " preloaded once", loads === 1, loads);
    }

    // what arrived from `who` (version, event; any when undefined) between two times
    function seen(who, version, event, from, to) {
        return events.filter(function (e) {
            return e.who === who && (version === undefined || e.version === version) &&
                (event === undefined || e.event === event) && e.at >= from && e.at < (to || Infinity);
        });
    }

    function scriptsEnabled() {
        var p = Entities.getEntityProperties(box, ["scriptsEnabled"]);
        return p ? p.scriptsEnabled : undefined;
    }

    Messages.subscribe(CHANNEL);
    Messages.subscribe(CONTROL);
    Messages.messageReceived.connect(function (channel, message) {
        var m;
        try {
            m = JSON.parse(message);
        } catch (e) {
            return;
        }
        if (channel === CHANNEL) {
            m.at = Date.now();
            events.push(m);
            if (m.event !== "tick") {
                log(m.who + " " + m.version + " " + m.event + " after " + m.n + " ticks");
            }
        } else if (channel === CONTROL && m.here && !box) {
            start(m.here);
        }
    });

    // the agent's own view of the entities, for reading the box back
    EntityViewer.setPosition({ x: 0, y: 0, z: 0 });
    EntityViewer.setCenterRadius(1000);
    Script.setInterval(function () {
        EntityViewer.queryOctree();
    }, 1000);

    // leftovers of an earlier run
    Script.setTimeout(function () {
        var near = Entities.findEntities({ x: 0, y: 0, z: 0 }, 1000);
        for (var i = 0; i < near.length; i++) {
            var p = Entities.getEntityProperties(near[i], ["name", "locked"]);
            if (p && p.name === NAME) {
                Entities.editEntity(near[i], { locked: false });
                Entities.deleteEntity(near[i]);
                log("removed a box of an earlier run");
            }
        }
    }, 3000);

    function start(here) {
        var t0 = Date.now();
        box = Entities.addEntity({
            type: "Box", name: NAME, position: here, dimensions: { x: 6, y: 6, z: 6 }, collisionless: true,
            color: { red: 200, green: 120, blue: 60 }, alpha: 0.3, lifetime: 300,
            script: entityScript("client", "A"), serverScripts: entityScript("server", "A")
        });
        log("added " + box + " round the client at " + JSON.stringify(here));
        var stamp = Entities.addEntity({
            type: "Box", name: NAME, position: Vec3.sum(here, { x: 0, y: 5, z: 0 }), dimensions: { x: 0.5, y: 0.5, z: 0.5 },
            lifetime: 300, serverScripts: "(function () {" +
                "  this.preload = function (id) {" +
                "    Messages.sendMessage('" + CHANNEL + "', JSON.stringify({ who: 'stamp', version: 'S', event: 'preload', n: 0 }));" +
                "    Entities.editEntity(id, { scriptTimestamp: Date.now() });" +
                "  };" +
                "})"
        });

        var offAt, onAt, lockAt, selfAt;
        Script.setTimeout(function () {
            ["client", "server"].forEach(function (who) {
                checkLoadedOnce(who, "A", t0);
                check(who + " A ticks", seen(who, "A", "tick", t0).length > 10, seen(who, "A", "tick", t0).length);
            });
            check("client A entered", seen("client", "A", "enter", t0).length >= 1);
            checkLoadedOnce("stamp", "S", t0);
            check("scriptsEnabled reads true", scriptsEnabled() === true, scriptsEnabled());
            offAt = Date.now();
            Entities.editEntity(box, { scriptsEnabled: false });
            log("scripts off");
        }, 12000);

        // while they're off, both scripts change: nothing may load
        Script.setTimeout(function () {
            Entities.editEntity(box, { script: entityScript("client", "B"), serverScripts: entityScript("server", "B") });
            log("both scripts swapped for B while off");
        }, 15000);

        Script.setTimeout(function () {
            ["client", "server"].forEach(function (who) {
                var unloads = seen(who, "A", "unload", offAt);
                check(who + " A unloaded once", unloads.length === 1, unloads.length);
                var late = seen(who, undefined, "tick", offAt + 2000);
                check(who + " no ticks from 2 s after off", late.length === 0, late.length);
                var loads = seen(who, undefined, "preload", offAt);
                check(who + " nothing loaded while off (URL changed meanwhile)", loads.length === 0, loads.length);
            });
            var leave = seen("client", "A", "leave", offAt)[0];
            var unload = seen("client", "A", "unload", offAt)[0];
            check("client A left before its unload", leave && unload && leave.at <= unload.at && events.indexOf(leave) <
                events.indexOf(unload));
            check("scriptsEnabled reads false", scriptsEnabled() === false, scriptsEnabled());
            onAt = Date.now();
            Entities.editEntity(box, { scriptsEnabled: true });
            log("scripts on");
        }, 22000);

        Script.setTimeout(function () {
            ["client", "server"].forEach(function (who) {
                checkLoadedOnce(who, "B", onAt);
                check(who + " B ticks", seen(who, "B", "tick", onAt).length > 10, seen(who, "B", "tick", onAt).length);
                check(who + " no A after on", seen(who, "A", undefined, onAt).length === 0);
            });
            check("client B entered", seen("client", "B", "enter", onAt).length >= 1);
            check("scriptsEnabled reads true again", scriptsEnabled() === true, scriptsEnabled());
            // locked: only `locked` itself may change, the same for this switch as for any other property
            Entities.editEntity(box, { locked: true });
            log("locked");
        }, 32000);

        Script.setTimeout(function () {
            lockAt = Date.now();
            Entities.editEntity(box, { scriptsEnabled: false });
            log("tried scripts off while locked");
        }, 34000);

        Script.setTimeout(function () {
            check("locked: scriptsEnabled still true", scriptsEnabled() === true, scriptsEnabled());
            ["client", "server"].forEach(function (who) {
                check("locked: " + who + " B still ticks", seen(who, "B", "tick", lockAt + 2000).length > 10,
                    seen(who, "B", "tick", lockAt + 2000).length);
                check("locked: " + who + " B not unloaded", seen(who, "B", "unload", lockAt).length === 0);
            });
            Entities.editEntity(box, { locked: false });
        }, 40000);

        // a server script turning its own entity's scripts off: its own edit comes back from the entity server with
        // nothing changed, so the entity script server goes by the edit itself
        Script.setTimeout(function () {
            selfAt = Date.now();
            Entities.callEntityServerMethod(box, "selfOff", []);
            log("asked the server script to turn the scripts off itself");
        }, 42000);

        Script.setTimeout(function () {
            check("self: server B asked", seen("server", "B", "selfOff", selfAt).length === 1);
            ["client", "server"].forEach(function (who) {
                check("self: " + who + " B unloaded once", seen(who, "B", "unload", selfAt).length === 1,
                    seen(who, "B", "unload", selfAt).length);
                var late = seen(who, undefined, "tick", selfAt + 2000);
                check("self: " + who + " no ticks from 2 s after", late.length === 0, late.length);
            });
            check("self: scriptsEnabled reads false", scriptsEnabled() === false, scriptsEnabled());
        }, 48000);

        Script.setTimeout(function () {
            Entities.deleteEntity(box);
            Entities.deleteEntity(stamp);
            Messages.sendMessage(CONTROL, JSON.stringify({ done: true }));
            log("RESULT " + (failures.length ? "FAIL " + failures.join("; ") : "PASS") + " (" + events.length +
                " messages)");
        }, 49000);
    }
}());
