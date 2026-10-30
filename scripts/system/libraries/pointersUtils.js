"use strict";

//  pointerUtils.js
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html

/* jslint bitwise: true */

/* global Script, Pointers,
   DEFAULT_SEARCH_SPHERE_DISTANCE, COLORS_GRAB_SEARCHING_HALF_SQUEEZE, COLORS_GRAB_SEARCHING_FULL_SQUEEZE,
   COLORS_GRAB_DISTANCE_HOLD, TRIGGER_ON_VALUE,
   Pointer:true, PointerManager:true
*/

Script.include("/~/system/libraries/controllerDispatcherUtils.js");
Pointer = function(hudLayer, pickType, pointerData) {
    // the look (Caity 10-28, look A): a fine cyan tube, bright at the hand and fading out along its length, with a soft
    // dot where it hits, a ring while the trigger is squeezed; the same colour searching, squeezing and holding (it was
    // blue while searching, red while squeezing). The pictures are rendering's (images/lasers: fade.png stretched along the line, dot.png, ring.png),
    // white, tinted by the colour. The line is a PolyLine: glow gives the tube's soft edge, three widths taper it.
    var LASER_COLOR = { red: 0, green: 175, blue: 255 };
    var DOT_COLOR = { red: 150, green: 220, blue: 255 };   // lighter, so its centre reads white-hot
    var LASER_PICTURES = Script.resourcesPath() + "images/lasers/";
    this.SEARCH_SPHERE_SIZE = 0.015;   // the dot, a metre away (the end scales with distance)
    this.SQUEEZE_RING_SIZE = 0.02;
    function laserPath() {
        return {
            type: "line3d",
            color: LASER_COLOR,
            visible: true,
            alpha: 1,
            solid: true,
            glow: 1.0,
            faceCamera: true,
            isUVModeStretch: true,
            textures: LASER_PICTURES + "fade.png",
            strokeWidths: [0.006, 0.0045, 0.003],   // the glow's falloff leaves about 40% of each as the bright core
            ignoreRayIntersection: true, // always ignore this
            drawInFront: !hudLayer, // Even when burried inside of something, show it.
            drawHUDLayer: hudLayer
        };
    }
    function laserEnd(picture, size, color) {
        return {
            type: "image3d",
            url: LASER_PICTURES + picture,
            dimensions: { x: size, y: size, z: 0.001 },
            color: color,
            alpha: 1,
            emissive: true,
            billboardMode: "full",
            ignoreRayIntersection: true,
            drawInFront: !hudLayer, // Even when burried inside of something, show it.
            drawHUDLayer: hudLayer,
            visible: true
        };
    }
    this.halfPath = laserPath();
    this.halfEnd = laserEnd("dot.png", this.SEARCH_SPHERE_SIZE, DOT_COLOR);
    this.fullPath = laserPath();
    this.fullEnd = laserEnd("ring.png", this.SQUEEZE_RING_SIZE, LASER_COLOR);
    this.holdPath = laserPath();

    this.renderStates = [
        {name: "half", path: this.halfPath, end: this.halfEnd},
        {name: "full", path: this.fullPath, end: this.fullEnd},
        {name: "hold", path: this.holdPath}
    ];

    this.defaultRenderStates = [
        {name: "half", distance: DEFAULT_SEARCH_SPHERE_DISTANCE, path: this.halfPath},
        {name: "full", distance: DEFAULT_SEARCH_SPHERE_DISTANCE, path: this.fullPath},
        {name: "hold", distance: DEFAULT_SEARCH_SPHERE_DISTANCE, path: this.holdPath}
    ];


    this.pointerID = null;
    this.visible = false;
    this.locked = false;
    this.allwaysOn = false;
    this.hand = pointerData.hand;
    delete pointerData.hand;

    function createPointer(pickType, pointerData) {
        var pointerID = Pointers.createPointer(pickType, pointerData);
        Pointers.setRenderState(pointerID, "");
        Pointers.enablePointer(pointerID);
        return pointerID;
    }

    this.enable = function() {
        Pointers.enablePointer(this.pointerID);
    };

    this.disable = function() {
        Pointers.disablePointer(this.pointerID);
    };

    this.removePointer = function() {
        Pointers.removePointer(this.pointerID);
    };

    this.makeVisible = function() {
        this.visible = true;
    };

    this.makeInvisible = function() {
        this.visible = false;
    };

    this.lockEnd = function(lockData) {
        if (lockData !== undefined) {
            if (this.visible && !this.locked && lockData.targetID !== null) {
                var targetID = lockData.targetID;
                var targetIsOverlay = lockData.isOverlay;
                if (lockData.offset === undefined) {
                    Pointers.setLockEndUUID(this.pointerID, targetID, targetIsOverlay);
                } else {
                    Pointers.setLockEndUUID(this.pointerID, targetID, targetIsOverlay, lockData.offset);
                }
                this.locked = targetID;
            }
        } else if (this.locked) {
            Pointers.setLockEndUUID(this.pointerID, null, false);
            this.locked = false;
        }
    };

    this.updateRenderState = function(triggerClicks, triggerValues) {
        var mode = "";
        if (this.visible) {
            if (this.locked) {
                mode = "hold";
            } else if (triggerClicks[this.hand]) {
                mode = "full";
            } else if (triggerValues[this.hand] > TRIGGER_ON_VALUE || this.alwaysOn) {
                mode = "half";
            }
        }

        Pointers.setRenderState(this.pointerID, mode);
    };
    pointerData.renderStates = this.renderStates;
    pointerData.defaultRenderStates = this.defaultRenderStates;
    this.pointerID = createPointer(pickType, pointerData);
};


PointerManager = function() {
    this.pointers = [];

    this.createPointer = function(hudLayer, pickType, pointerData) {
        var pointer = new Pointer(hudLayer, pickType, pointerData);
        this.pointers.push(pointer);
        return pointer.pointerID;
    };

    this.makePointerVisible = function(laserParams) {
        var index = laserParams.hand;
        if (index < this.pointers.length && index >= 0) {
            this.pointers[index].makeVisible();
            this.pointers[index].alwaysOn = laserParams.alwaysOn;
        }
    };

    this.makePointerInvisible = function(laserParams) {
        var index = laserParams.hand;
        if (index < this.pointers.length && index >= 0) {
            this.pointers[index].makeInvisible();
        }
    };

    this.lockPointerEnd = function(laserParams, lockData) {
        var index = laserParams.hand;
        if (index < this.pointers.length && index >= 0) {
            this.pointers[index].lockEnd(lockData);
        }
    };

    this.updatePointersRenderState = function(triggerClicks, triggerValues) {
        for (var index = 0; index < this.pointers.length; index++) {
            this.pointers[index].updateRenderState(triggerClicks, triggerValues);
        }
    };

    this.removePointers = function() {
        for (var index = 0; index < this.pointers.length; index++) {
            this.pointers[index].removePointer();
        }
        this.pointers = [];
    };
};
