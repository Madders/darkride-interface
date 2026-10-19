//
//  StarterAvatars.qml
//  interface/resources/qml/hifi/avatarapp
//
//  Created on 2026-10-19 for Darkride's avatar support.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.5
import stylesUit 1.0
import controlsUit 1.0 as HifiControlsUit

// Ready-made avatars anyone may wear, from a list on the web: a JSON object whose "avatars" are each a name, a url to
// wear, a thumbnail url, and who made it and on what terms ("author", "licence"); with a "credit" for the whole list.
// Opened in place of the High Fidelity Marketplace, which is gone.
Rectangle {
    id: root
    visible: false
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.5)
    z: 999

    property string listUrl: "https://overte.jollysoft.com/Assets/Avatars/Starter/starter-avatars.json"
    property var onWear  // function(url, name)
    property string status: ""
    property string credit: ""

    function open() {
        visible = true;
        if (avatars.count === 0) {
            load();
        }
    }

    function close() {
        visible = false;
    }

    function load() {
        status = "Loading...";
        var request = new XMLHttpRequest();
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) {
                return;
            }
            var list = null;
            try {
                list = request.status === 200 ? JSON.parse(request.responseText) : null;
            } catch (e) {
                list = null;
            }
            if (!list || !list.avatars) {
                status = "Couldn't get the list of avatars. Check your connection and try again.";
                return;
            }
            avatars.clear();
            for (var i = 0; i < list.avatars.length; i++) {
                var avatar = list.avatars[i];
                avatars.append({ 'name': avatar.name || '', 'url': avatar.url || '', 'thumbnail': avatar.thumbnail || '',
                                 'author': avatar.author || '', 'licence': avatar.licence || '' });
            }
            credit = list.credit || '';
            status = "";
        };
        request.open("GET", listUrl);
        request.send();
    }

    HifiConstants {
        id: hifi
    }

    AvatarAppStyle {
        id: style
    }

    ListModel {
        id: avatars
    }

    // over the app: nothing under it takes the clicks
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: false
        hoverEnabled: true
        onClicked: root.close()
    }

    Rectangle {
        id: mainContainer
        width: parent.width - 30
        height: parent.height - 60
        anchors.centerIn: parent
        color: "white"
        radius: 4

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: false
        }

        // TextStyle1
        RobotoMedium {
            id: title
            size: 24
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 24
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            text: "Starter Avatars"
        }

        RobotoRegular {
            id: subtitle
            size: 15
            anchors.top: title.bottom
            anchors.topMargin: 6
            anchors.left: title.left
            anchors.right: title.right
            wrapMode: Text.WordWrap
            color: style.colors.lightGray
            text: root.status !== "" ? root.status : "Pick one to wear it. " + root.credit
        }

        GridView {
            id: grid
            anchors.top: subtitle.bottom
            anchors.topMargin: 14
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: buttons.top
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.bottomMargin: 10
            clip: true
            model: avatars
            cellWidth: Math.floor(width / 3)
            cellHeight: cellWidth * 1.4 + 26

            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle {
                    id: card
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 10
                    height: width * 1.4
                    radius: 5
                    color: style.colors.lightGrayBackground
                    border.color: cardMouse.containsMouse ? style.colors.blueHighlight : "transparent"
                    border.width: 3

                    Image {
                        anchors.fill: parent
                        anchors.margins: 3
                        source: thumbnail
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }

                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (root.onWear) {
                                root.onWear(url, name);
                            }
                            root.close();
                        }
                    }
                }

                RobotoRegular {
                    anchors.top: card.bottom
                    anchors.topMargin: 4
                    anchors.left: card.left
                    anchors.right: card.right
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    size: 15
                    text: name
                }
            }
        }

        DialogButtons {
            id: buttons
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 24
            yesButton.visible: false
            noText: "CLOSE"
            onNoClicked: function() {
                root.close();
            }
        }
    }
}
