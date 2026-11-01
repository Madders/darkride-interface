//
//  walk-frames-watch.js
//  tests-manual/wasm-scripting
//
//  The watcher of walk-frames.sh: a second agent that sees walk-frames-body.js as anyone's client does, through
//  AvatarList, and looks for flashes in the body's joints: a joint that turns away by more than the clip ever turns
//  in one update (the body says how much: 45 degrees for the walk, 60 for the run), then straight back, ending nearer
//  where it started than either turn. A T-pose flash swings the arms 60 to 110 degrees out and back; a joint catching
//  up after a lost update, or the run's loop going from frame 19 back to 1, turns one way only. The first 1.5 s of each
//  phase is left out (the crossfade from the clip before). "WALK-FRAMES RESULT" says PASS when both loops from frame 0
//  show flashes (the flash is there and this test sees it) and both loops over the client's frame range show none.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "walk-frames-check";
    var SETTLE_MS = 1500;
    var phase = null;          // {name, jump, at, updates, flashes, joints: {name: count}}
    var phases = [];
    var last = null;           // the body's joint rotations at its last update
    var before = null;         // and at the update before that

    Agent.isAvatar = true;
    Avatar.displayName = "walk-frames-watch";
    Avatar.position = { x: 0, y: 1, z: -4 };

    function log(text) {
        print("WALK-FRAMES " + text);
    }

    function body() {
        var others = AvatarList.getAvatarIdentifiers().filter(function (id) {
            return !Uuid.isNull(id) && !Uuid.isEqual(id, Agent.sessionUUID);
        });
        return others.length === 1 ? AvatarList.getAvatar(others[0]) : null;
    }

    function degrees(a, b) {
        return Quat.angle(Quat.multiply(Quat.inverse(a), b)) * 180 / Math.PI;
    }

    function same(a, b) {
        if (a.length !== b.length) {
            return false;
        }
        for (var i = 0; i < a.length; i++) {
            if (a[i].x !== b[i].x || a[i].y !== b[i].y || a[i].z !== b[i].z || a[i].w !== b[i].w) {
                return false;
            }
        }
        return true;
    }

    function report() {
        var pass = phases.length === 4;
        phases.forEach(function (p) {
            var seconds = (p.end - p.at - SETTLE_MS) / 1000;
            var worst = Object.keys(p.joints).sort(function (a, b) {
                return p.joints[b] - p.joints[a];
            }).slice(0, 4).map(function (name) {
                return name + " " + p.joints[name];
            });
            log(p.name + ": " + p.updates + " updates in " + Math.round(seconds * 10) / 10 + " s, " + p.flashes +
                " flashes" + (worst.length ? " (" + worst.join(", ") + ")" : ""));
            var fromZero = / from 0$/.test(p.name);
            pass = pass && p.updates >= seconds * 10 && (fromZero ? p.flashes > 0 : p.flashes === 0);
        });
        log("RESULT " + (pass ? "PASS" : "FAIL"));
    }

    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(function (channel, message) {
        if (channel !== CHANNEL) {
            return;
        }
        var data = JSON.parse(message);
        if (phase) {
            phase.end = Date.now();
            phases.push(phase);
        }
        phase = null;
        if (data.phase === "done") {
            report();
        } else {
            phase = { name: data.phase, jump: data.jump, at: Date.now(), updates: 0, flashes: 0, joints: {} };
        }
    });

    Script.update.connect(function () {
        var avatar = body();
        if (!avatar) {
            return;
        }
        var rotations = avatar.getJointRotations();
        if (last && same(last, rotations)) {
            return;
        }
        var first = before;
        var middle = last;
        before = last;
        last = rotations;
        if (!phase || !first || Date.now() - phase.at < SETTLE_MS || first.length !== rotations.length ||
            middle.length !== rotations.length) {
            return;
        }
        phase.updates++;
        var names = avatar.getJointNames();
        var flashed = false;
        for (var i = 0; i < rotations.length; i++) {
            var away = degrees(first[i], middle[i]);
            var back = degrees(middle[i], rotations[i]);
            if (away > phase.jump && back > phase.jump && degrees(first[i], rotations[i]) < Math.min(away, back)) {
                var name = names[i] || "joint " + i;
                phase.joints[name] = (phase.joints[name] || 0) + 1;
                flashed = true;
            }
        }
        if (flashed) {
            phase.flashes++;
        }
    });
}());
