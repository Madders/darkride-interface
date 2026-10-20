//
//  baseline.js
//  tests-manual/wasm-scripting
//
//  Round 0 of WASM-SCRIPTING.md: the scripting engine as it stands, measured, so that the WebAssembly engine of
//  round 3 can be compared on the same script. Each test loops long enough for the clock's millisecond
//  resolution not to matter and reports a rate. The engine's own speed (plain arithmetic, objects, strings,
//  arrays) is measured apart from the cost of reaching the engine's objects (a call, a property, an entity).
//  Lines starting with WASM-BASELINE are the summary; the numbers are written with Test.saveObject when the
//  client was started with --testResultsLocation.
//
//  Run (serverless, no domain needed):
//    tests-manual/wasm-scripting/run.sh [path/to/Darkride.app] [results dir]
//
//  Written in ES5 on purpose: it must run on the engine it measures.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var ENTITY_POSITION = { x: 0, y: 2, z: -3 };
    var UPDATE_FRAMES = 600;        // the update test: ten seconds at the engine's 60 Hz
    var TIMER_COUNT = 50;           // the timer test: setTimeout(10 ms), one after another
    var TIMER_MS = 10;

    var results = {
        engine: (typeof Symbol === "function" && typeof Promise === "function") ? "ES2015 or later" : "ES5 (QtScript)",
        started: new Date().toISOString(),
        tests: {}
    };

    function log(line) { print("WASM-BASELINE " + line); }
    function now() { return Date.now(); }

    // Runs body(i) `iterations` times and records the rate. `check` is a value the loop produced, kept so an
    // engine cannot optimise the work away and so the two engines can be seen to compute the same thing.
    function measure(name, iterations, body) {
        var t0 = now();
        var check = body(iterations);
        var ms = now() - t0;
        var usPerCall = ms * 1000 / iterations;
        results.tests[name] = { iterations: iterations, ms: ms, usPerCall: usPerCall, perSecond: ms > 0 ? Math.round(iterations * 1000 / ms) : null, check: check };
        log(name + ": " + iterations + " in " + ms + " ms, " + usPerCall.toFixed(2) + " us each" + (check !== undefined ? " (check " + check + ")" : ""));
    }

    function stats(values) {
        var n = values.length, sum = 0, min = Infinity, max = -Infinity, i;
        for (i = 0; i < n; i++) { sum += values[i]; if (values[i] < min) { min = values[i]; } if (values[i] > max) { max = values[i]; } }
        var mean = sum / n, sq = 0;
        for (i = 0; i < n; i++) { sq += (values[i] - mean) * (values[i] - mean); }
        return { n: n, mean: mean, sd: Math.sqrt(sq / n), min: min, max: max };
    }

    // --- the engine alone -----------------------------------------------------------------------------------

    function engineTests() {
        measure("js-arithmetic", 2000000, function (n) {
            var a = 1.0, i;
            for (i = 0; i < n; i++) { a = (a * 1.000001 + i) % 1000; }
            return Math.round(a);
        });
        measure("js-objects", 300000, function (n) {
            var sum = 0, i, v;
            for (i = 0; i < n; i++) { v = { x: i, y: i * 2, z: 0 }; sum += v.x + v.y; }
            return sum;
        });
        measure("js-strings", 100000, function (n) {
            var s = "", i, len = 0;
            for (i = 0; i < n; i++) { s = "entity-" + i + "-" + (i % 7); len += s.length; }
            return len;
        });
        measure("js-arrays", 200000, function (n) {
            var arr = [], i;
            for (i = 0; i < n; i++) { arr.push((i * 7919) % 1000); }
            arr.sort(function (a, b) { return a - b; });
            return arr[n - 1];
        });
        measure("js-function-calls", 1000000, function (n) {
            function add(a, b) { return a + b; }
            var s = 0, i;
            for (i = 0; i < n; i++) { s = add(s, i & 7); }
            return s;
        });
        measure("js-json", 2000, function (n) {
            var obj = { position: { x: 1.5, y: 2.5, z: -3.5 }, rotation: { x: 0, y: 0, z: 0, w: 1 }, name: "box", list: [1, 2, 3, 4, 5, 6, 7, 8], nested: { a: { b: { c: "deep" } } } };
            var len = 0, i;
            for (i = 0; i < n; i++) { len += JSON.stringify(JSON.parse(JSON.stringify(obj))).length; }
            return len;
        });
    }

    // --- reaching the engine's objects ----------------------------------------------------------------------

    function hostTests(entityID) {
        var step = { x: 0.001, y: 0.002, z: 0.003 };
        measure("host-Vec3.sum", 100000, function (n) {
            var v = { x: 0, y: 0, z: 0 }, i;
            for (i = 0; i < n; i++) { v = Vec3.sum(v, step); }
            return Math.round(v.z * 1000);
        });
        measure("host-Vec3.length", 100000, function (n) {
            var v = { x: 3, y: 4, z: 12 }, s = 0, i;
            for (i = 0; i < n; i++) { s += Vec3.length(v); }
            return Math.round(s);
        });
        measure("host-Quat.multiply", 50000, function (n) {
            var q = { x: 0, y: 0, z: 0, w: 1 }, r = Quat.fromPitchYawRollDegrees(0, 1, 0), i;
            for (i = 0; i < n; i++) { q = Quat.multiply(q, r); }
            return Math.round(q.w * 1000);
        });
        measure("host-Uuid.generate", 50000, function (n) {
            var len = 0, i;
            for (i = 0; i < n; i++) { len += String(Uuid.generate()).length; }
            return len;
        });
        measure("host-MyAvatar.position-get", 100000, function (n) {
            var s = 0, i, p;
            for (i = 0; i < n; i++) { p = MyAvatar.position; s += p.y; }
            return Math.round(s);
        });
        measure("host-MyAvatar.position-set", 10000, function (n) {
            var p = MyAvatar.position, i;
            for (i = 0; i < n; i++) { MyAvatar.position = p; }
            return 0;
        });
        measure("host-Entities.getEntityProperties-all", 2000, function (n) {
            var count = 0, i, props;
            for (i = 0; i < n; i++) { props = Entities.getEntityProperties(entityID); count += props.position ? 1 : 0; }
            return count;
        });
        measure("host-Entities.getEntityProperties-position", 20000, function (n) {
            var s = 0, i, props;
            for (i = 0; i < n; i++) { props = Entities.getEntityProperties(entityID, ["position"]); s += props.position.y; }
            return Math.round(s);
        });
        measure("host-Entities.editEntity-position", 5000, function (n) {
            var i;
            for (i = 0; i < n; i++) { Entities.editEntity(entityID, { position: { x: ENTITY_POSITION.x, y: ENTITY_POSITION.y + (i % 10) * 0.001, z: ENTITY_POSITION.z } }); }
            return 0;
        });
        measure("host-JSON.stringify-entity", 1000, function (n) {
            var props = Entities.getEntityProperties(entityID), len = 0, i;
            for (i = 0; i < n; i++) { len += JSON.stringify(props).length; }
            return len / n;
        });
        measure("host-Script.resolvePath", 20000, function (n) {
            var len = 0, i;
            for (i = 0; i < n; i++) { len += String(Script.resolvePath("x/y.js")).length; }
            return len;
        });
    }

    // --- the update loop and the timers ---------------------------------------------------------------------

    var updateDts = [], updateWall = [], lastWall = 0, updateHandler = null, firstUpdate = true;

    function updateTest(done) {
        updateHandler = function (dt) {
            var t = now();
            if (firstUpdate) {
                // the first update after the synchronous tests carries their whole duration as its dt: not a frame
                firstUpdate = false;
                lastWall = t;
                return;
            }
            updateDts.push(dt * 1000);
            updateWall.push(t - lastWall);
            lastWall = t;
            if (updateDts.length >= UPDATE_FRAMES) {
                Script.update.disconnect(updateHandler);
                var d = stats(updateDts), w = stats(updateWall);
                results.tests["update-dt-ms"] = d;
                results.tests["update-wall-ms"] = w;
                log("update: " + d.n + " frames, dt mean " + d.mean.toFixed(2) + " ms sd " + d.sd.toFixed(2) + " min " + d.min.toFixed(2) + " max " + d.max.toFixed(2) +
                    "; wall mean " + w.mean.toFixed(2) + " ms sd " + w.sd.toFixed(2) + " max " + w.max.toFixed(1));
                done();
            }
        };
        Script.update.connect(updateHandler);
    }

    function timerTest(done) {
        var delays = [], remaining = TIMER_COUNT;
        function one() {
            var t0 = now();
            Script.setTimeout(function () {
                delays.push(now() - t0);
                if (--remaining > 0) { one(); return; }
                var s = stats(delays);
                results.tests["timer-10ms-actual-ms"] = s;
                log("setTimeout(" + TIMER_MS + "): " + s.n + " timers, actual mean " + s.mean.toFixed(2) + " ms min " + s.min + " max " + s.max);
                done();
            }, TIMER_MS);
        }
        one();
    }

    // --- the run --------------------------------------------------------------------------------------------

    var entityID = null;

    function finish() {
        results.finished = new Date().toISOString();
        Test.saveObject(results, "wasm-baseline.json");
        if (entityID) { Entities.deleteEntity(entityID); }
        log("done");
        Script.setTimeout(function () { Test.quit(); }, 1000);
    }

    function start() {
        log("engine: " + results.engine);
        entityID = Entities.addEntity({
            type: "Box", name: "wasm-baseline", position: ENTITY_POSITION, dimensions: { x: 0.5, y: 0.5, z: 0.5 },
            color: { red: 200, green: 120, blue: 40 }, lifetime: 600
        });
        // give the entity a frame to exist, then run everything
        Script.setTimeout(function () {
            var props = Entities.getEntityProperties(entityID, ["name"]);
            if (props.name !== "wasm-baseline") { log("entity not found, host tests will fail"); }
            engineTests();
            hostTests(entityID);
            updateTest(function () { timerTest(finish); });
        }, 1000);
    }

    // wait for the client to settle (the serverless world loads, the avatar lands) before measuring
    Script.setTimeout(start, 8000);
}());
