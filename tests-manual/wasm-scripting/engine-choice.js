//
//  engine-choice.js
//  tests-manual/wasm-scripting
//
//  The engine chosen for one script (the Running Scripts window's choice): loads engine-choice-helper.js, moves it
//  to the WebAssembly engine and back with ScriptDiscoveryService.setRunOnWebAssembly, and prints the engine
//  getRunning reports at each step, with the helper's own line. Run with the setting off:
//
//    DARKRIDE_RUN_JS_ON_WASM=0 tests-manual/wasm-scripting/run-client.sh engine-choice 90 \
//        --url file://$PWD/tests-manual/wasm-scripting/world.json --testScript $PWD/tests-manual/wasm-scripting/engine-choice.js
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var HELPER = Script.resolvePath("engine-choice-helper.js");

    function helperEntry() {
        var running = ScriptDiscoveryService.getRunning();
        for (var i = 0; i < running.length; i++) {
            if (running[i].name === "engine-choice-helper.js") {
                return running[i];
            }
        }
        return null;
    }
    function report(step) {
        var entry = helperEntry();
        print("ENGINE-CHOICE " + step + ": " + (entry ? entry.engine : "not running"));
    }

    var steps = [
        function () { ScriptDiscoveryService.loadOneScript(HELPER); },
        function () { report("loaded"); ScriptDiscoveryService.setRunOnWebAssembly(helperEntry().url, true); },
        function () { report("WebAssembly chosen"); ScriptDiscoveryService.setRunOnWebAssembly(helperEntry().url, false); },
        function () { report("QtScript chosen"); ScriptDiscoveryService.stopScript(helperEntry().url); },
        function () { report("stopped"); Test.quit(); }
    ];
    var step = 0;
    function next() {
        if (step < steps.length) {
            try {
                steps[step]();
            } catch (e) {
                print("ENGINE-CHOICE error at step " + step + ": " + e);
            }
            step++;
            Script.setTimeout(next, 3000);
        }
    }
    Script.setTimeout(next, 6000);
}());
