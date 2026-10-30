//
//  scripts-off-client.js
//  tests-manual/wasm-scripting
//
//  The test client of scripts-off.sh: once in the sandbox, it tells scripts-off-driver.js where its avatar stands (the
//  driver puts the box with the scripts round it) until the box comes, and quits when the driver says it's done or
//  after 100 s. Its own part is only to be there: the box's client entity script runs in this client.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var CONTROL = "scripts-off-control";
    var started = Date.now();
    var told = null;

    function quit(why) {
        print("SCRIPTS-OFF client quits: " + why);
        Script.setTimeout(function () {
            Test.quit();
        }, 1000);
    }

    Messages.subscribe(CONTROL);
    Messages.messageReceived.connect(function (channel, message) {
        if (channel === CONTROL && message.indexOf("\"done\"") >= 0) {
            quit("the driver is done");
        }
    });

    told = Script.setInterval(function () {
        var near = Entities.findEntities(MyAvatar.position, 10);
        for (var i = 0; i < near.length; i++) {
            if (Entities.getEntityProperties(near[i], ["name"]).name === "scripts-off box") {
                print("SCRIPTS-OFF client sees the box " + near[i]);
                Script.clearInterval(told);
                return;
            }
        }
        if (Date.now() - started > 8000) {
            Messages.sendMessage(CONTROL, JSON.stringify({ here: MyAvatar.position }));
        }
    }, 2000);

    Script.setTimeout(function () {
        quit("100 s");
    }, 100000);
}());
