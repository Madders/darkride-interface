//
//  AlertLook.qml
//  interface/resources/qml/dialogs/alert
//
//  The colours Darkride's alerts are drawn in: Darkride's own, or while a skin is on (SKINS.md, stage 2) the skin's
//  where it gives them, and its "popup" frame in place of the rounded panel. The native alerts (interface/src/ui/Popup)
//  take the same colours from the same keys.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

import QtQuick 2.12
import DarkridePanels 1.0

QtObject {
    readonly property var skinColors: (PanelTheme.skin && PanelTheme.skin.colors) || ({})
    function pick(key, own) {
        return skinColors[key] !== undefined ? skinColors[key] : own;
    }
    function faded(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha);
    }

    readonly property color panel: pick("card", "#2b2b2e")
    readonly property color panelBorder: pick("separator", Qt.rgba(1, 1, 1, 0.10))
    readonly property color title: pick("text", "#ffffff")
    readonly property color text: pick("secondaryText", Qt.rgba(1, 1, 1, 0.74))
    readonly property color dimText: pick("tertiaryText", Qt.rgba(1, 1, 1, 0.35))
    readonly property color placeholder: pick("tertiaryText", Qt.rgba(1, 1, 1, 0.4))
    readonly property color link: pick("tint", "#4c9ffe")
    readonly property color tint: pick("tint", "#0b6bd8")             // the default button, a ticked box, selections
    readonly property color tintPressed: Qt.darker(tint, 1.15)
    readonly property color focus: faded(pick("tint", "#0a84ff"), 0.59)
    readonly property color well: pick("fill", Qt.rgba(0, 0, 0, 0.28))  // under fields, menus, details, boxes
    readonly property color wellBorder: pick("separator", Qt.rgba(1, 1, 1, 0.14))
    readonly property color button: pick("fill", Qt.rgba(1, 1, 1, 0.14))
    readonly property color buttonPressed: pick("cardPressed", Qt.rgba(1, 1, 1, 0.22))
    readonly property color buttonText: pick("text", "#ffffff")
    readonly property color hover: pick("cardPressed", Qt.rgba(1, 1, 1, 0.08))
    readonly property color destructive: pick("destructive", "#ff6961")
    readonly property color menu: pick("card", "#323236")
    readonly property color menuBorder: pick("separator", Qt.rgba(1, 1, 1, 0.12))
    readonly property color checkBorder: pick("secondaryText", Qt.rgba(1, 1, 1, 0.3))

    // the skin's popup frame: { source, slice: [l, t, r, b], padding }, or null
    readonly property var frame: (PanelTheme.skin && PanelTheme.skin.frames && PanelTheme.skin.frames.popup) || null
}
