//
//  SkinFrame.qml
//  qml/darkride/panel
//
//  A skin's frame for a part of a panel (ui/Skins.h, SKINS.md): its picture for a state ("image", "hover",
//  "pressed", "checked", "disabled"), stretched as its slice says, the corners kept as drawn. Shown only while a skin
//  gives the part a frame; Darkride's own look draws without it.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

import QtQuick 2.15
import "."

BorderImage {
    id: skinFrame

    property string part: ""
    property string look: "image"
    readonly property var frame: Theme.frame(part)
    readonly property bool shown: frame !== null

    visible: shown
    source: shown ? (look !== "image" && frame[look] ? frame[look] : frame.source) : ""
    border.left: shown ? frame.slice[0] : 0
    border.top: shown ? frame.slice[1] : 0
    border.right: shown ? frame.slice[2] : 0
    border.bottom: shown ? frame.slice[3] : 0
    horizontalTileMode: BorderImage.Stretch
    verticalTileMode: BorderImage.Stretch
}
