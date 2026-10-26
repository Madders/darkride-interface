//
//  NearbyPeople.qml
//  qml/darkride/panel
//
//  Everyone in the world but you, for the panels that list them (Settings' mixer, People's Here): name, distance,
//  how loud they are now, and your volume, mute and block for them. A ListModel updated in place four times a second
//  while active, in name order, so a row being dragged isn't rebuilt under the mouse and rows don't move as people
//  walk about. The engine forgets a person's volume when they or you leave; a mute is mutual (Users.personalMute);
//  a block hides them and their voice from you (Users.ignore), and a blocked row stays until you leave, so it can be
//  unblocked.
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
