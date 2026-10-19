//
//  autoGaze.js
//  scripts/system/libraries
//
//  Created on 2026-10-15 for Darkride's avatar gaze styles.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//
/* global module, Script, Settings, MyAvatar, Chat, Messages, XMLHttpRequest, print */

// Automatic gaze style ("auto" in the Emote app's Eyes row): the avatar's eyes follow its owner's mood, read from what
// they type, the emotes they play and how they move, and change only when the mood is obvious.
//
// - What they type: a clear cue first (the rules below: "!!", "omg", a line in capitals, a stammer, "*blushes*",
//   "yawn", an emoji); else, where a mood service answers (the AI residents' perception service has one, /mood, which
//   asks GLiNER2), its reading. Without a service, cues alone.
// - Emotes they play (Dancing, Cheering, ...) and running or jumping add to excited; a few idle minutes doze off
//   into sleepy, and any activity wakes them.
// - When the eyes change: each reading adds to its mood's score and the scores fade (half-life HALF_LIFE_S). A mood is
//   worn once its score reaches SWITCH_AT and held at least HOLD_S; calm comes back once no score is CALM_BELOW or
//   more. A cue counts RULE and switches at once; the service's obvious reading counts MODEL, so on its own it needs a
//   second line that agrees within about a minute. No score passes SWITCH_AT, and a clear cue for one mood halves
//   the others, so a fresh clear mood beats an older pile-up. The same rules and numbers as the residents' mood.py.
//
// On or off with the setting "gaze/auto"; the service's address is "gaze/moodURL".

var SETTING_ON = "gaze/auto";
var SETTING_URL = "gaze/moodURL";
var DEFAULT_URL = "http://127.0.0.1:8798/mood";
var EMOTE_CHANNEL = "tivoli.emote";

var STYLES = ["calm", "excited", "shy", "sleepy", "intent"];
var RULE = 1.0, MODEL = 0.7;
var HALF_LIFE_S = 60, SWITCH_AT = 1.0, CALM_BELOW = 0.35, HOLD_S = 20;
var IDLE_S = 240;                 // still, silent and typing nothing this long: dozing off
var RUN_SPEED = 3.5;              // m/s along the ground
var JUMP_SPEED = 2.5;             // m/s upward

// style -> cues, as in mood.py; the old script engine knows no "u" flag, so emoji are surrogate pairs
var RULES = {
    sleepy: [/\b(sleepy|drowsy|exhausted|yawn\w*|zz+z|nap ?time|bed ?time|(going|off|heading) to bed|(so|really|kinda|getting|i'?m|im) tired\b(?! of)|can'?t keep my eyes open|need (a )?nap)\b/gi,
             /😴|🥱|💤/g],
    shy: [/\b([a-z])-\1[a-z]*/gi,
          /\b(\w+)\s*(\.{2,}|…)\s*\1\b/gi,
          /\b(um+|uh+|erm+)\b.*\b(um+|uh+|erm+)\b/gi,
          /\*\s*(blush\w*|hides?|hiding|fidget\w*|looks? (away|down)|shuffles?|twiddles?)\b/gi,
          /\b(i'?m|so|kinda|a bit|little) (shy|nervous|embarrassed)\b|\bembarrass(ed|ing)\b/gi,
          />\/+</g,
          /🥺|😳|🙈|👉👈/g],
    intent: [/\b(listen to me|pay attention|look at me|i mean it|this is (important|serious)|i'?m (serious|angry|furious|mad)|so (angry|mad)|furious)\b/gi,
             /😠|😡|🤬/g],
    excited: [/!{2,}/g,
              /\b(omg|oh my god|ya+y+|wo+h?o+|wo+w+|yes{2,}|so excited|can'?t wait|let'?s go+|ha(ha){2,}|lmao)\b/gi,
              /🎉|🥳|🤩|😆|🤣|‼/g],
    calm: [/\b(chill(ing|ed)?|relax(ing|ed)?|calm|peaceful|cozy|comfy|no rush|take it easy)\b/gi,
           /😌|☕|🍵/g]
};
var RULE_ORDER = ["sleepy", "shy", "intent", "excited", "calm"];

// emotes from the Emote app (or an avatar's own menu): the mood each adds to, and how much
var EMOTE_MOODS = [[/danc/i, "excited", 0.5], [/cheer/i, "excited", 0.6], [/clap/i, "excited", 0.4],
                   [/surpris/i, "excited", 0.4], [/cry/i, "shy", 0.5], [/love/i, "shy", 0.5]];

function shouted(text) {
    var letters = text.replace(/[^A-Za-z]/g, "");
    return letters.length >= 8 && letters.replace(/[^A-Z]/g, "").length >= 0.8 * letters.length;
}

// {mood, obvious, why} from the cues, or null when none is there
function byRules(text) {
    var counts = {}, first = {};
    RULE_ORDER.forEach(function (style) {
        RULES[style].forEach(function (cue) {
            cue.lastIndex = 0;
            var m;
            while ((m = cue.exec(text)) !== null) {
                counts[style] = (counts[style] || 0) + 1;
                if (!first[style]) { first[style] = m[0]; }
                if (m[0].length === 0) { cue.lastIndex++; }
            }
        });
    });
    if (shouted(text)) {
        counts.excited = (counts.excited || 0) + 1;
        if (!first.excited) { first.excited = "written in capitals"; }
    }
    var best = null;
    RULE_ORDER.forEach(function (style) {
        if (counts[style] && (best === null || counts[style] > counts[best])) { best = style; }
    });
    return best === null ? null : { mood: best, obvious: true, why: "rule, " + counts[best] + " cue" +
                                    (counts[best] > 1 ? "s" : "") + ": '" + first[best] + "'" };
}

// when the eyes change (mood.py's Follower)
function Follower() {
    this.scores = {};
    this.ready = {};
    this.style = "calm";
    this.since = null;
    this.t = null;
}
Follower.prototype.fade = function (now) {
    if (this.t !== null && now > this.t) {
        var k = Math.pow(0.5, (now - this.t) / HALF_LIFE_S), kept = {};
        for (var s in this.scores) {
            if (this.scores[s] * k > 0.01) { kept[s] = this.scores[s] * k; }
        }
        this.scores = kept;
    }
    this.t = now;
};
Follower.prototype.wear = function (style, now) {
    if (style === this.style) { return null; }
    this.style = style;
    this.since = now;
    return style;
};
Follower.prototype.add = function (reading, now) {
    this.fade(now);
    if (!reading || !reading.obvious || STYLES.indexOf(reading.mood) < 0) { return this.tick(now); }
    var rule = String(reading.why || "").indexOf("rule") === 0;
    if (reading.mood === "calm") {
        if (rule) {
            for (var s in this.scores) { this.scores[s] *= 0.25; }
        }
        return this.tick(now);
    }
    if (rule) {
        for (var other in this.scores) {
            if (other !== reading.mood) { this.scores[other] *= 0.5; }
        }
    }
    this.scores[reading.mood] = Math.min(SWITCH_AT, (this.scores[reading.mood] || 0) + (rule ? RULE : MODEL));
    return this.tick(now);
};
Follower.prototype.nudge = function (mood, amount, now) {
    this.fade(now);
    if (STYLES.indexOf(mood) > 0) { this.scores[mood] = Math.min(SWITCH_AT, (this.scores[mood] || 0) + amount); }
    return this.tick(now);
};
Follower.prototype.tick = function (now) {
    this.fade(now);
    var s, best = null;
    for (s in this.ready) {
        if ((this.scores[s] || 0) < CALM_BELOW) { delete this.ready[s]; }
    }
    for (s in this.scores) {
        if (this.scores[s] >= SWITCH_AT) { this.ready[s] = true; }
        if (best === null || this.scores[s] > this.scores[best]) { best = s; }
    }
    var held = this.since === null || now - this.since >= HOLD_S;
    if (best !== null && this.ready[best] && best !== this.style && (held || this.style === "calm")) {
        return this.wear(best, now);
    }
    if (this.style !== "calm" && held && (best === null || this.scores[best] < CALM_BELOW)) {
        return this.wear("calm", now);
    }
    return null;
};

// --- the module: reads the owner's lines, emotes and moves while "gaze/auto" is on ---

var follower = new Follower();
var on = false;
var timer = null;
var lastActive = Date.now();
var dozing = false;
var lastPosition = null, lastOrientation = null, lastJump = 0;
var serviceWarned = false;

function now() { return Date.now() / 1000; }

function log(text) { print("[autoGaze] " + text); }

function wear(style, why) {
    if (style) {
        MyAvatar.gazeStyle = style;
        log("eyes: " + style + " (" + why + ")");
    }
}

function active() {
    lastActive = Date.now();
    if (dozing) {
        dozing = false;
        delete follower.scores.sleepy;
        delete follower.ready.sleepy;
        if (follower.style === "sleepy") {
            follower.since = null;     // awake at once
            wear(follower.tick(now()) || follower.wear("calm", now()), "awake again");
        }
    }
}

function askService(text) {
    var url = Settings.getValue(SETTING_URL, DEFAULT_URL);
    if (!url) { return; }
    var req = new XMLHttpRequest();
    req.onreadystatechange = function () {
        if (req.readyState !== 4) { return; }
        if (req.status !== 200) {
            if (!serviceWarned) {
                serviceWarned = true;
                log("no mood service at " + url + " (" + req.status + "): only clear cues are read");
            }
            return;
        }
        serviceWarned = false;
        var reading;
        try { reading = JSON.parse(req.responseText); } catch (e) { return; }
        if (on) { wear(follower.add(reading, now()), reading.mood + ", " + reading.why); }
    };
    req.open("POST", url, true);
    req.setRequestHeader("Content-Type", "application/json");
    req.timeout = 3000;
    req.send(JSON.stringify({ text: String(text).slice(0, 1000) }));
}

function onChat(data, senderID) {
    if (!on || !data || data.local || senderID !== MyAvatar.sessionUUID || typeof data.message !== "string") { return; }
    active();
    var ruled = byRules(data.message);
    if (ruled) {
        wear(follower.add(ruled, now()), ruled.mood + ", " + ruled.why);
    } else {
        askService(data.message);
    }
}

function onMessage(channel, message, senderID) {
    if (!on || channel !== EMOTE_CHANNEL || senderID !== MyAvatar.sessionUUID) { return; }
    var data;
    try { data = JSON.parse(message); } catch (e) { return; }
    if (!data || !data.emote) { return; }
    active();
    EMOTE_MOODS.forEach(function (row) {
        if (row[0].test(String(data.emote))) {
            wear(follower.nudge(row[1], row[2], now()), row[1] + ", the " + data.emote + " emote");
        }
    });
}

function everySecond() {
    var wanted = Settings.getValue(SETTING_ON, false) === true || Settings.getValue(SETTING_ON, false) === "true";
    if (wanted !== on) {
        on = wanted;
        follower = new Follower();
        follower.style = MyAvatar.gazeStyle;
        lastActive = Date.now();
        dozing = false;
        log(on ? "on" : "off");
        if (on && follower.style !== "calm") { follower.since = now(); }
    }
    if (!on) { return; }
    var t = now();
    var v = MyAvatar.velocity;
    var ground = Math.sqrt(v.x * v.x + v.z * v.z);
    if (ground > RUN_SPEED) {
        wear(follower.nudge("excited", 0.12, t), "excited, running");
    }
    if (v.y > JUMP_SPEED && Date.now() - lastJump > 1000) {
        lastJump = Date.now();
        wear(follower.nudge("excited", 0.25, t), "excited, jumping");
    }
    var p = MyAvatar.position, q = MyAvatar.orientation;
    var moved = lastPosition !== null && (Math.abs(p.x - lastPosition.x) + Math.abs(p.y - lastPosition.y) + Math.abs(p.z - lastPosition.z) > 0.05 ||
                                          Math.abs(q.x - lastOrientation.x) + Math.abs(q.y - lastOrientation.y) + Math.abs(q.z - lastOrientation.z) + Math.abs(q.w - lastOrientation.w) > 0.01);
    lastPosition = p;
    lastOrientation = q;
    if (moved || MyAvatar.audioLoudness > 30) {
        active();
    } else if (Date.now() - lastActive > IDLE_S * 1000) {
        // dozing off, and staying dozy for as long as nothing happens
        wear(follower.nudge("sleepy", RULE, t), "sleepy, idle " + Math.round((Date.now() - lastActive) / 1000) + " s");
        dozing = true;
    }
    var turn = follower.tick(t);
    wear(turn, turn === "calm" ? "faded" : "its turn, after the last style's hold");
}

function start() {
    if (timer !== null) { return; }
    Chat.messageReceived.connect(onChat);
    Messages.subscribe(EMOTE_CHANNEL);
    Messages.messageReceived.connect(onMessage);
    timer = Script.setInterval(everySecond, 1000);
}

function shutdown() {
    if (timer === null) { return; }
    Script.clearInterval(timer);
    timer = null;
    Chat.messageReceived.disconnect(onChat);
    Messages.messageReceived.disconnect(onMessage);
}

function setOn(value) {
    Settings.setValue(SETTING_ON, !!value);
}

function isOn() {
    return Settings.getValue(SETTING_ON, false) === true || Settings.getValue(SETTING_ON, false) === "true";
}

module.exports = {
    start: start,
    shutdown: shutdown,
    setOn: setOn,
    isOn: isOn,
    byRules: byRules,           // for tests
    Follower: Follower,
    _test: { everySecond: everySecond, onChat: onChat, onMessage: onMessage }
};
