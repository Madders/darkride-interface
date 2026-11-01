import QtQuick 2.5
import Qt.labs.settings 1.0

import DarkridePanels 1.0
import "../../windows"
import "."

Window {
    id: window
    frame: ToolFrame {
        horizontalSpacers: horizontal
        verticalSpacers: !horizontal
    }
    property var tabletProxy;
    property var buttonModel: ListModel {}
    hideBackground: true
    resizable: false
    destroyOnCloseButton: false
    destroyOnHidden: false
    closable: false
    shown: true
    width: content.width
    height: content.height
    // Disable this window from being able to call 'desktop.raise() and desktop.showDesktop'
    activator: Item {}
    property bool horizontal: true
    // a skin's bar frame (SKINS.md "bar", stage 2) behind the buttons, its padding around them; without a skin, look
    // A's dark bar (BarButton.qml)
    readonly property var skinBar: (PanelTheme.skin && PanelTheme.skin.frames && PanelTheme.skin.frames.bar) || null
    readonly property var barPadding: skinBar ? skinBar.padding : [6, 8, 6, 8]
    // every button in one cell, wide enough for the widest label (Caity 10-28)
    readonly property real cellWidth: {
        var widest = horizontal ? 64 : 68;
        var buttons = (horizontal ? row : column).children;
        for (var i = 0; i < buttons.length; i++) {
            if (buttons[i].captionWidth !== undefined) {
                widest = Math.max(widest, buttons[i].captionWidth + 12);
            }
        }
        return widest;
    }

    property alias settings: settings

    Settings {
        id: settings
        category: "toolbar/" + window.objectName
        property alias x: window.x
        property real y: 0
        property real desktopHeight: 0 //placeholder for desktop height
    }

    onYChanged: {
        //check if Y changed not due to Desktop size changed. ie. mouse manipulations
        //otherwise it will be save by Desktop
        if (desktop.height > 100 && desktop.height === settings.desktopHeight) {
            settings.y = window.y
        }
    }

    Component {
        id: buttonComponent
        BarButton {
            id: toolbarButton
            cellWidth: window.cellWidth
            property var proxy: modelData;
            onClicked: proxy.clicked()
            Component.onCompleted: updateProperties()

            Connections {
                target: proxy;
                function onPropertiesChanged() {
                    updateProperties();
                } 
            }
            
            function updateProperties() {
                Object.keys(proxy.properties).forEach(function (key) {
                    if (toolbarButton[key] !== proxy.properties[key]) {
                        toolbarButton[key] = proxy.properties[key];
                    }
                });
            }
        }
    }

    Item {
        id: content
        implicitHeight: (horizontal ? row.height : column.height) + window.barPadding[1] + window.barPadding[3]
        implicitWidth: (horizontal ? row.width : column.width) + window.barPadding[0] + window.barPadding[2]

        Rectangle {
            anchors.fill: parent
            visible: !window.skinBar
            color: "#171b2d"
            radius: 10
            border.color: "#262c47"
        }

        BorderImage {
            anchors.fill: parent
            visible: !!window.skinBar
            source: window.skinBar ? window.skinBar.source : ""
            border.left: window.skinBar ? window.skinBar.slice[0] : 0
            border.top: window.skinBar ? window.skinBar.slice[1] : 0
            border.right: window.skinBar ? window.skinBar.slice[2] : 0
            border.bottom: window.skinBar ? window.skinBar.slice[3] : 0
        }

        Row {
            id: row
            x: window.barPadding[0]
            y: window.barPadding[1]
            visible: window.horizontal
            spacing: 2
            Repeater {
                model: buttonModel
                delegate: buttonComponent
            }
        }
        
        Column {
            id: column
            x: window.barPadding[0]
            y: window.barPadding[1]
            visible: !window.horizontal 
            spacing: 2
            Repeater {
                model: buttonModel
                delegate: buttonComponent
            }
        }
    }
}
