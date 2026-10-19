"use strict";
//
//  impostorLodView.js
//  scripts/system
//
//  Developer > Render > Show Impostors and LODs (Menu.cpp): while the item is checked, writes at the top of the view
//  what the view draws of the thing at its centre (the first entity box it meets), marked by a small cross: the
//  entity, its impostor card or its model (IMPOSTORS.md), how wide it looks against the switch angle, and for a model
//  its automatic LOD level and triangles (AUTOMATIC-LODS.md). The menu item itself tints every card blue. The same menu has Impostor Cards and Automatic
//  LODs to turn either off.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {
    var REFRESH_MS = 250;
    var PANEL_WIDTH = 560;
    var PANEL_HEIGHT = 96;
    var PANEL_TOP = 12;
    var CROSS_SIZE = 24;
    var FONT_SIZE = 16;

    var panel = null;
    var cross = null;

    function layout() {
        var size = Controller.getViewportDimensions();
        return {
            panel: { x: Math.round((size.x - PANEL_WIDTH) / 2), y: PANEL_TOP, width: PANEL_WIDTH, height: PANEL_HEIGHT },
            cross: { x: Math.round((size.x - CROSS_SIZE) / 2), y: Math.round((size.y - CROSS_SIZE) / 2),
                     width: CROSS_SIZE, height: CROSS_SIZE }
        };
    }

    function show() {
        var place = layout();
        panel = Overlays.addOverlay("text", {
            bounds: place.panel, text: "", font: { size: FONT_SIZE }, lineHeight: FONT_SIZE + 4,
            leftMargin: 10, topMargin: 8, color: { red: 255, green: 255, blue: 255 },
            backgroundColor: { red: 15, green: 18, blue: 30 }, backgroundAlpha: 0.8, visible: true
        });
        cross = Overlays.addOverlay("text", {
            bounds: place.cross, text: "+", font: { size: CROSS_SIZE }, lineHeight: CROSS_SIZE,
            leftMargin: 6, topMargin: 0, color: { red: 255, green: 220, blue: 90 }, backgroundAlpha: 0.0, visible: true
        });
    }

    function hide() {
        if (panel) {
            Overlays.deleteOverlay(panel);
            panel = null;
        }
        if (cross) {
            Overlays.deleteOverlay(cross);
            cross = null;
        }
    }

    function degrees(value) {
        return value >= 180 ? "the camera is inside it" : "looks " + value.toFixed(value < 10 ? 2 : 1) + "° across";
    }

    // the card's line: what draws, how wide the card's sphere looks and below what angle the card shows
    function cardLine(card) {
        if (!card) {
            return "MODEL: no impostor card";
        }
        if (!card.enabled) {
            return "MODEL: impostor cards are off (Developer > Render > Impostor Cards)";
        }
        var what = { card: "CARD", model: "MODEL", dissolving: "DISSOLVING (card and model)",
                     loading: "MODEL (the card's pictures are loading)" }[card.showing] || card.showing;
        return what + ": " + degrees(card.angleDeg) + ", the card shows below " + card.switchAngleDeg.toFixed(1) + "°";
    }

    // the LOD line, over the model's parts: the level the main view draws and the triangles that come to
    function lodLine(lods, cardDrawn) {
        if (!lods) {
            return "LOD: not a model";
        }
        var full = 0;
        var drawn = 0;
        var levelCount = 0;
        var shownLow = Infinity;
        var shownHigh = 0;
        lods.parts.forEach(function(part) {
            full += part.full;
            levelCount = Math.max(levelCount, part.levels.length);
            var shown = Math.min(part.shown, part.levels.length);
            drawn += shown > 0 ? part.levels[shown - 1] : part.full;
            shownLow = Math.min(shownLow, shown);
            shownHigh = Math.max(shownHigh, shown);
        });
        if (cardDrawn) {
            return "LOD: the model is not drawn (" + full + " triangles in full)";
        }
        if (levelCount === 0) {
            return "LOD: no levels (" + full + " triangles)";
        }
        if (!lods.enabled) {
            return "LOD: automatic LODs are off (Developer > Render > Automatic LODs): " + full + " triangles";
        }
        var level = shownLow === shownHigh ? String(shownHigh) : shownLow + " to " + shownHigh;
        var name = shownHigh === 0 ? "full detail" : "level " + level + " of " + levelCount;
        return "LOD: " + name + ", " + drawn + " of " + full + " triangles";
    }

    function update() {
        if (!panel) {
            return;
        }
        var place = layout();
        Overlays.editOverlay(panel, { bounds: place.panel });
        Overlays.editOverlay(cross, { bounds: place.cross });

        // the first entity whose box the view's centre meets (a box is easier to put the cross on than a chair's tubes
        // far away, and the precise pick missed the test field's chairs), the zones the camera stands in left out
        var ray = { origin: Camera.position, direction: Quat.getForward(Camera.orientation) };
        var discard = [];
        var hit = { intersects: false };
        for (var tries = 0; !hit.intersects && tries < 4; tries++) {
            hit = Entities.findRayIntersection(ray, false, [], discard, true, false);
            if (hit.intersects && Entities.getEntityProperties(hit.entityID, ["type"]).type === "Zone") {
                discard.push(hit.entityID);
                hit = { intersects: false };
            }
        }
        if (!hit.intersects) {
            Overlays.editOverlay(panel, { text: "Nothing at the centre of the view" });
            return;
        }
        var info = LODManager.describeEntity(hit.entityID);
        var name = info.name || "(unnamed)";
        if (info.card && info.card.ownerID !== hit.entityID) {
            name = (info.card.ownerName || "(unnamed)") + " > " + name;
        }
        var cardDrawn = info.card && info.card.enabled && info.card.showing === "card";
        Overlays.editOverlay(panel, {
            text: name + "   (" + hit.distance.toFixed(1) + " m)\n" + cardLine(info.card) + "\n" + lodLine(info.lods, cardDrawn)
        });
    }

    function sync() {
        var on = LODManager.showImpostorsAndLods;   // not Menu.isOptionChecked, which waits on the main thread
        if (on && !panel) {
            show();
        } else if (!on && panel) {
            hide();
        }
        update();
    }

    var timer = Script.setInterval(sync, REFRESH_MS);
    Script.scriptEnding.connect(function() {
        Script.clearInterval(timer);
        hide();
    });
}());
