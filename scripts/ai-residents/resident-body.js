//
//  resident-body.js
//  scripts/ai-residents
//
//  The body of an AI resident or a bot, driven by the resident service (resident-service/, RESIDENT-API.md). The
//  service starts one assignment-client per body, signed in as the resident, and serves it this script with the
//  service's address, the resident's name and a one-time token written in. The body links to the service over a
//  local WebSocket, says who it is (body_hello), tells the service when it is in the world (ready), and from then on
//  does what the service asks: walk, speak, play recorded lines, look, gesture, animate, cue the world's scripts, send
//  and listen on message channels. It tells the service what it perceives, only as often as the service asks.
//
//  Written in the JavaScript both script engines run (no let, const or arrow functions).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var SERVICE = "__SERVICE_URL__";
    var RESIDENT = "__RESIDENT__";
    var TOKEN = "__TOKEN__";
    var ANIM = "__ANIM_URL__";       // the service's clips, on 127.0.0.1: only this body loads them
    var AVATAR = "__AVATAR_URL__";   // the resident's model, which every visitor loads: a public URL ("" the default)

    // the codes of RESIDENT-API.md's errors that a body can meet
    var UNKNOWN_PERSON = 1006;
    var INVALID_PARAMS = -32602;
    var METHOD_NOT_FOUND = -32601;

    var WALK_SPEED = 1.3;          // m/s, "walk"
    var RUN_ABOVE = 2.5;           // m/s: faster than this uses the run clip
    var TURN_RATE = 4.0;           // the share of the remaining turn made in a second
    var WALK_WITHIN_DEGREES = 60;  // it walks once it faces the goal within this
    var FOLLOW_SLACK = 0.5;        // m: a follower sets off again when this much further than its distance
    var HEAD_ABOVE_HIPS = 0.6;     // m at scale 1: where another avatar's eyes are, from its position
    var LEAVE_AFTER_S = 60;        // a body whose service went away leaves the world after this
    var IDENTITY_RESENDS = 4;      // until the engine sends an agent's identity when the avatar mixer comes up

    // the stock clips (the client's own animations), served to the body by the service at ANIM
    var CLIPS = {
        idle: "idle", walk: "walk_fwd", run: "run_fast_fwd", talk: "talk"
    };
    // gesture name: [clip, seconds] (each plays once, then the body goes back to what it was doing)
    var GESTURES = {
        nod: ["emote_agree_headnod", 2.0],
        yes: ["emote_agree_headnodyes", 2.0],
        shake_head: ["emote_disagree_thoughtfulheadshake", 2.5],
        clap: ["emote_clap01_all", 3.0],
        point: ["emote_point01_all", 2.5],
        raise_hand: ["emote_raisehand01_all", 3.0],
        peace: ["emote-peace-right", 2.5],
        acknowledge: ["emote_agree_acknowledge", 2.0],
        drop_head: ["emote_disagree_drophead", 2.5]
    };
    // the client's frame ranges for the stock clips (interface/resources/avatar/avatar-animation.json): their frame 0
    // is the skeleton's reference pose (a T-pose), which the client never plays. Played from 0, a loop showed it once
    // each time round. A clip not listed (emote-peace-right, a builder's own) plays from its frame 0, as before.
    var FRAMES = {
        idle: [1, 300], walk_fwd: [1, 30], run_fast_fwd: [1, 19], talk: [1, 500],
        emote_agree_headnod: [1, 55], emote_agree_headnodyes: [1, 94], emote_disagree_thoughtfulheadshake: [1, 84],
        emote_clap01_all: [1, 160], emote_point01_all: [1, 134], emote_raisehand01_all: [1, 435],
        emote_agree_acknowledge: [1, 64], emote_disagree_drophead: [1, 100]
    };

    var socket = null;
    var linked = false;
    var lostAt = 0;                // when the link closed (0: linked, or never linked)
    var nextId = 0;
    var ready = false;
    var started = Date.now();

    var movement = null;           // {kind: "go_to"|"route"|"follow", re, ...}
    var facing = null;             // a yaw in degrees to turn to while standing
    var looking = null;            // {person} or {position}, and eyesOnly
    var lines = [];                // the ids of the recorded lines asked for, oldest first: the engine reports each
                                   // one's end (Agent.avatarSoundFinished) in that order, a cut-short one included
    var custom = null;             // {re, url, loop, fps}: an animation that replaces walk and idle
    var gesture = null;            // {clip, until}
    var currentClip = null;
    var perceiveEvery = 0;         // ms; 0: the service doesn't want perception now
    var perceivedAt = 0;
    var channels = {};             // the channels the service listens to (besides chat)
    var identityTimer = 0;
    var identityResends = 0;

    function log(text) {
        print("[resident-body " + RESIDENT + "] " + text);
    }

    // ---- the link ----

    function send(message) {
        if (socket && linked) {
            message.jsonrpc = "2.0";
            socket.send(JSON.stringify(message));
        }
    }

    function notify(method, params) {
        send({ method: method, params: params });
    }

    function reply(id, result) {
        send({ id: id, result: result || {} });
    }

    function refuse(id, code, message) {
        send({ id: id, error: { code: code, message: message } });
    }

    function connect() {
        socket = new WebSocket(SERVICE);
        socket.onopen = function () {
            linked = true;
            nextId++;
            socket.send(JSON.stringify({ jsonrpc: "2.0", id: nextId, method: "body_hello",
                                         params: { resident: RESIDENT, token: TOKEN } }));
        };
        socket.onmessage = function (event) {
            var message;
            try {
                message = JSON.parse(event.data);
            } catch (e) {
                return;
            }
            if (message.method) {
                handle(message);
            } else if (message.error) {
                log("the service refused the body: " + JSON.stringify(message.error));
                leave("refused");
            }
        };
        socket.onclose = function () {
            if (linked) {
                log("the link to the service closed");
            }
            linked = false;
            lostAt = Date.now();
        };
        socket.onerror = function () {
            log("the link to the service failed");
        };
    }

    // ---- people and places ----

    function vec(a) {
        return { x: Number(a[0]), y: Number(a[1]), z: Number(a[2]) };
    }

    function arr(v) {
        return [Math.round(v.x * 100) / 100, Math.round(v.y * 100) / 100, Math.round(v.z * 100) / 100];
    }

    function avatarOf(id) {
        var avatar = id ? AvatarList.getAvatar(id) : null;
        return avatar && avatar.position && !Uuid.isNull(avatar.sessionUUID) ? avatar : null;
    }

    function yawOf(orientation) {
        return Quat.safeEulerAngles(orientation).y;
    }

    function yawToward(from, to) {
        var d = Vec3.subtract(to, from);
        return Math.atan2(-d.x, -d.z) * 180 / Math.PI;   // ahead is -z
    }

    function flatDistance(a, b) {
        return Vec3.length({ x: a.x - b.x, y: 0, z: a.z - b.z });
    }

    function eyesOf(avatar) {
        return Vec3.sum(avatar.position, { x: 0, y: HEAD_ABOVE_HIPS * (avatar.scale || 1), z: 0 });
    }

    function speedOf(speed) {
        return typeof speed === "number" ? speed : speed === "run" ? 3.5 : WALK_SPEED;
    }

    function people() {
        var list = [];
        AvatarList.getAvatarIdentifiers().forEach(function (id) {
            if (Uuid.isNull(id) || Uuid.isEqual(id, Avatar.sessionUUID)) {
                return;
            }
            var avatar = avatarOf(id);
            if (!avatar) {
                return;
            }
            list.push({ id: id, name: avatar.displayName || avatar.sessionDisplayName || "",
                        position: arr(avatar.position), yaw: Math.round(yawOf(avatar.orientation)),
                        lookAt: arr(avatar.lookAtPosition) });
        });
        return list;
    }

    function me() {
        return { position: arr(Avatar.position), yaw: Math.round(yawOf(Avatar.orientation)) };
    }

    // ---- requests from the service ----

    function endMovement(completed) {
        if (movement && movement.kind === "route") {
            notify("route_finished", { re: movement.re, completed: completed });
        }
        movement = null;
    }

    function target(params) {
        if (params.person !== undefined) {
            var avatar = avatarOf(params.person);
            if (!avatar) {
                return null;
            }
            return { person: params.person };
        }
        if (params.position !== undefined) {
            return { position: vec(params.position) };
        }
        return null;
    }

    function handle(message) {
        var id = message.id;
        var params = message.params || {};
        var method = message.method;
        var found;
        if (method === "go_to" || method === "follow") {
            found = target(params);
            if (!found) {
                refuse(id, UNKNOWN_PERSON, "nobody with that id is here");
                return;
            }
            endMovement(false);
            movement = { kind: method, re: id, to: found, speed: speedOf(params.speed),
                         within: method === "go_to" ? (params.within || 1.2) : (params.distance || 1.5) };
            facing = null;
            reply(id);
        } else if (method === "route") {
            endMovement(false);
            movement = { kind: "route", re: id, points: params.points.map(vec), index: 0,
                         speed: speedOf(params.speed), pause: params.pause || 0, pausedUntil: 0, within: 0.3 };
            facing = null;
            reply(id);
        } else if (method === "face") {
            if (params.yaw !== undefined) {
                facing = Number(params.yaw);
            } else {
                found = target(params);
                if (!found) {
                    refuse(id, UNKNOWN_PERSON, "nobody with that id is here");
                    return;
                }
                facing = yawToward(Avatar.position, found.person ? avatarOf(found.person).position : found.position);
            }
            reply(id);
        } else if (method === "stop") {
            endMovement(false);
            reply(id);
        } else if (method === "say") {
            Messages.sendMessage("chat", JSON.stringify({ message: String(params.text) }), false);
            reply(id);
        } else if (method === "play_line") {
            lines.push(id);
            Agent.playAvatarSound(SoundCache.getSound(params.url), params.volume === undefined ? 1 : Number(params.volume));
            reply(id);
        } else if (method === "stop_line") {
            Agent.stopAvatarSound();
            reply(id);
        } else if (method === "look_at") {
            found = target(params);
            if (!found) {
                refuse(id, UNKNOWN_PERSON, "nobody with that id is here");
                return;
            }
            looking = found;
            looking.eyesOnly = !!params.eyes_only;
            reply(id);
        } else if (method === "look_away") {
            looking = null;
            if (Avatar.releaseHeadLookAtControl) {
                Avatar.releaseHeadLookAtControl();
                Avatar.releaseEyesLookAtControl();
            }
            reply(id);
        } else if (method === "gesture") {
            var g = GESTURES[params.name];
            if (!g) {
                refuse(id, INVALID_PARAMS, "no gesture named " + params.name);
                return;
            }
            gesture = { clip: g[0], until: Date.now() + g[1] * 1000 };
            reply(id);
        } else if (method === "animate") {
            custom = { re: id, url: params.url, loop: params.loop !== false, fps: params.fps || 30 };
            currentClip = null;
            reply(id);
        } else if (method === "stop_animation") {
            custom = null;
            currentClip = null;
            reply(id);
        } else if (method === "cue") {
            Entities.callEntityServerMethod(params.entity, params.method, params.params || []);
            reply(id);
        } else if (method === "send") {
            Messages.sendMessage(params.channel, params.message, false);
            reply(id);
        } else if (method === "listen") {
            if (params.off) {
                delete channels[params.channel];
                if (params.channel !== "chat") {
                    Messages.unsubscribe(params.channel);
                }
            } else {
                channels[params.channel] = true;
                Messages.subscribe(params.channel);
            }
            reply(id);
        } else if (method === "perceive") {
            perceiveEvery = Math.max(0, Number(params.every) || 0);
            reply(id);
        } else if (method === "leave") {
            reply(id);
            leave("asked");
        } else if (id !== undefined) {
            refuse(id, METHOD_NOT_FOUND, "a body has no " + method);
        }
    }

    Agent.avatarSoundFinished.connect(function (completed) {
        if (lines.length > 0) {
            notify("line_finished", { re: lines.shift(), completed: completed });
        }
    });

    // ---- what the body hears ----

    Messages.subscribe("chat");
    Messages.messageReceived.connect(function (channel, message, senderID, localOnly) {
        if (localOnly || Uuid.isEqual(senderID, Avatar.sessionUUID)) {
            return;
        }
        if (channel === "chat") {
            var data;
            try {
                data = JSON.parse(message);
            } catch (e) {
                return;
            }
            if (data && data.message !== undefined && !data.local) {
                notify("heard", { id: senderID, text: String(data.message) });
            }
        }
        if (channels[channel]) {
            notify("message", { channel: channel, message: message, from: avatarOf(senderID) ? senderID : null });
        }
    });

    // ---- each frame ----

    function turnToward(yaw, dt) {
        var now = yawOf(Avatar.orientation);
        var diff = yaw - now;
        while (diff > 180) {
            diff -= 360;
        }
        while (diff < -180) {
            diff += 360;
        }
        Avatar.orientation = Quat.fromPitchYawRollDegrees(0, now + diff * Math.min(1, TURN_RATE * dt), 0);
        return Math.abs(diff);
    }

    // a step toward the goal; true when it is within `within`
    function walk(goal, within, speed, dt) {
        var position = Avatar.position;
        var distance = flatDistance(position, goal);
        if (distance <= within) {
            return true;
        }
        if (turnToward(yawToward(position, goal), dt) < WALK_WITHIN_DEGREES) {
            var step = Math.min(speed * dt, distance - within);
            var direction = Vec3.normalize({ x: goal.x - position.x, y: 0, z: goal.z - position.z });
            var next = Vec3.sum(position, Vec3.multiply(step, direction));
            next.y = position.y + (goal.y - position.y) * Math.min(1, dt * 2);
            Avatar.position = next;
        }
        return false;
    }

    function move(dt) {
        if (!movement) {
            if (facing !== null && turnToward(facing, dt) < 1) {
                facing = null;
            }
            return 0;
        }
        var speed = movement.speed;
        if (movement.kind === "route") {
            if (Date.now() < movement.pausedUntil) {
                return 0;
            }
            if (walk(movement.points[movement.index], movement.within, speed, dt)) {
                notify("arrived", { re: movement.re, point: movement.index });
                movement.index++;
                movement.pausedUntil = Date.now() + movement.pause * 1000;
                if (movement.index >= movement.points.length) {
                    endMovement(true);
                }
                return 0;
            }
            return speed;
        }
        var goal = movement.to.position;
        if (movement.to.person) {
            var avatar = avatarOf(movement.to.person);
            if (!avatar) {
                if (movement.kind === "follow") {
                    notify("lost", { re: movement.re });
                }
                movement = null;
                return 0;
            }
            goal = avatar.position;
        }
        if (movement.kind === "follow") {
            // stand until they are further than the distance and a little, then walk up to the distance
            if (!movement.walking && flatDistance(Avatar.position, goal) < movement.within + FOLLOW_SLACK) {
                turnToward(yawToward(Avatar.position, goal), dt);
                return 0;
            }
            movement.walking = !walk(goal, movement.within, speed, dt);
            return movement.walking ? speed : 0;
        }
        if (walk(goal, movement.within, speed, dt)) {
            notify("arrived", { re: movement.re });
            movement = null;
            return 0;
        }
        return speed;
    }

    function look() {
        if (!looking || !Avatar.setHeadLookAt) {
            return;
        }
        var point = looking.position;
        if (looking.person) {
            var avatar = avatarOf(looking.person);
            if (!avatar) {
                looking = null;
                return;
            }
            point = eyesOf(avatar);
        }
        if (looking.eyesOnly) {
            Avatar.setEyesLookAt(point);
        } else {
            Avatar.setHeadLookAt(point);
        }
    }

    function play(clip, loop, fps, url) {
        var key = url || clip;
        if (currentClip === key) {
            return;
        }
        currentClip = key;
        var frames = !url && FRAMES[clip];
        if (frames) {
            Avatar.startAnimation(ANIM + clip + ".fbx", fps || 30, 1, loop, !loop, frames[0], frames[1]);
        } else {
            Avatar.startAnimation(url || ANIM + clip + ".fbx", fps || 30, 1, loop, !loop);
        }
    }

    function animate(speed) {
        if (custom) {
            play(null, custom.loop, custom.fps, custom.url);
        } else if (speed > 0) {
            gesture = null;
            play(speed > RUN_ABOVE ? CLIPS.run : CLIPS.walk, true);
        } else if (gesture && Date.now() < gesture.until) {
            play(gesture.clip, false);
        } else if (Agent.isPlayingAvatarSound) {
            gesture = null;
            play(CLIPS.talk, true);
        } else {
            gesture = null;
            play(CLIPS.idle, true);
        }
    }

    // until the engine does it (agent-identity-1), the identity (name, model, attachments) is sent again a few times:
    // the first can go out before the avatar mixer's socket is up and be lost. Changing the name marks it all changed.
    function assertIdentity(dt) {
        if (identityResends >= IDENTITY_RESENDS) {
            return;
        }
        identityTimer += dt;
        if (identityTimer < 3) {
            return;
        }
        identityTimer = 0;
        identityResends++;
        var name = Avatar.displayName;
        Avatar.displayName = name + " ";
        Avatar.displayName = name;
    }

    function leave(reason) {
        log("leaving: " + reason);
        notify("bye", { reason: reason });
        if (socket) {
            socket.close();
        }
        Agent.isAvatar = false;
        Script.stop();
    }

    Script.update.connect(function (dt) {
        assertIdentity(dt);
        if (!ready && linked && identityResends >= 1) {
            ready = true;
            notify("ready", { id: Avatar.sessionUUID, position: arr(Avatar.position),
                              yaw: Math.round(yawOf(Avatar.orientation)), gestures: Object.keys(GESTURES),
                              people: people() });
            log("in the world after " + Math.round((Date.now() - started) / 1000) + " s");
        }
        if (!linked && lostAt && Date.now() - lostAt > LEAVE_AFTER_S * 1000) {
            leave("the service went away");
            return;
        }
        var speed = move(dt);
        look();
        animate(speed);
        if (ready && perceiveEvery > 0 && Date.now() - perceivedAt >= perceiveEvery) {
            perceivedAt = Date.now();
            notify("perception", { me: me(), people: people() });
        }
    });

    Script.scriptEnding.connect(function () {
        if (linked) {
            notify("bye", { reason: "stopping" });
        }
    });

    Agent.isAvatar = true;
    Avatar.displayName = RESIDENT;
    if (AVATAR) {
        Avatar.skeletonModelURL = AVATAR;
    }
    connect();
    log("started; the service is " + SERVICE);
}());
