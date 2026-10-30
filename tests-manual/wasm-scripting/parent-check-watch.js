//
//  parent-check-watch.js
//  tests-manual/wasm-scripting
//
//  The visitor of parent-check.sh: a second agent that sees entities as anyone's client does, through its own
//  EntityViewer query (20 m round the bot, so not the origin), and reports what the entity server sent it about the
//  bot's two boxes: the one parented to the bot and the one with no parent. "PARENT-CHECK RESULT" says PASS when the
//  parented box reaches the visitor 2 m above the bot, before and after the bot walks 6 m.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "parent-check";
    var ids = null;
    var botAt = null;
    var phase = 0;              // 1: the bot at its start, 2: after it walked
    var phaseAt = 0;
    var seen = {};              // phase: {child: bool, childNear: bool, control: bool}

    Agent.isAvatar = false;
    EntityViewer.setPosition({ x: 60, y: 1, z: 60 });
    EntityViewer.setCenterRadius(20);
    EntityViewer.queryOctree();

    function log(text) {
        print("PARENT-CHECK watch " + text);
    }

    function look() {
        var child = Entities.getEntityProperties(ids.child, ["name", "position", "parentID", "queryAACube"]);
        var control = Entities.getEntityProperties(ids.control, ["name", "position"]);
        var near = Entities.findEntities(botAt, 10).length;
        var want = Vec3.sum(botAt, { x: 0, y: 2, z: 0 });
        var known = !!(child && child.name);
        var childNear = known && Vec3.distance(child.position, want) < 0.5;
        var s = seen[phase] || (seen[phase] = {});
        s.child = s.child || known;
        s.childNear = s.childNear || childNear;
        s.control = s.control || !!(control && control.name);
        log("phase " + phase + ": child " + (known ? "at " + JSON.stringify(child.position) + " (want " +
            JSON.stringify(want) + "), parent " + child.parentID + ", cube " + JSON.stringify(child.queryAACube) :
            "not received") + "; control " + (control && control.name ? "received" : "not received") + "; " + near +
            " entities within 10 m of the bot");
    }

    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(function (channel, message) {
        if (channel !== CHANNEL) {
            return;
        }
        var data = JSON.parse(message);
        ids = data;
        botAt = data.at;
        phase++;
        phaseAt = Date.now();
        EntityViewer.setPosition(botAt);
        EntityViewer.queryOctree();
    });

    Script.setInterval(function () {
        EntityViewer.queryOctree();
        if (!ids) {
            return;
        }
        look();
        if (phase === 2 && Date.now() - phaseAt > 8000) {
            var pass = [1, 2].every(function (p) {
                return seen[p] && seen[p].childNear;
            });
            log("RESULT " + (pass ? "PASS" : "FAIL") + " " + JSON.stringify(seen));
            ids = null;
        }
    }, 2000);
}());
