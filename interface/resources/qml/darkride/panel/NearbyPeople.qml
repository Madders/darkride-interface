//
//  NearbyPeople.qml
//  qml/darkride/panel
//
//  Everyone in the world but you, for the panels that list them (Settings' mixer, People's Here): name, distance,
//  how loud they are now, and your volume, mute and block for them. A ListModel updated in place four times a second
//  while active, in name order, so a row being dragged isn't rebuilt under the mouse and rows don't move as people
//  walk about. The engine forgets a person's volume when they or you leave; a mute is mutual (Users.personalMute);
//  a block hides them and their voice from you (Users.ignore), and a blocked row stays until you leave, so it can be
//  unblocked. A bot whose host hasn't switched "Show in lists" on is left out while it's quiet, and listed while it
//  speaks or makes noise and for a little after (Caity 10-26, option a): the metaverse's list of who's in this world
//  says which session is a bot and its show_in_lists, asked on arrival and again when someone new comes.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15

Item {
    id: nearby

    property bool active: false
    // whether this world lets you kick and mute others for everyone (Users.canKick, read at each refresh: it has no
    // change signal of its own)
    property bool canKick: false
    readonly property alias model: people
    readonly property int count: people.count

    ListModel {
        id: people
    }

    // the metaverse's word on each session here, by session id without braces: { kind, shown }
    // (GET /api/v1/users?status=<domain>, for the signed in; metaverse website-1029 4be078a). Asked again, every 5 s
    // at most, only when someone arrives who wasn't here at the last ask (a visitor it doesn't list isn't asked again)
    property var accounts: ({})
    property var askedAbout: ({})
    property string accountsDomain: ""
    property real accountsAsked: 0
    property var lastHeard: ({})   // session id -> when a quiet-listed bot was last heard
    readonly property real heardFor: 10000   // ms a bot stays listed after it was last heard

    function bare(id) {
        return String(id).replace(/[{}]/g, "").toLowerCase();
    }
    function askAccounts() {
        var domain = String(AddressManager.domainID);
        var none = "00000000-0000-0000-0000-000000000000";
        // asked signed out too: the metaverse then answers 401 and everyone stays listed, as before
        if (bare(domain) === "" || bare(domain) === none) {
            return;
        }
        accountsDomain = domain;
        accountsAsked = Date.now();
        var present = {};
        AvatarList.getAvatarIdentifiers().forEach(function (id) { present[String(id)] = true; });
        askedAbout = present;
        MetaverseRequests.send("GET", "/api/v1/users?status=" + encodeURIComponent(bare(domain)), null,
                               function (error, code, answer) {
            if (error) {
                if (code !== 401) {
                    console.log("People: who's here failed: " + code + " " + error);
                }
                return;
            }
            var users = (answer && answer.data && answer.data.users) || (answer && answer.users) || [];
            var known = {};
            users.forEach(function (user) {
                if (user && user.location && user.location.node_id) {
                    known[bare(user.location.node_id)] = { kind: user.kind || "", shown: user.show_in_lists !== false };
                }
            });
            accounts = known;
        });
    }
    // left out of the list now: a bot not shown in lists that hasn't been heard for a while
    function quietBot(id, loudness) {
        var account = accounts[bare(id)];
        if (!account || account.kind !== "bot" || account.shown) {
            return false;
        }
        var now = Date.now();
        if (loudness > 30) {   // audioLoudness: speaking or making noise (the meters reach 1 at 3000)
            lastHeard[id] = now;
        }
        return !(lastHeard[id] && now - lastHeard[id] < heardFor);
    }

    Timer {
        interval: 250
        repeat: true
        running: nearby.active
        triggeredOnStart: true
        onTriggered: nearby.refresh()
    }

    function refresh() {
        canKick = Users.canKick;
        var mine = String(MyAvatar.sessionUUID);
        var ids = AvatarList.getAvatarIdentifiers().map(function (id) { return String(id); }).filter(function (id) {
            return id !== "" && id !== mine && id !== "{00000000-0000-0000-0000-000000000000}";
        });
        // the metaverse asked again on arriving in a world, and when someone it hasn't told about is here
        var stale = String(AddressManager.domainID) !== accountsDomain
            || ids.some(function (id) { return !askedAbout[id]; });
        if (stale && Date.now() - accountsAsked > 5000) {
            askAccounts();
        }
        ids = ids.filter(function (id) {
            var avatar = AvatarList.getAvatar(id);
            return !avatar || !quietBot(id, avatar.audioLoudness);
        });
        for (var i = people.count - 1; i >= 0; i--) {
            if (ids.indexOf(people.get(i).id) === -1 && !people.get(i).blocked) {
                people.remove(i);
            }
        }
        ids.forEach(function (id) {
            var avatar = AvatarList.getAvatar(id);
            if (!avatar) {
                return;
            }
            var name = avatar.sessionDisplayName || avatar.displayName || "Someone";
            var distance = Vec3.distance(avatar.position, MyAvatar.position);
            var record = {
                id: id,
                name: name,
                distance: isFinite(distance) ? distance : -1,
                level: Math.max(0, Math.min(1, Math.log(1 + Math.max(0, avatar.audioLoudness)) / Math.log(1 + 3000))),
                gain: Users.getAvatarGain(id),
                muted: Users.getPersonalMuteStatus(id),
                blocked: Users.getIgnoreStatus(id)
            };
            var at = indexOf(id);
            if (at === -1) {
                var place = 0;
                while (place < people.count && people.get(place).name.toLowerCase() < name.toLowerCase()) {
                    place++;
                }
                people.insert(place, record);
            } else {
                for (var key in record) {
                    if (people.get(at)[key] !== record[key]) {
                        people.setProperty(at, key, record[key]);
                    }
                }
            }
        });
    }

    function indexOf(id) {
        for (var j = 0; j < people.count; j++) {
            if (people.get(j).id === id) {
                return j;
            }
        }
        return -1;
    }

    function setGain(index, gain) {
        Users.setAvatarGain(people.get(index).id, gain);
        people.setProperty(index, "gain", gain);
    }

    function setMuted(index, muted) {
        Users.personalMute(people.get(index).id, muted);
        people.setProperty(index, "muted", muted);
    }

    function setBlocked(index, blocked) {
        Users.ignore(people.get(index).id, blocked);
        people.setProperty(index, "blocked", blocked);
    }

    // the world's own powers, for whoever may kick there (Users.canKick)
    function kick(index) {
        Users.kick(people.get(index).id);
    }

    function muteForEveryone(index) {
        Users.mute(people.get(index).id);
    }
}
