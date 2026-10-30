//
//  mannequin-body.js
//  tests-manual/wasm-scripting
//
//  The body of mannequin.sh: an agent with no model of its own (the default avatar) that walks on the spot. Before the
//  assignment client carried the default avatar's skeleton, such an agent never got a skeleton, so it never animated.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var started = Date.now();
    var told = false;

    Agent.isAvatar = true;
    Avatar.position = { x: 0, y: 1, z: 0 };
    var WALK = Script.resolvePath("../../interface/resources/avatar/animations/walk_fwd.fbx");
    Avatar.startAnimation(WALK, 30, 1, true, false);

    Script.update.connect(function () {
        if (!told && Date.now() - started > 5000) {
            told = true;
            print("MANNEQUIN body: model " + JSON.stringify(Avatar.skeletonModelURL) + ", " +
                  Avatar.getJointNames().length + " joints named");
        }
    });
}());
