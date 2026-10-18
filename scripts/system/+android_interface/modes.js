"use strict";
//
//  modes.js
//  scripts/system/
//
//  Created by Gabriel Calero & Cristian Duarte on Jan 23, 2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
(function() { // BEGIN LOCAL_SCOPE

// The view from your avatar. (High Fidelity's mobile app also had a top-down "TOP VIEW", for meetings, switched by a
// button here; it doesn't work in most places, where there's a ceiling, so it's gone.)

var displayNames = Script.require('./displayNames.js');
var clickWeb = Script.require('./clickWeb.js');

displayNames.init();
clickWeb.init();

Script.scriptEnding.connect(function () {
    displayNames.ending();
    clickWeb.ending();
});

}()); // END LOCAL_SCOPE
