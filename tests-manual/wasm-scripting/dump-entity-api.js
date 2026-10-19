//
//  dump-entity-api.js
//  tests-manual/wasm-scripting
//
//  The entity script of dump-api.js: on preload it writes the entity-script context's API beside the client's.
//  On a client it writes api-entity_client.json; on an entity script server, api-entity_server.json.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    this.preload = function (entityID) {
        var context = Script.context;
        var path = Script.resolvePath("results/api-" + context + ".json").replace(/^file:\/\//, "");
        var ok = Script.dumpApi(path);
        print("WASM-API " + context + " context " + (ok ? "written" : "FAILED") + " for " + entityID);
    };
});
