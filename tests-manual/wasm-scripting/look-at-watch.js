//
//  look-at-watch.js
//  tests-manual/wasm-scripting
//
//  The watcher of look-at.sh: a second agent that sees look-at-body.js as anyone's client does, through AvatarList.
//  Five seconds into each of the body's phases it takes the body's joint rotations and its lookAtPosition: where the
//  eyes aim, the joint the body posed by hand, and how far the head joint has turned from the head phase.
//  "LOOK-AT RESULT" says PASS when the joint posed by hand keeps its pose throughout; the eyes look ahead at first;
//  then at the left point while the head turns toward it; then at the right point with the head back (about 45
//  degrees from where it was); then ahead again.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "look-at-check";
    var checks = [];
    var info = null;

    Agent.isAvatar = true;
    Avatar.displayName = "look-at-watch";
    Avatar.position = { x: 0, y: 1, z: -6 };

    function log(text) {
        print("LOOK-AT " + text);
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

    function rounded(v) {
        return "(" + [v.x, v.y, v.z].map(function (n) { return Math.round(n * 100) / 100; }).join(", ") + ")";
    }

    function near(a, b, metres) {
        return Vec3.distance(a, b) < metres;
    }

    var headBefore = null;   // the head joint's rotation while it turns toward the left point
    var allBefore = null;

    function sample(phase) {
        var avatar = body();
        if (!avatar) {
            log(phase + ": no body seen");
            checks.push(false);
            return;
        }
        var rotations = avatar.getJointRotations();
        var lookAt = avatar.lookAtPosition;
        var ahead = { x: info.hips.x, y: info.hips.y + 0.65, z: info.hips.z - 3 };
        var handPose = info.posed && info.posed.index < rotations.length ?
            Math.round(degrees(info.posed.rotation, rotations[info.posed.index])) : null;
        var head = info.head >= 0 && info.head < rotations.length ? rotations[info.head] : null;
        // how far the head turned back between the head phase and this one (the head is turned in the head phase,
        // ahead in the others): about 45 degrees in the eyes phase, about none after
        var turnedBack = headBefore && head ? Math.round(degrees(headBefore, head)) : null;
        if (phase === "head") {
            headBefore = head;
            allBefore = rotations;
        } else if (allBefore) {
            var moved = [];
            for (var i = 0; i < rotations.length && i < allBefore.length; i++) {
                var angle = Math.round(degrees(allBefore[i], rotations[i]));
                if (angle > 5) {
                    moved.push(i + ":" + angle);
                }
            }
            log(phase + ": joints turned since the head phase (index:degrees): " + moved.join(" "));
        }
        var kept = handPose !== null && handPose < 2;
        var pass = {
            ahead: near(lookAt, ahead, 1.0) && kept,
            head: near(lookAt, info.left, 0.1) && kept && head !== null,
            eyes: near(lookAt, info.right, 0.1) && kept && turnedBack > 35 && turnedBack < 55,
            released: near(lookAt, ahead, 1.0) && kept && turnedBack > 35 && turnedBack < 55
        }[phase];
        log(phase + ": " + rotations.length + " joints; lookAt " + rounded(lookAt) + "; the hand-posed joint " +
            handPose + " degrees from its pose; the head " + (turnedBack === null ? "-" : turnedBack) +
            " degrees from where it was in the head phase -> " + (pass ? "ok" : "WRONG"));
        checks.push(pass);
    }

    Messages.subscribe(CHANNEL);
    Messages.messageReceived.connect(function (channel, message) {
        if (channel !== CHANNEL) {
            return;
        }
        var data = JSON.parse(message);
        info = data;
        if (data.phase === "done") {
            var all = checks.length === 4 && checks.every(function (ok) { return ok; });
            log("RESULT " + (all ? "PASS" : "FAIL") + " (" + checks.length + " phases checked)");
            return;
        }
        Script.setTimeout(function () { sample(data.phase); }, data.phase === "ahead" ? 2000 : 5000);
    });
}());
