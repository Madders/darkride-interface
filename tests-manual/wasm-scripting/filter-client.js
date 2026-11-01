//
//  filter-client.js
//  tests-manual/wasm-scripting
//
//  The client of server-check.sh: joins the sandbox, where localhost may not change locks for the check, and edits a
//  box through the entity server's edit filter (server-edit-filter.js): a refused name, a height above 5. An
//  assignment client can't test the filter, since the domain gives every assignment client the right to change locks,
//  which bypasses it. The results are FILTER-CHECK lines; agent-check.js reports the box as the server keeps it.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    function report(name, value) { print("FILTER-CHECK " + name + " = " + JSON.stringify(value)); }
    var box;
    function start() {
        box = Entities.addEntity({ type: "Box", name: "server-check filter box", position: { x: -4, y: 1, z: -3 },
            dimensions: { x: 0.5, y: 0.5, z: 0.5 }, lifetime: 180 });
        report("added", typeof box);
        Script.setTimeout(function () {
            Entities.editEntity(box, { name: "forbidden" });
            Entities.editEntity(box, { position: { x: -4, y: 50, z: -3 } });
            // the entity server answers a refused or changed edit with its own version; watch for it here
            var seconds = 0, firstCorrect = -1;
            var watch = Script.setInterval(function () {
                seconds++;
                var p = Entities.getEntityProperties(box, ["name", "position"]);
                if (firstCorrect < 0 && p.name === "server-check filter box" && Math.round(p.position.y) === 5) {
                    firstCorrect = seconds;
                }
                if (seconds === 15) {
                    Script.clearInterval(watch);
                    report("the box as this client sees it", { name: p.name, y: Math.round(p.position.y) });
                    report("seconds until this client had the server's version", firstCorrect);
                    report("done", true);
                    Test.quit();
                }
            }, 1000);
        }, 3000);
    }
    var waited = 0;
    var timer = Script.setInterval(function () {
        waited++;
        if (Entities.canRez() || waited > 60) {
            Script.clearInterval(timer);
            report("rights", { canRez: Entities.canRez(), canAdjustLocks: Entities.canAdjustLocks() });
            start();
        }
    }, 500);
}());
