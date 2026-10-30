//
//  walk-frames-body.js
//  tests-manual/wasm-scripting
//
//  The body of walk-frames.sh: an agent on the default avatar that loops the stock walk, then the stock run, each
//  first from frame 0 (Avatar.startAnimation's default) and then over the client's frame range (avatar-animation.json:
//  walk_fwd 1-30, run_fast_fwd 1-19). Frame 0 of the stock clips is the skeleton's reference pose (a T-pose), so a loop
//  from 0 shows it once each time round. It tells walk-frames-watch.js each phase on the "walk-frames-check" channel,
//  with the turn from one update to the next that counts toward a flash: the walk turns a joint under 10 degrees a
//  frame, the run up to about 33 (and 38 where its loop goes from frame 19 back to 1, as in the client); frame 0's
//  T-pose is 60 to 110 degrees from frame 1.
//  The clips play at 27 frames a second, not 30: at 30 the loop and the body's 30 updates a second keep step, so every
//  loop is seen at the same moment, and a flash is seen every time round or never.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CHANNEL = "walk-frames-check";
    var ANIM = Script.resolvePath("../../interface/resources/avatar/animations/");
    var PHASE_SECONDS = 10;
    var FPS = 27;
    var PHASES = [
        { name: "walk from 0", clip: "walk_fwd", jump: 45 },
        { name: "walk 1-30", clip: "walk_fwd", frames: [1, 30], jump: 45 },
        { name: "run from 0", clip: "run_fast_fwd", jump: 60 },
        { name: "run 1-19", clip: "run_fast_fwd", frames: [1, 19], jump: 60 }
    ];
    var started = Date.now();
    var next = 0;
    var phaseAt = 0;

    Agent.isAvatar = true;
    Avatar.displayName = "walk-frames-check";
    Avatar.skeletonModelURL = Script.resolvePath("../../interface/resources/meshes/defaultAvatar_full.fst");
    Avatar.position = { x: 0, y: 1, z: 0 };
    Avatar.orientation = Quat.IDENTITY;

    function tell(name, jump) {
        print("WALK-FRAMES body phase " + name + " at " + Math.round((Date.now() - started) / 100) / 10 + " s");
        Messages.sendMessage(CHANNEL, JSON.stringify({ phase: name, jump: jump }));
    }

    Script.update.connect(function () {
        var now = Date.now();
        if (next === 0 && (now - started < 4000 || Avatar.getJointIndex("Hips") < 0)) {
            return;
        }
        if (next > 0 && now - phaseAt < PHASE_SECONDS * 1000) {
            return;
        }
        if (next === PHASES.length) {
            tell("done");
            next++;
            return;
        }
        if (next > PHASES.length) {
            return;
        }
        var phase = PHASES[next++];
        phaseAt = now;
        if (phase.frames) {
            Avatar.startAnimation(ANIM + phase.clip + ".fbx", FPS, 1, true, false, phase.frames[0], phase.frames[1]);
        } else {
            Avatar.startAnimation(ANIM + phase.clip + ".fbx", FPS, 1, true, false);
        }
        tell(phase.name, phase.jump);
    });
}());
