//
//  standardButtons.js
//  interface/resources/qml/dialogs/alert
//
//  Created on 2026-10-19 for Darkride's popups.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//
//  Turns a message box's standard buttons (QMessageBox::StandardButton flags, which every caller still passes and gets
//  back) into an alert's buttons: their words, which one is the default (blue, Return), which one answers Escape, and
//  which ones delete or replace something for good (red words, never the default).

.pragma library

// The standard buttons, with the words each has unless a box gives its own: buttonTexts { yes: "Run" }.
var STANDARD = [
    { name: "ok", value: 0x00000400, text: "OK" },
    { name: "save", value: 0x00000800, text: "Save" },
    { name: "saveAll", value: 0x00001000, text: "Save All" },
    { name: "open", value: 0x00002000, text: "Open" },
    { name: "yes", value: 0x00004000, text: "Yes" },
    { name: "yesToAll", value: 0x00008000, text: "Yes to All" },
    { name: "no", value: 0x00010000, text: "No" },
    { name: "noToAll", value: 0x00020000, text: "No to All" },
    { name: "abort", value: 0x00040000, text: "Abort" },
    { name: "retry", value: 0x00080000, text: "Retry" },
    { name: "ignore", value: 0x00100000, text: "Ignore" },
    { name: "close", value: 0x00200000, text: "Close" },
    { name: "cancel", value: 0x00400000, text: "Cancel" },
    { name: "discard", value: 0x00800000, text: "Discard" },
    { name: "help", value: 0x01000000, text: "Help" },
    { name: "apply", value: 0x02000000, text: "Apply" },
    { name: "reset", value: 0x04000000, text: "Reset" },
    { name: "restoreDefaults", value: 0x08000000, text: "Restore Defaults" }
];

var OK = 0x00000400;
var CANCEL = 0x00400000;
var DISCARD = 0x00800000;

// The buttons that answer Escape, first found first; a box with one button lets Escape answer it.
var ESCAPE_ORDER = [CANCEL, 0x00010000, 0x00200000, 0x00040000];  // Cancel, No, Close, Abort
// When a box names no default, the first of these it shows is the default.
var ACCEPT_ORDER = [OK, 0x00004000, 0x00000800, 0x00002000, 0x02000000, 0x00080000];  // OK, Yes, Save, Open, Apply, Retry

function textFor(standard, texts) {
    if (texts) {
        if (texts[standard.name] !== undefined) {
            return texts[standard.name];
        }
        if (texts[String(standard.value)] !== undefined) {
            return texts[String(standard.value)];
        }
    }
    return standard.text;
}

function firstShown(order, shown) {
    for (var i = 0; i < order.length; i++) {
        if (shown.indexOf(order[i]) >= 0) {
            return order[i];
        }
    }
    return 0;
}

// The alert's buttons for a box's flags, default and destructive button, and words: [{ text, value, role, cancel }],
// the cancel button last.
function list(flags, defaultButton, destructiveButton, texts) {
    var shown = [];
    for (var i = 0; i < STANDARD.length; i++) {
        if (flags & STANDARD[i].value) {
            shown.push(STANDARD[i].value);
        }
    }
    function destructive(value) {
        return value === destructiveButton || value === DISCARD;
    }

    var escape = firstShown(ESCAPE_ORDER, shown);
    if (!escape && shown.length === 1 && !destructive(shown[0])) {
        escape = shown[0];
    }

    var chosen = shown.indexOf(defaultButton) >= 0 && !destructive(defaultButton) ? defaultButton : 0;
    if (!chosen) {
        var anyDestructive = shown.some(destructive);
        if (shown.length === 1 && !destructive(shown[0])) {
            chosen = shown[0];
        } else if (anyDestructive) {
            chosen = escape;  // with a button that deletes for good, the safe answer is the default
        } else {
            chosen = firstShown(ACCEPT_ORDER, shown);
        }
    }

    var result = [];
    var cancelItem = null;
    for (i = 0; i < STANDARD.length; i++) {
        var standard = STANDARD[i];
        if (shown.indexOf(standard.value) < 0) {
            continue;
        }
        var item = {
            text: textFor(standard, texts),
            value: standard.value,
            role: standard.value === chosen ? "default" : (destructive(standard.value) ? "destructive" : "plain"),
            cancel: standard.value === escape
        };
        if (item.cancel && item.role !== "default") {
            cancelItem = item;
        } else {
            result.push(item);
        }
    }
    if (cancelItem) {
        result.push(cancelItem);
    }
    return result;
}

// A box's text with its informative text, if any, as a second paragraph.
function joined(text, informativeText) {
    if (!informativeText) {
        return text || "";
    }
    return text ? text + "\n\n" + informativeText : informativeText;
}
