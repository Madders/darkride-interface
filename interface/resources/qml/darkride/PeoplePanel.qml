//
//  PeoplePanel.qml
//  qml/darkride
//
//  Darkride's People panel, in the panels' iOS look (darkride/panel), with three tabs (Caity, 10-23/24):
//  Here: everyone in this world, with how loud they are now, your volume and mute for each, and a line of actions
//    (Block; Mute for Everyone and Kick for whoever may kick here);
//  Friends: requests to and from you, adding a friend by their exact username, and your friends, online first, with
//    the world each is in and Join, and Remove; a name opens their profile. Your friends are collections under the
//    panels' search bar (search, Cards / Tiles / List), as Explore's worlds and the avatars in My Files are;
//  Profile: someone's profile (the metaverse's /api/user/<name>/profile): picture, names, About, since when, an AI
//    resident's word and host; signed in, how you two stand (with the matching friend action), the friends you share,
//    and online and where as they allow, with Join. Yours when no one is chosen. Explore opens it on an author: it
//    sets io.darkride.peoplePanel.profile and the tab, and the bar opens this panel, which reads them as it shows.
//  The friends are the metaverse's (/api/v1/friends and its requests, through MetaverseRequests); joining one is
//  AddressManager.goToUser. There is no search for people by name: that is a privacy call (Caity's). It replaces
//  High Fidelity's People app (pal.js and Pal.qml), whose connections the metaverse no longer has.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import QtQuick.Layouts 1.15
import "panel"

Page {
    id: root

    property real unit: 1
    signal sendToScript(var message)   // the tablet's loader connects it; nothing is sent

    anchors.fill: parent
    title: "People"
    tabs: ["Here", "Friends", "Profile"]

    readonly property string tabSetting: "io.darkride.peoplePanel.tab"

    header: [
        SearchBar {
            id: friendBar
            visible: root.currentTab === 1 && friends.signedIn
            settingsKey: "io.darkride.peoplePanel.friends"
            placeholder: "Search your friends"
            views: [{ text: "Cards", value: "cards" }, { text: "Tiles", value: "tiles" }, { text: "List", value: "list" }]
            view: "list"
        }
    ]
    readonly property string profileSetting: "io.darkride.peoplePanel.profile"

    // the tab and the person, as saved (Explore may have changed them before asking for this panel)
    function readSettings() {
        profile.username = Settings.getValue(profileSetting, "");
        var tab = Math.max(0, Math.min(2, Settings.getValue(tabSetting, 0)));
        if (tab !== currentTab) {
            currentTab = tab;
        } else {
            loadTab();
        }
    }
    function loadTab() {
        if (currentTab === 1) {
            friends.load();
        } else if (currentTab === 2) {
            profile.load();
        }
    }
    // someone's profile, from a name here
    function showProfile(username) {
        profile.username = username;
        Settings.setValue(profileSetting, username);
        if (currentTab !== 2) {
            currentTab = 2;
        } else {
            profile.load();
        }
        scrollToTop();
    }

    Component.onCompleted: readSettings()
    onCurrentTabChanged: {
        Settings.setValue(tabSetting, currentTab);
        loadTab();
    }
    onVisibleChanged: {
        if (visible) {
            readSettings();
        }
    }

    // ==== Here =======================================================================================================

    NearbyPeople {
        id: peopleHere
        active: root.visible && root.currentTab === 0
    }

    Tab {
        visible: root.currentTab === 0

        Section {
            title: "In this world"
            footer: "Click a name for more. Muting is mutual: neither of you hears the other. Blocking hides them "
                + "and their voice from you. Volumes last for this visit."

            Repeater {
                model: peopleHere.model
                delegate: PersonRow {
                    nearby: peopleHere
                    index: model.index
                    name: model.name
                    distance: model.distance
                    level: model.level
                    gain: model.gain
                    muted: model.muted
                    blocked: model.blocked
                    actions: true
                }
            }
            ListRow {
                visible: peopleHere.count === 0
                label: "No one else is here."
                enabled: false
            }
        }
    }

    // ==== Friends ====================================================================================================

    QtObject {
        id: friends

        readonly property bool signedIn: AccountServices.loggedIn
        property var online: []         // [{ username, ai, image, location }], by username
        property var offline: []
        property var incoming: []       // requests to you: [{ username, image, createdAt }], newest first
        property var outgoing: []       // yours
        property bool busy: false
        property string status: ""      // the last action's result, under the Add section
        property string removing: ""    // the friend whose Remove waits for its second press
        property string adding: ""      // a request on its way

        // a friend as a collection's item, the friend kept for the row's buttons
        function entries(list) {
            return list.map(function (friend) {
                var where = friend.online
                    ? (friend.location && friend.location.name ? "in " + friend.location.name : "online")
                    : "offline";
                return { key: friend.username, title: friend.username, subtitle: where + (friend.ai ? " · AI resident" : ""),
                         picture: picture(friend), friend: friend };
            }).filter(friendBar.matches);
        }

        function picture(entry) {
            var image = entry.image || "";
            if (/^https?:\/\//.test(image)) {
                return image;
            }
            return MetaverseRequests.metaverseUrl() + (image !== "" ? image
                : "/api/user/" + encodeURIComponent(entry.username) + "/image");
        }

        function load() {
            if (!signedIn || busy) {
                return;
            }
            busy = true;
            MetaverseRequests.send("GET", "/api/v1/friends", null, function (error, code, answer) {
                busy = false;
                if (error) {
                    status = "Couldn't read your friends: " + error + ".";
                    return;
                }
                var list = (answer && answer.data && answer.data.friends) || [];
                online = list.filter(function (friend) { return friend.online; });
                offline = list.filter(function (friend) { return !friend.online; });
            });
            MetaverseRequests.send("GET", "/api/v1/friends/requests", null, function (error, code, answer) {
                if (error) {
                    return;
                }
                incoming = (answer && answer.data && answer.data.incoming) || [];
                outgoing = (answer && answer.data && answer.data.outgoing) || [];
            });
        }

        // a request's or a removal's answer: { username, friends, requested }, or a message
        function act(method, path, body, done) {
            MetaverseRequests.send(method, path, body, function (error, code, answer) {
                var message = answer && answer.message ? String(answer.message) : "";
                if (error) {
                    status = message !== "" ? message : "That didn't go through: " + error + ".";
                } else if (done) {
                    status = done(answer && answer.data ? answer.data : {});
                }
                load();
                if (root.currentTab === 2) {
                    profile.load();
                }
            });
        }

        function request(username) {
            username = username.trim();
            if (username === "") {
                return;
            }
            status = "Asking " + username + "…";
            act("POST", "/api/v1/friends/requests", { username: username }, function (data) {
                addField.text = "";
                return data.friends ? "You and " + data.username + " are friends now."
                    : "Asked " + (data.username || username) + ". You'll be friends when they accept.";
            });
        }
        function accept(username) {
            act("POST", "/api/v1/friends/requests/" + encodeURIComponent(username) + "/accept", {}, function (data) {
                return "You and " + username + " are friends now.";
            });
        }
        function decline(username) {
            act("POST", "/api/v1/friends/requests/" + encodeURIComponent(username) + "/decline", {}, function () {
                return "Declined " + username + "'s request.";
            });
        }
        function withdraw(username) {
            act("DELETE", "/api/v1/friends/requests/" + encodeURIComponent(username), null, function () {
                return "Withdrew your request to " + username + ".";
            });
        }
        function remove(username) {
            removing = "";
            act("DELETE", "/api/v1/friends/" + encodeURIComponent(username), null, function () {
                return username + " is no longer a friend.";
            });
        }
        function join(username) {
            AddressManager.goToUser(username);
        }
        // for the rows drawn by an inline component, which can't reach the page's id
        function showProfile(username) {
            root.showProfile(username);
        }

        onSignedInChanged: load()
    }

    Timer {
        interval: 30 * 1000
        repeat: true
        running: root.visible && root.currentTab === 1 && friends.signedIn
        onTriggered: friends.load()
    }

    // a small button inside a row
    component RowButton: Rectangle {
        id: rowButton
        property string text: ""
        property bool danger: false
        property bool strong: false
        signal pressed()
        implicitWidth: rowButtonLabel.implicitWidth + 18 * Theme.unit
        implicitHeight: 26 * Theme.unit
        radius: 7 * Theme.unit
        color: strong ? Theme.tint : Theme.fill
        Text {
            id: rowButtonLabel
            anchors.centerIn: parent
            text: rowButton.text
            color: rowButton.strong ? "#ffffff" : rowButton.danger ? Theme.destructive : Theme.tint
            font.pixelSize: Theme.controlTextSize
        }
        MouseArea {
            anchors.fill: parent
            onClicked: rowButton.pressed()
        }
    }

    // a friend: where they are and Join when online; Remove behind a second press
    component FriendRow: ListRow {
        id: friendRow
        property var friend: ({})
        label: friend.username || ""
        detail: friend.online
            ? (friend.location && friend.location.name ? "in " + friend.location.name : "online")
              + (friend.ai ? " · AI resident" : "")
            : (friend.ai ? "AI resident · offline" : "offline")
        icon: friends.picture(friend)
        clickable: true
        onClicked: friends.showProfile(friend.username)
        RowButton {
            visible: friends.removing === friendRow.friend.username
            text: "Cancel"
            onPressed: friends.removing = ""
        }
        RowButton {
            text: friends.removing === friendRow.friend.username ? "Remove them?" : "Remove"
            danger: true
            onPressed: {
                if (friends.removing === friendRow.friend.username) {
                    friends.remove(friendRow.friend.username);
                } else {
                    friends.removing = friendRow.friend.username;
                }
            }
        }
        RowButton {
            visible: friendRow.friend.online && !!friendRow.friend.location
            text: "Join"
            strong: true
            onPressed: friends.join(friendRow.friend.username)
        }
    }

    // a friend as a tile: picture, name, where; Join when you can; a click opens their profile
    component FriendTile: CollectionTile {
        id: friendTile
        property var friend: ({})
        title: friend.username || ""
        subtitle: friend.online ? (friend.location && friend.location.name ? "in " + friend.location.name : "online") : "offline"
        picture: friends.picture(friend)
        pictureRatio: 1
        onClicked: friends.showProfile(friend.username)
        Item {
            Layout.fillWidth: true
        }
        RowButton {
            visible: friendTile.friend.online && !!friendTile.friend.location
            text: "Join"
            strong: true
            onPressed: friends.join(friendTile.friend.username)
        }
    }

    Tab {
        visible: root.currentTab === 1

        Section {
            visible: !friends.signedIn
            footer: "Sign in to see your friends and their requests."
        }

        Section {
            visible: friends.signedIn && (friends.incoming.length > 0 || friends.outgoing.length > 0)
            title: "Requests"

            Repeater {
                model: friends.incoming
                delegate: ListRow {
                    label: modelData.username
                    detail: "wants to be friends"
                    icon: friends.picture(modelData)
                    clickable: true
                    onClicked: root.showProfile(modelData.username)
                    RowButton { text: "Decline"; onPressed: friends.decline(modelData.username) }
                    RowButton { text: "Accept"; strong: true; onPressed: friends.accept(modelData.username) }
                }
            }
            Repeater {
                model: friends.outgoing
                delegate: ListRow {
                    label: modelData.username
                    detail: "you asked"
                    icon: friends.picture(modelData)
                    clickable: true
                    onClicked: root.showProfile(modelData.username)
                    RowButton { text: "Withdraw"; onPressed: friends.withdraw(modelData.username) }
                }
            }
        }

        Section {
            visible: friends.signedIn
            title: "Add a friend"
            footer: friends.status !== "" ? friends.status : "Ask by their exact username: they'll see your request here."

            ListRow {
                label: "Username"
                fill: true
                Field {
                    id: addField
                    Layout.fillWidth: true
                    horizontalAlignment: TextInput.AlignLeft
                    placeholderText: "someone"
                    onAccepted: friends.request(text)
                }
            }
            ListRow {
                action: true
                label: "Send Friend Request"
                enabled: addField.text.trim() !== ""
                onClicked: friends.request(addField.text)
            }
        }

        CollectionView {
            id: onlineFriends
            visible: friends.signedIn
            title: "Online" + (friends.online.length ? " · " + friends.online.length : "")
            items: friends.entries(friends.online)
            view: friendBar.view
            row: Component { FriendRow { friend: entry.friend } }
            tile: Component { FriendTile { friend: entry.friend } }
            empty: friends.online.length === 0 ? (friends.busy ? "…" : "None of your friends is online.")
                : "No friend online matches \u201c" + friendBar.search + "\u201d."
            onActivated: friends.showProfile(item.key)
        }

        CollectionView {
            visible: friends.signedIn && friends.offline.length > 0
            title: "Offline · " + friends.offline.length
            items: friends.entries(friends.offline)
            view: friendBar.view
            row: Component { FriendRow { friend: entry.friend } }
            tile: Component { FriendTile { friend: entry.friend } }
            empty: "No friend offline matches \u201c" + friendBar.search + "\u201d."
            onActivated: friends.showProfile(item.key)
        }
    }

    // ==== Profile ====================================================================================================

    QtObject {
        id: profile

        property string username: ""    // whose; "" for yours
        property var data: null         // the metaverse's answer
        property bool busy: false
        property string status: ""
        property bool confirmRemove: false

        readonly property string whose: username !== "" ? username : (friends.signedIn ? AccountServices.username : "")
        readonly property string relation: data && data.relation ? data.relation : "none"
        readonly property bool mine: data !== null && relation === "self"

        function load() {
            confirmRemove = false;
            if (whose === "") {
                data = null;
                status = "";
                return;
            }
            busy = true;
            var asked = whose;
            MetaverseRequests.send("GET", "/api/user/" + encodeURIComponent(asked) + "/profile", null,
                                   function (error, code, answer) {
                if (asked !== whose) {
                    return;   // someone else was chosen meanwhile
                }
                busy = false;
                if (error) {
                    data = null;
                    status = code === 404 ? "There's no one called " + asked + "." : "Couldn't read the profile: " + error + ".";
                    return;
                }
                data = answer;
                status = "";
            });
        }

        function since(date) {
            var when = new Date(date);
            return isNaN(when.getTime()) ? "" : when.toLocaleDateString(Qt.locale(), "MMMM yyyy");
        }
    }

    Tab {
        visible: root.currentTab === 2

        Section {
            visible: profile.whose === ""
            footer: "Sign in to see your profile. A name in Friends, or an author's name in Explore, shows theirs here."
        }
        Section {
            visible: profile.whose !== "" && profile.data === null
            footer: profile.busy ? "…" : profile.status
        }

        // who they are
        Section {
            visible: profile.data !== null

            Item {
                width: parent.width
                height: who.implicitHeight + 2 * Theme.cardPadding

                RowLayout {
                    id: who
                    x: Theme.cardPadding
                    y: Theme.cardPadding
                    width: parent.width - 2 * Theme.cardPadding
                    spacing: 14 * Theme.unit

                    RoundedImage {
                        Layout.preferredWidth: 72 * Theme.unit
                        Layout.preferredHeight: 72 * Theme.unit
                        radius: width / 2
                        placeholder: profile.data ? profile.data.username : ""
                        source: profile.data ? friends.picture(profile.data) : ""
                    }
                    Column {
                        Layout.fillWidth: true
                        spacing: 2 * Theme.unit

                        Text {
                            width: parent.width
                            text: profile.data ? (profile.data.displayName || profile.data.username) : ""
                            color: Theme.text
                            font.pixelSize: 20 * Theme.unit
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: !!profile.data && !!profile.data.displayName && profile.data.displayName !== profile.data.username
                            text: profile.data ? profile.data.username : ""
                            color: Theme.secondaryText
                            font.pixelSize: Theme.bodySize
                        }
                        Text {
                            visible: !!profile.data && profile.data.ai
                            width: parent.width
                            text: profile.data ? (profile.data.aiWord || "AI resident")
                                + (profile.data.host ? " · hosted by " + profile.data.host : "") : ""
                            color: Theme.tint
                            font.pixelSize: Theme.detailSize
                            elide: Text.ElideRight
                        }
                        Text {
                            text: {
                                if (!profile.data) return "";
                                var bits = [];
                                if (profile.data.online !== undefined) bits.push(profile.data.online ? "online" : "offline");
                                var since = profile.since(profile.data.memberSince);
                                if (since !== "") bits.push("here since " + since);
                                return bits.join(" · ");
                            }
                            color: Theme.secondaryText
                            font.pixelSize: Theme.detailSize
                        }
                    }
                }
            }
        }

        Section {
            visible: profile.data !== null && !!profile.data.about
            title: "About"

            Item {
                width: parent.width
                height: aboutText.implicitHeight + 2 * Theme.rowPadding + 6 * Theme.unit
                Text {
                    id: aboutText
                    x: Theme.cardPadding
                    width: parent.width - 2 * Theme.cardPadding
                    anchors.verticalCenter: parent.verticalCenter
                    text: profile.data ? profile.data.about || "" : ""
                    color: Theme.text
                    font.pixelSize: Theme.bodySize
                    wrapMode: Text.WordWrap
                    lineHeight: 1.15
                }
            }
        }

        // where they are, and you two (signed in; the metaverse leaves these out otherwise)
        Section {
            visible: profile.data !== null && !!profile.data.location
            ListRow {
                label: profile.data && profile.data.location ? "In " + profile.data.location.name : ""
                RowButton {
                    visible: !profile.mine
                    text: "Join"
                    strong: true
                    onPressed: friends.join(profile.data.username)
                }
            }
        }

        Section {
            visible: profile.data !== null && friends.signedIn && !profile.mine && profile.data.relation !== undefined
            footer: friends.status

            ListRow {
                visible: profile.relation === "none"
                action: true
                label: "Send Friend Request"
                onClicked: friends.request(profile.data.username)
            }
            ListRow {
                visible: profile.relation === "requested"
                label: "You asked to be friends"
                RowButton { text: "Withdraw"; onPressed: friends.withdraw(profile.data.username) }
            }
            ListRow {
                visible: profile.relation === "asked"
                label: "Wants to be friends"
                RowButton { text: "Decline"; onPressed: friends.decline(profile.data.username) }
                RowButton { text: "Accept"; strong: true; onPressed: friends.accept(profile.data.username) }
            }
            ListRow {
                visible: profile.relation === "friend"
                label: "Friends"
                RowButton {
                    visible: profile.confirmRemove
                    text: "Cancel"
                    onPressed: profile.confirmRemove = false
                }
                RowButton {
                    text: profile.confirmRemove ? "Remove them?" : "Remove"
                    danger: true
                    onPressed: {
                        if (profile.confirmRemove) {
                            profile.confirmRemove = false;
                            friends.remove(profile.data.username);
                        } else {
                            profile.confirmRemove = true;
                        }
                    }
                }
            }
        }

        Section {
            visible: profile.data !== null && (profile.data.mutualFriends || []).length > 0
            title: "Friends you share · " + (profile.data ? (profile.data.mutualFriends || []).length : 0)

            Repeater {
                model: profile.data ? profile.data.mutualFriends || [] : []
                delegate: ListRow {
                    label: modelData
                    icon: friends.picture({ username: modelData })
                    clickable: true
                    onClicked: root.showProfile(modelData)
                }
            }
        }

        Section {
            visible: friends.signedIn && profile.username !== "" && profile.username !== AccountServices.username
            ListRow {
                action: true
                label: "Show My Profile"
                onClicked: root.showProfile("")
            }
        }
    }
}
