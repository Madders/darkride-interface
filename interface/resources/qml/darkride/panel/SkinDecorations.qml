//
//  SkinDecorations.qml
//  qml/darkride/panel
//
//  A skin's decorations on a panel (SKINS.md "decorations", stage 2): each picture pinned at its place on the panel
//  (top-left … bottom-right), moved by its offset, at its size or its own, and stretched along its edge when it asks
//  (grass along the bottom). Drawn over the panel; they never take a click. Nothing without a skin.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.15
import "."

Item {
    id: decorations
    enabled: false   // never in the way of a click

    Repeater {
        model: Theme.decorations
        Image {
            readonly property string at: modelData.at || "top-left"
            readonly property bool alongWidth: modelData.stretch && (at === "top" || at === "bottom")
            readonly property bool alongHeight: modelData.stretch && (at === "left" || at === "right")
            readonly property var size: modelData.size && modelData.size.length === 2 ? modelData.size : null
            readonly property var offset: modelData.offset && modelData.offset.length === 2 ? modelData.offset : [0, 0]

            source: modelData.source
            fillMode: modelData.stretch ? Image.Stretch : Image.PreserveAspectFit
            mipmap: true
            width: alongWidth ? decorations.width : (size ? size[0] : implicitWidth)
            height: alongHeight ? decorations.height : (size ? size[1] : implicitHeight)
            x: (at.indexOf("left") >= 0 ? 0
                : at.indexOf("right") >= 0 ? decorations.width - width
                : (decorations.width - width) / 2) + offset[0]
            y: (at.indexOf("top") >= 0 ? 0
                : at.indexOf("bottom") >= 0 ? decorations.height - height
                : (decorations.height - height) / 2) + offset[1]
        }
    }
}
