//
//  agent-identity-watch.js
//  tests-manual/wasm-scripting
//
//  The watcher of agent-identity.sh: a second agent that reads every other avatar's name and model through AvatarList,
//  as anyone's client does, once a second for 30 s. "AGENT-IDENTITY RESULT" says PASS when it sees the body's model
//  (defaultAvatar_full.fst), which only its identity carries. Its name can't show: an avatar's display name is its
//  account's username (AvatarData::setDisplayName), and these agents aren't signed in.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var LIMIT_SECONDS = 30;
    var started = Date.now();
    var lastSeen = "";

    Agent.isAvatar = true;
    Avatar.displayName = "identity-watch";

    function log(text) {
        print("AGENT-IDENTITY " + text);
    }

    var timer = Script.setInterval(function () {
        var seconds = Math.round((Date.now() - started) / 1000);
        var others = AvatarList.getAvatarIdentifiers().filter(function (id) {
            return !Uuid.isNull(id) && !Uuid.isEqual(id, Agent.sessionUUID);
        });
        var seen = others.map(function (id) {
            var avatar = AvatarList.getAvatar(id);
            return JSON.stringify({ displayName: avatar.displayName, skeletonModelURL: avatar.skeletonModelURL });
        }).join(", ");
        if (seen !== lastSeen) {
            log("at " + seconds + " s: " + others.length + " other avatar(s): " + seen);
            lastSeen = seen;
        }
        var found = others.some(function (id) {
            var avatar = AvatarList.getAvatar(id);
            return /defaultAvatar_full\.fst$/.test(String(avatar.skeletonModelURL));
        });
        if (found || seconds >= LIMIT_SECONDS) {
            Script.clearInterval(timer);
            log("RESULT " + (found ? "PASS" : "FAIL") + " after " + seconds + " s");
        }
    }, 1000);
}());
