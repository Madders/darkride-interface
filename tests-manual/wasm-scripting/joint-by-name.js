//
//  joint-by-name.js
//  tests-manual/wasm-scripting
//
//  The agent of joint-by-name.sh: sets and reads its own joints by name (Avatar.setJointRotation("RightArm", ...) and
//  the rest), each step printed before and after, so a step that hangs the script shows as the last "step" line with
//  no "done" after it. Each value set is read back by name. An unknown name changes nothing and reads the default.
//  Then a stress phase: the stock walk plays, so the avatar's own thread writes every joint each frame, while the
//  script reads joints by name in a tight loop and sets one each frame. A read by name that locks the joints twice can
//  hang when that writer comes in between; the phase prints its count every 2 s, so a hang shows as the count stopping.
//  "JOINT-BY-NAME RESULT" says PASS when every step and the stress phase finished and read back what was set.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var JOINT = "RightArm";
    var STRESS_SECONDS = 30;
    var STRESS_READS = 300;     // reads by name each frame
    var started = Date.now();
    var ran = false;
    var stressFrom = 0;
    var stressCalls = 0;
    var stressTold = 0;
    var stressLeg = null;       // the walking leg's last rotation, and how often it changed: the writer is busy
    var stressMoves = 0;
    var failures = [];

    Agent.isAvatar = true;
    Avatar.displayName = "joint-by-name";
    Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
    Avatar.position = { x: 0, y: 1, z: 0 };

    function log(text) {
        print("JOINT-BY-NAME " + text);
    }

    function degrees(a, b) {
        return Quat.angle(Quat.multiply(Quat.inverse(a), b)) * 180 / Math.PI;
    }

    function near(a, b) {
        return Vec3.distance(a, b) < 0.001;
    }

    function step(name, f) {
        log("step " + name);
        var problem = f();
        log("done " + name + (problem ? ": " + problem : ""));
        if (problem) {
            failures.push(name + ": " + problem);
        }
    }

    function run() {
        var index = Avatar.getJointIndex(JOINT);
        log("joint " + JOINT + " is " + index + " of " + Avatar.getJointNames().length);
        var turn = Quat.fromPitchYawRollDegrees(0, 0, 45);
        var turn2 = Quat.fromPitchYawRollDegrees(30, 0, 0);
        var move = { x: 0.1, y: 0.2, z: 0.3 };
        var move2 = { x: -0.1, y: 0.25, z: 0 };

        step("setJointRotation", function () {
            Avatar.setJointRotation(JOINT, turn);
            var back = Avatar.getJointRotation(JOINT);
            var byIndex = Avatar.getJointRotation(index);
            return degrees(back, turn) > 0.1 ? "read back " + JSON.stringify(back) :
                degrees(byIndex, turn) > 0.1 ? "by index " + JSON.stringify(byIndex) : null;
        });
        step("setJointTranslation", function () {
            Avatar.setJointTranslation(JOINT, move);
            var back = Avatar.getJointTranslation(JOINT);
            return near(back, move) ? null : "read back " + JSON.stringify(back);
        });
        step("setJointData", function () {
            Avatar.setJointData(JOINT, turn2, move2);
            var rotation = Avatar.getJointRotation(JOINT);
            var translation = Avatar.getJointTranslation(JOINT);
            return degrees(rotation, turn2) > 0.1 ? "rotation " + JSON.stringify(rotation) :
                !near(translation, move2) ? "translation " + JSON.stringify(translation) : null;
        });
        step("isJointDataValid", function () {
            return Avatar.isJointDataValid(JOINT) ? null : "false";
        });
        step("clearJointData", function () {
            Avatar.clearJointData(JOINT);
            return null;
        });
        step("unknown name", function () {
            Avatar.setJointRotation("NoSuchJoint", turn);
            var back = Avatar.getJointRotation("NoSuchJoint");
            return degrees(back, Quat.IDENTITY) > 0.1 ? "read " + JSON.stringify(back) : null;
        });
        log("step stress");
        Avatar.startAnimation(Script.resolvePath("../../interface/resources/avatar/animations/walk_fwd.fbx"), 30, 1,
            true, false, 1, 30);
        stressFrom = Date.now();
    }

    function stress() {
        var now = Date.now();
        var turn = Quat.fromPitchYawRollDegrees(0, (now % 3600) / 10, 0);
        Avatar.setJointRotation("LeftHand", turn);
        stressCalls++;
        for (var i = 0; i < STRESS_READS; i++) {
            Avatar.getJointRotation(JOINT);
            Avatar.getJointTranslation("Hips");
            Avatar.isJointDataValid("Head");
            stressCalls += 3;
        }
        var leg = Avatar.getJointRotation("RightUpLeg");
        if (stressLeg && degrees(stressLeg, leg) > 0.01) {
            stressMoves++;
        }
        stressLeg = leg;
        if (now - stressTold >= 2000) {
            stressTold = now;
            log("stress at " + Math.round((now - stressFrom) / 1000) + " s, " + stressCalls + " calls");
        }
        if (now - stressFrom >= STRESS_SECONDS * 1000) {
            stressFrom = 0;
            Avatar.stopAnimation();
            log("done stress: " + stressCalls + " calls, the leg moved in " + stressMoves + " frames");
            if (stressMoves < 5 * STRESS_SECONDS) {
                failures.push("stress: the walk wrote the joints in only " + stressMoves + " frames");
            }
            log("RESULT " + (failures.length ? "FAIL (" + failures.join("; ") + ")" : "PASS"));
        }
    }

    Script.update.connect(function () {
        if (stressFrom) {
            stress();
            return;
        }
        if (ran || Date.now() - started < 4000 || Avatar.getJointIndex(JOINT) < 0) {
            return;
        }
        ran = true;
        run();
    });
}());
