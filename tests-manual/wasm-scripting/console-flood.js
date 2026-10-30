//
//  console-flood.js
//  tests-manual/wasm-scripting
//
//  A hand test of the scripting console under a flood of messages: paste all of this into Developer > Scripting >
//  Console... and press Enter. For 90 s it sends the same error 45 times a second, as a timer that fails on every tick
//  does; then for 90 s a numbered line 45 times a second; then it stops on its own.
//  What to see:
//  1. The error is one red line whose grey count at its end goes up, 45 a second.
//  2. Then the numbered lines scroll by, and only the newest 500 are kept (scroll up: the oldest left is about 500
//     before the newest).
//  3. With the console left open, the app keeps responding throughout, and after 3 minutes "CONSOLE-FLOOD done" is
//     printed and the lines stop.
//  4. Paste it again and, while it runs, close the console (its close button), then open it again from the menu: it
//     opens at once, a blue line says "Closing the console stopped everything typed above...", and no count or
//     numbered line moves after it.
//  5. While it runs, drag the console out, dock it, then tab it behind another panel: the count keeps going up
//     throughout.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

var consoleFloodErrors = Script.setInterval(function () {
    var props = {};
    return props.dimensions.y;
}, 1000 / 45);
var consoleFloodLines;
Script.setTimeout(function () {
    Script.clearInterval(consoleFloodErrors);
    var count = 0;
    consoleFloodLines = Script.setInterval(function () {
        count++;
        print("CONSOLE-FLOOD line " + count);
    }, 1000 / 45);
}, 90 * 1000);
Script.setTimeout(function () {
    Script.clearInterval(consoleFloodLines);
    print("CONSOLE-FLOOD done");
}, 180 * 1000);
