//
//  running-scripts-window.js
//  tests-manual/wasm-scripting
//
//  Opens the Running Scripts window with the Developer Menu on, so that each row shows the engine that runs the
//  script, keeps it open for fifteen seconds (for a look or a screenshot) and quits.
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    Script.setTimeout(function () {
        Menu.setIsOptionChecked("Developer Menu", true);
        Menu.triggerOption("Running Scripts...");
        print("RS-WINDOW open");
        Script.setTimeout(function () {
            print("RS-WINDOW closing");
            Test.quit();
        }, 15000);
    }, 6000);
}());
