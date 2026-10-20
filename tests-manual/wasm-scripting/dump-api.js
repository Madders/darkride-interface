//
//  dump-api.js
//  tests-manual/wasm-scripting
//
//  Writes the API a script sees (WASM-SCRIPTING.md round 0) as JSON, one file per context: the client's own
//  context from this script, the client's entity-script context from an entity this script adds (its script is
//  dump-entity-api.js). The files land in results/ beside this script and are copied into darkride-ops api/.
//
//  Run (serverless):  tests-manual/wasm-scripting/dump-api.sh
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    function localPath(name) {
        return Script.resolvePath("results/" + name).replace(/^file:\/\//, "");
    }

    function start() {
        var ok = Script.dumpApi(localPath("api-client.json"));
        print("WASM-API client context " + (ok ? "written" : "FAILED"));
        var entityID = Entities.addEntity({
            type: "Box", name: "wasm-api-dump", position: { x: 0, y: 2, z: -3 }, dimensions: { x: 0.5, y: 0.5, z: 0.5 },
            color: { red: 40, green: 120, blue: 200 }, lifetime: 600,
            script: Script.resolvePath("dump-entity-api.js")
        });
        // the entity script's preload writes the entity context; give it a few seconds, then leave
        Script.setTimeout(function () {
            Entities.deleteEntity(entityID);
            print("WASM-API done");
            Script.setTimeout(function () { Test.quit(); }, 1000);
        }, 6000);
    }

    // wait for the client to settle (the serverless world loads, the avatar lands)
    Script.setTimeout(start, 8000);
}());
