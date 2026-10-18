import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.0
import stylesUit 1.0
import controlsUit 1.0 as HifiControlsUit
import "../../controls" as HifiControls
import ".."

Item {
    id: actionBar
    x:0
    y:0
    width: 300
    // tall enough for the back and settings buttons, one above the other
    height: 640
    z: -1

    signal sendToScript(var message);
    signal windowClosed();
    
    property bool shown: true

    onShownChanged: {
        actionBar.visible = shown;
    }

	Rectangle {
        anchors.fill : parent
        color: "transparent"
        Flow {
            id: flowMain
            spacing: 10
            flow: Flow.TopToBottom
            layoutDirection: Flow.TopToBottom
            anchors.fill: parent
            anchors.margins: 4
        }
    }

    Component.onCompleted: {
        // put on bottom
        x = 7;
        y = 7;
        width = 300;
        height = 640;
    }
    
    // Messages from the bar's script (sendToQml, on this thread): {buttons: [properties, ...]} sets the properties of
    // the buttons, in the order they were added (the app theme's icons and colours, when it changes)
    function fromScript(message) {
        if (!message || !message.buttons) {
            return;
        }
        var index = 0;
        for (var i = 0; i < flowMain.children.length && index < message.buttons.length; i++) {
            var button = flowMain.children[i];
            if (button.icon === undefined) {
                continue;
            }
            var properties = message.buttons[index++];
            Object.keys(properties).forEach(function (key) {
                button[key] = properties[key];
            });
        }
    }

    function addButton(properties) {
        var component = Qt.createComponent("button.qml");
        if (component.status == Component.Ready) {
            var button = component.createObject(flowMain);
            // copy all properites to button
            var keys = Object.keys(properties).forEach(function (key) {
                button[key] = properties[key];
            });
            return button;
        } else if( component.status == Component.Error) {
            console.log("Load button errors " + component.errorString());
        }
    }

    function urlHelper(src) {
            // (a theme's own icons are in the app's assets: assets:/themes/<theme>/icons)
            if (src.match(/\bhttp/) || src.match(/^(assets|file|qrc):/)) {
                return src;
            } else {
                return "../../../" + src;
            }
        }

}
