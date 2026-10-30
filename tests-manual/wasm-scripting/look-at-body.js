//
//  look-at-body.js
//  tests-manual/wasm-scripting
//
//  The body of look-at.sh: an agent in the default pose that looks ahead, then turns its head toward a point 45 degrees
//  to its left (Avatar.setHeadLookAt, every frame), then looks at a point to its right with its eyes only
//  (Avatar.setEyesLookAt), then lets go. It tells look-at-watch.js each phase on the "look-at-check" channel, with
//  the two points. First, with no clip playing, it poses its right arm by hand (Avatar.setJointRotation), which the
//  look-at must leave alone while it isn't turning the head.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "look-at-check";
    var HIPS = { x: 0, y: 1, z: 0 };
    var LEFT = { x: -2, y: 1.65, z: -2 };   // 45 degrees left of ahead (-z), at about head height
    var RIGHT = { x: 2, y: 1.65, z: -2 };
    var started = Date.now();
    var phase = "";
    var posed = null;   // {index, rotation}: the joint posed by hand

    Agent.isAvatar = true;
    Avatar.displayName = "look-at-check";
    Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
    Avatar.position = HIPS;
    Avatar.orientation = Quat.IDENTITY;

    function tell(name) {
        phase = name;
        print("LOOK-AT body phase " + name + " at " + Math.round((Date.now() - started) / 100) / 10 + " s");
        var data = { phase: name, hips: HIPS, left: LEFT, right: RIGHT, posed: posed,
                     head: Avatar.getJointIndex("Head") };
        Messages.sendMessage(CHANNEL, JSON.stringify(data));
    }

    Script.update.connect(function () {
        var seconds = (Date.now() - started) / 1000;
        if (seconds >= 3 && !posed && Avatar.getJointIndex("RightArm") >= 0) {
            posed = { index: Avatar.getJointIndex("RightArm"), rotation: Quat.fromPitchYawRollDegrees(0, 0, 60) };
            Avatar.setJointRotation(posed.index, posed.rotation);
            print("LOOK-AT body posed joint " + posed.index + " by hand at " + Math.round(seconds * 10) / 10 + " s");
        }
        if (seconds >= 6 && phase === "") {
            tell("ahead");
        } else if (seconds >= 9 && phase === "ahead") {
            tell("head");
        } else if (seconds >= 15 && phase === "head") {
            Avatar.releaseHeadLookAtControl();
            tell("eyes");
        } else if (seconds >= 21 && phase === "eyes") {
            Avatar.releaseEyesLookAtControl();
            tell("released");
        } else if (seconds >= 27 && phase === "released") {
            tell("done");
        }
        if (phase === "head") {
            Avatar.setHeadLookAt(LEFT);
        } else if (phase === "eyes") {
            Avatar.setEyesLookAt(RIGHT);
        }
    });
}());
