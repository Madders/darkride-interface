//
//  parent-check-body.js
//  tests-manual/wasm-scripting
//
//  The bot of parent-check.sh: an agent away from the origin adds two boxes, one with its own avatar as parent (2 m
//  above it) and one with no parent at the same place, then walks 6 m along x. It tells parent-check-watch.js the
//  boxes' ids on the "parent-check" channel, and prints what its own entity tree says about them.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "parent-check";
    var START = { x: 60, y: 1, z: 60 };
    var MOVED = { x: 66, y: 1, z: 60 };
    var started = Date.now();
    var added = 0;
    var moved = 0;
    var child = null;
    var control = null;

    Agent.isAvatar = true;
    Avatar.displayName = "parent-check";
    Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
    Avatar.position = START;
    Avatar.orientation = Quat.IDENTITY;
    EntityViewer.setPosition(START);
    EntityViewer.setCenterRadius(20);
    EntityViewer.queryOctree();
    Script.setInterval(function () {
        EntityViewer.setPosition(Avatar.position);
        EntityViewer.queryOctree();
    }, 1000);

    function log(text) {
        print("PARENT-CHECK body " + text);
    }

    function describe(id) {
        var p = Entities.getEntityProperties(id, ["name", "position", "parentID", "queryAACube"]);
        return p && p.name ? p.name + " at " + JSON.stringify(p.position) + ", parent " + p.parentID + ", cube " +
            JSON.stringify(p.queryAACube) : "unknown here";
    }

    function tell() {
        Messages.sendMessage(CHANNEL, JSON.stringify({ child: child, control: control, at: Avatar.position }));
    }

    Script.update.connect(function () {
        var now = Date.now();
        if (!added && now - started > 4000 && Avatar.getJointIndex("Hips") >= 0) {
            added = now;
            log("session " + Agent.sessionUUID + " at " + JSON.stringify(Avatar.position));
            child = Entities.addEntity({ type: "Box", name: "parent-check child", parentID: Agent.sessionUUID,
                localPosition: { x: 0, y: 2, z: 0 }, dimensions: { x: 0.3, y: 0.3, z: 0.3 } });
            control = Entities.addEntity({ type: "Box", name: "parent-check control",
                position: Vec3.sum(START, { x: 0, y: 2, z: 0 }), dimensions: { x: 0.3, y: 0.3, z: 0.3 } });
            log("added child " + child + ", control " + control);
            tell();
        }
        if (added && !moved && now - added > 12000) {
            moved = now;
            Avatar.position = MOVED;
            log("moved to " + JSON.stringify(MOVED));
            tell();
        }
        if (added && (now - added) % 4000 < 20) {
            log("child: " + describe(child) + "; control: " + describe(control));
        }
    });

    Script.scriptEnding.connect(function () {
        if (child) {
            Entities.deleteEntity(child);
            Entities.deleteEntity(control);
        }
    });
}());
