//
//  mannequin-watch.js
//  tests-manual/wasm-scripting
//
//  The watcher of mannequin.sh: a second agent that sees mannequin-body.js through AvatarList, as anyone's client does.
//  Ten seconds in it reads the body's model URL and its joints twice, half a second apart. "MANNEQUIN RESULT" says
//  PASS when the model is the default one (sent as an empty URL, which a receiver reads as its own default, qrc:...:
//  every viewer draws its own default avatar) and the joints move (the walk is playing on a skeleton).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    Agent.isAvatar = true;
    Avatar.position = { x: 0, y: 1, z: -4 };

    function log(text) {
        print("MANNEQUIN " + text);
    }

    function body() {
        var others = AvatarList.getAvatarIdentifiers().filter(function (id) {
            return !Uuid.isNull(id) && !Uuid.isEqual(id, Agent.sessionUUID);
        });
        return others.length === 1 ? AvatarList.getAvatar(others[0]) : null;
    }

    Script.setTimeout(function () {
        var avatar = body();
        if (!avatar) {
            log("RESULT FAIL: no body seen");
            return;
        }
        var first = avatar.getJointRotations();
        Script.setTimeout(function () {
            var second = body().getJointRotations();
            var moving = 0;
            for (var i = 0; i < first.length && i < second.length; i++) {
                var angle = Quat.angle(Quat.multiply(Quat.inverse(first[i]), second[i])) * 180 / Math.PI;
                moving += angle > 3 ? 1 : 0;
            }
            var url = String(body().skeletonModelURL);
            log("model " + JSON.stringify(url) + "; " + second.length + " joints, " + moving +
                " moved more than 3 degrees in half a second");
            var isDefault = url === "" || /^qrc:.*defaultAvatar_full\.fst$/.test(url);
            log("RESULT " + (isDefault && moving >= 5 ? "PASS" : "FAIL"));
        }, 500);
    }, 10000);
}());
