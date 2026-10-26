//
//  camera-collision.js
//  tests-manual/physics-baseline
//
//  Camera collision for the third-person camera (ViewState::collideCameraBoom, View > Camera Collision), serverless on
//  world.json's ground. The avatar faces -z with the camera behind it at +z. Cases, each read off Camera.position and
//  the avatar's eye every frame:
//    open        nothing behind: the camera stands at the boom's full length (the reference for the rest)
//    walk back   a wall 3 m behind; the avatar is walked back into it: no frame may show the camera behind the wall's
//                face (it moves in at once)
//    ease out    the wall is deleted with the avatar against it: the camera goes back out, eased (95 % in 0.2-0.6 s)
//    look at     the same wall in the "look at" camera mode: held in front of it
//    glass       a wall you can see through (alpha 0.3): the camera goes through it, to its full length
//    small ball  a moving 0.3 m ball on the boom: ignored
//    big box     a moving 1.5 m box on the boom: stops the camera
//    off         View > Camera Collision unchecked, the wall behind: the camera goes through it, as before
//    strafe in   strafing sideways into a corridor whose back wall comes in from the side of the boom; and
//                close beside that wall, as Caity was (the wall's end passes between her and the camera)
//    strafe along strafing into a wall beside the avatar and along it, pressed against it
//    turn at end turning on the spot at a thin wall's end, so the boom sweeps across the wall's narrow side
//    angled      pressed against a wall with the head turned towards it, the boom running back into the wall
//                (in these three, no frame may have a corner or the centre of the camera's near plane inside a wall:
//                Caity's strafing bug of 10-25, the probe started at the eye, which can be at or in a wall)
//    script      Camera.collideBoom (a script's chase camera): a wall on the boom shortens it; the wall named in its
//                ignore list, or a parent of the wall named, lets it through; with the option off, the whole length
//  Pictures of the wall case with the option on and off. Prints CAMERA-COLLISION lines; samples in
//  camera-collision.json.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

(function () {
    var LIFETIME = 300;
    var WALL_THICKNESS = 0.3;
    var MENU_OPTION = "Camera Collision";
    var failures = 0;
    var samples = [];
    var made = [];
    var fullLength = null;
    var config = typeof CAMERA_CONFIG !== "undefined" ? CAMERA_CONFIG : {};
    var walls = [];   // { id, center, dimensions } of the axis-aligned walls the near-plane check tests

    function log(s) { print("CAMERA-COLLISION " + s); }
    function after(seconds, f) { Script.setTimeout(f, seconds * 1000); }
    function check(ok, text) {
        if (!ok) {
            failures++;
        }
        log((ok ? "PASS " : "FAIL ") + text);
    }
    function eye() { return MyAvatar.getDefaultEyePosition(); }
    function boomLength() { return Vec3.distance(Camera.position, eye()); }

    function add(properties) {
        properties.lifetime = LIFETIME;
        var id = Entities.addEntity(properties);
        made.push(id);
        return id;
    }
    // a wall across the boom, its face (the side toward the avatar) at z = face
    function wall(face, alpha) {
        return add({ type: "Box", name: "camera-collision wall", position: { x: 0, y: 2, z: face + WALL_THICKNESS / 2 },
                     dimensions: { x: 8, y: 4, z: WALL_THICKNESS }, color: { red: 200, green: 120, blue: 60 },
                     alpha: alpha === undefined ? 1.0 : alpha, dynamic: false, collisionless: false });
    }

    // stand the avatar at z, facing -z, and settle
    function stand(z, then) {
        MyAvatar.motorVelocity = { x: 0, y: 0, z: 0 };
        MyAvatar.position = { x: 0, y: 1.0, z: z };
        MyAvatar.orientation = Quat.IDENTITY;
        MyAvatar.headPitch = 0;
        after(1.5, then);
    }

    // records the boom's length every frame for the seconds; then(list of { t, length, cameraZ })
    function record(seconds, then) {
        var list = [];
        var t0 = Date.now();
        function tick() {
            var t = (Date.now() - t0) / 1000;
            list.push({ t: t, length: boomLength(), cameraZ: Camera.position.z, avatarZ: MyAvatar.position.z });
            if (t >= seconds) {
                Script.update.disconnect(tick);
                then(list);
            }
        }
        Script.update.connect(tick);
    }

    function steps(list) {
        var i = 0;
        function next() {
            if (i < list.length) {
                list[i++](next);
            }
        }
        next();
    }

    function open(next) {
        stand(0, function () {
            record(1.0, function (list) {
                fullLength = list[list.length - 1].length;
                samples.push({ name: "open", list: list });
                check(fullLength > 0.5, "open: the camera stands " + fullLength.toFixed(3) + " m from the eye");
                next();
            });
        });
    }

    function walkBack(next) {
        var face = 3.0;
        stand(0, function () {
            wall(face);
            after(1.0, function () {
                // walked back (+z) at 1 m/s until the wall stops the avatar
                MyAvatar.motorReferenceFrame = "world"; MyAvatar.motorMode = "simple"; MyAvatar.motorTimescale = 0.25;
                MyAvatar.motorVelocity = { x: 0, y: 0, z: 1.0 };
                record(4.0, function (list) {
                    MyAvatar.motorVelocity = { x: 0, y: 0, z: 0 }; MyAvatar.motorTimescale = 1000000;
                    samples.push({ name: "walk back", list: list });
                    var worst = -1000;
                    list.forEach(function (s) { worst = Math.max(worst, s.cameraZ - face); });
                    var last = list[list.length - 1];
                    check(worst <= 0.0, "walk back: the camera's furthest past the wall's face over " + list.length +
                          " frames: " + worst.toFixed(3) + " m (must be <= 0); at the end the avatar is at z " +
                          last.avatarZ.toFixed(2) + ", the camera " + last.length.toFixed(3) + " m from the eye");
                    next();
                });
            });
        });
    }

    function easeOut(next) {
        // the avatar against the wall from walk back; the wall goes
        var walls = Entities.findEntitiesByName("camera-collision wall", MyAvatar.position, 20, true);
        var before = boomLength();
        walls.forEach(function (id) { Entities.deleteEntity(id); });
        record(1.5, function (list) {
            samples.push({ name: "ease out", list: list });
            var reached = null;
            list.forEach(function (s) {
                if (reached === null && s.length >= 0.95 * fullLength) {
                    reached = s.t;
                }
            });
            check(reached !== null && reached >= 0.2 && reached <= 0.6, "ease out: from " + before.toFixed(3) +
                  " m to 95 % of " + fullLength.toFixed(3) + " m in " + (reached === null ? "never" : reached.toFixed(2) + " s") +
                  " (0.2 to 0.6 s)");
            next();
        });
    }

    // a case with something on the boom; expectHeld: the camera must stay short of fullLength
    function boomCase(name, make, expectHeld, next, mode) {
        stand(0, function () {
            if (mode) {
                Camera.mode = mode;
            }
            var id = make();
            after(1.5, function () {
                record(0.5, function (list) {
                    var length = list[list.length - 1].length;
                    samples.push({ name: name, list: list });
                    var held = length < fullLength - 0.2;
                    check(held === expectHeld, name + ": the camera " + length.toFixed(3) + " m from the eye (" +
                          (expectHeld ? "held in" : "full length") + " expected; full " + fullLength.toFixed(3) + ")");
                    Entities.deleteEntity(id);
                    Camera.mode = "third person";
                    after(1.0, next);
                });
            });
        });
    }

    function picture(name, then) {
        Window.takeSnapshot(false, false, 0, name);
        log("picture " + name);
        after(1.5, then);
    }

    // an axis-aligned box the near-plane check knows about
    function box(name, center, dimensions) {
        var id = add({ type: "Box", name: "camera-collision " + name, position: center, dimensions: dimensions,
                       color: { red: 200, green: 120, blue: 60 }, dynamic: false, collisionless: false });
        walls.push({ id: id, center: center, dimensions: dimensions });
        return id;
    }
    function clearWalls() {
        walls.forEach(function (w) { Entities.deleteEntity(w.id); });
        walls = [];
    }
    function inside(p, w) {
        return Math.abs(p.x - w.center.x) < w.dimensions.x / 2 && Math.abs(p.y - w.center.y) < w.dimensions.y / 2 &&
            Math.abs(p.z - w.center.z) < w.dimensions.z / 2;
    }
    // signed distance from a point to an axis-aligned wall: positive outside, negative inside
    function clearance(p, w) {
        var dx = Math.abs(p.x - w.center.x) - w.dimensions.x / 2;
        var dy = Math.abs(p.y - w.center.y) - w.dimensions.y / 2;
        var dz = Math.abs(p.z - w.center.z) - w.dimensions.z / 2;
        var outside = Math.sqrt(Math.pow(Math.max(dx, 0), 2) + Math.pow(Math.max(dy, 0), 2) + Math.pow(Math.max(dz, 0), 2));
        return outside > 0 ? outside : Math.max(dx, dy, dz);
    }
    // the camera's near plane: its centre and four corners, from the camera's frustum
    function nearPlane() {
        var f = Camera.frustum || {};
        var near = f.nearClip || 0.08;
        var vfov = (f.fieldOfView || 45) * Math.PI / 180;
        var aspect = f.aspectRatio || 16 / 9;
        var hh = near * Math.tan(vfov / 2), hw = hh * aspect;
        var q = Camera.orientation, c = Camera.position;
        return [[0, 0], [-1, -1], [1, -1], [-1, 1], [1, 1]].map(function (k) {
            return Vec3.sum(c, Vec3.multiplyQbyV(q, { x: k[0] * hw, y: k[1] * hh, z: -near }));
        });
    }
    // records frames for the seconds while act(t) moves the avatar; counts the frames with the near plane in a wall
    function nearPlaneCase(name, seconds, act, next) {
        Camera.mode = config.mode || "look at";   // the menu's Third Person
        var t0 = Date.now(), frames = 0, bad = 0, firstBad = null, least = 1000;
        function tick() {
            var t = (Date.now() - t0) / 1000;
            act(t);
            frames++;
            var points = nearPlane();
            var hit = walls.some(function (w) { return points.some(function (p) { return inside(p, w); }); });
            walls.forEach(function (w) { points.forEach(function (p) { least = Math.min(least, clearance(p, w)); }); });
            if (hit) {
                bad++;
                if (firstBad === null) {
                    firstBad = t;
                }
            }
            samples.push({ name: name, t: t, camera: Camera.position, eye: eye(), inWall: hit });
            if (t >= seconds) {
                Script.update.disconnect(tick);
                MyAvatar.motorVelocity = { x: 0, y: 0, z: 0 }; MyAvatar.motorTimescale = 1000000;
                Camera.mode = "third person";
                check(bad === 0, name + " (" + (config.mode || "look at") + ", field of view " + ((Camera.frustum || {}).fieldOfView || "?") +
                      "): the near plane in a wall in " + bad + " of " + frames + " frames" +
                      (firstBad === null ? "" : " (first at " + firstBad.toFixed(2) + " s)") + "; least clearance " + least.toFixed(3) + " m");
                clearWalls();
                after(1.0, next);
            }
        }
        Script.update.connect(tick);
    }
    function strafe(vx) {   // sideways in the avatar's frame (it faces -z): +x right, -x left
        MyAvatar.motorReferenceFrame = "world"; MyAvatar.motorMode = "simple"; MyAvatar.motorTimescale = 0.25;
        MyAvatar.motorVelocity = { x: vx, y: 0, z: 0 };
    }

    // strafing right into a corridor (along x, 1.2 m wide) whose walls are in front of and behind the avatar
    function strafeIn(next) {
        stand(0, function () {
            box("corridor back", { x: 4.0, y: 1.5, z: 0.75 }, { x: 6.0, y: 3.0, z: 0.3 });
            box("corridor front", { x: 4.0, y: 1.5, z: -0.75 }, { x: 6.0, y: 3.0, z: 0.3 });
            after(1.0, function () {
                nearPlaneCase("strafe in", 5.0, function () { strafe(1.0); }, next);
            });
        });
    }
    // Caity's case (10-25): close beside the corridor's back wall, side-on, strafing into the corridor with the camera at
    // its full distance behind, so the wall's end passes between the avatar and the camera
    function strafeInClose(next) {
        stand(0.3, function () {
            box("corridor back", { x: 4.0, y: 1.5, z: 0.75 }, { x: 6.0, y: 3.0, z: 0.3 });
            box("corridor front", { x: 4.0, y: 1.5, z: -0.75 }, { x: 6.0, y: 3.0, z: 0.3 });
            after(1.0, function () {
                nearPlaneCase("strafe in close", 5.0, function () { strafe(1.0); }, next);
            });
        });
    }
    // strafing left into a wall beside the avatar (along z) and on along it, pressed against it
    function strafeAlong(next) {
        stand(0, function () {
            box("wall beside", { x: -0.65, y: 1.5, z: 0.0 }, { x: 0.3, y: 3.0, z: 8.0 });
            after(1.0, function () {
                nearPlaneCase("strafe along", 4.0, function (t) {
                    MyAvatar.motorReferenceFrame = "world"; MyAvatar.motorMode = "simple"; MyAvatar.motorTimescale = 0.25;
                    MyAvatar.motorVelocity = { x: -1.0, y: 0, z: t > 1.5 ? 0.8 : 0 };
                }, next);
            });
        });
    }
    // pressed against a wall on the left with the head (and so the look-at camera) turned 35 degrees towards it, so the
    // boom runs back into the wall from right beside it
    function angledIntoWall(next) {
        stand(0, function () {
            box("wall beside", { x: -0.65, y: 1.5, z: 0.0 }, { x: 0.3, y: 3.0, z: 8.0 });
            after(1.0, function () {
                nearPlaneCase("angled into wall", 4.0, function (t) {
                    MyAvatar.motorReferenceFrame = "world"; MyAvatar.motorMode = "simple"; MyAvatar.motorTimescale = 0.25;
                    MyAvatar.motorVelocity = { x: -1.0, y: 0, z: 0 };
                    MyAvatar.headYaw = t < 2 ? 35 * t / 2 : 35;
                }, function () {
                    MyAvatar.headYaw = 0;
                    next();
                });
            });
        });
    }

    // turning on the spot beside a thin wall's end (the wall runs back from just behind the avatar's left)
    function turnAtEnd(next) {
        stand(0, function () {
            box("thin wall", { x: -0.45, y: 1.5, z: 2.3 }, { x: 0.3, y: 3.0, z: 4.0 });
            after(1.0, function () {
                nearPlaneCase("turn at end", 8.0, function (t) {
                    MyAvatar.orientation = Quat.fromPitchYawRollDegrees(0, t * 90, 0);   // a full turn in 4 s, twice
                }, next);
            });
        });
    }

    // Camera.collideBoom from this script's thread: a vehicle's chase camera asks it with the vehicle in its ignore list
    function scriptBoom(next) {
        if (typeof Camera.collideBoom !== "function") {
            check(false, "script: no Camera.collideBoom in this build");
            next();
            return;
        }
        stand(0, function () {
            var parent = add({ type: "Box", name: "camera-collision parent", position: { x: 0, y: 0.25, z: -6 },
                               dimensions: { x: 0.5, y: 0.5, z: 0.5 }, dynamic: false, collisionless: false });
            // the wall as the parent's child, its face 1 m behind the avatar as wall(1.0) puts it
            var id = add({ type: "Box", name: "camera-collision wall", parentID: parent,
                           localPosition: { x: 0, y: 2 - 0.25, z: 1.0 + WALL_THICKNESS / 2 + 6 },
                           dimensions: { x: 8, y: 4, z: WALL_THICKNESS }, color: { red: 200, green: 120, blue: 60 },
                           dynamic: false, collisionless: false });
            after(1.5, function () {
                var pivot = eye();
                var wanted = Vec3.sum(pivot, { x: 0, y: 0, z: 3.0 });
                var blocked = Camera.collideBoom(pivot, wanted, []);
                var named = Camera.collideBoom(pivot, wanted, [id]);
                var parentNamed = Camera.collideBoom(pivot, wanted, [parent]);
                Menu.setIsOptionChecked(MENU_OPTION, false);
                var off = Camera.collideBoom(pivot, wanted, []);
                Menu.setIsOptionChecked(MENU_OPTION, true);
                check(blocked < 1.0, "script: a wall 1 m behind holds a 3 m boom to " + blocked.toFixed(3) + " m");
                check(Math.abs(named - 3.0) < 0.01, "script: the wall named in the ignore list lets it through: " + named.toFixed(3) + " m");
                check(Math.abs(parentNamed - 3.0) < 0.01, "script: the wall's parent named lets it through: " + parentNamed.toFixed(3) + " m");
                check(Math.abs(off - 3.0) < 0.01, "script: with the option off, the whole length: " + off.toFixed(3) + " m");
                Entities.deleteEntity(parent);   // the wall goes with it, as its child
                after(1.0, next);
            });
        });
    }

    function pictures(next) {
        stand(0, function () {
            var id = wall(1.0);
            after(1.5, function () {
                picture("camera-collision-on", function () {
                    Menu.setIsOptionChecked(MENU_OPTION, false);
                    after(1.0, function () {
                        picture("camera-collision-off", function () {
                            Menu.setIsOptionChecked(MENU_OPTION, true);
                            Entities.deleteEntity(id);
                            after(1.0, next);
                        });
                    });
                });
            });
        });
    }

    function finish() {
        made.forEach(function (id) { Entities.deleteEntity(id); });
        Test.saveObject({ failures: failures, samples: samples }, "camera-collision.json");
        log((failures === 0 ? "ALL PASS" : failures + " FAILED"));
        after(1.0, function () { Test.quit(); });
    }

    function start(tries) {
        if (!Window.isPhysicsEnabled()) {
            if (tries < 120) {
                after(1.0, function () { start(tries + 1); });
            } else {
                log("FAIL no physics after 120 s");
                Test.quit();
            }
            return;
        }
        if (config.avatarURL) {
            MyAvatar.useFullAvatarURL(config.avatarURL);
            log("wearing " + config.avatarURL);
        }
        Camera.mode = "third person";
        Menu.setIsOptionChecked(MENU_OPTION, true);
        log("option " + MENU_OPTION + ": " + Menu.isOptionChecked(MENU_OPTION));
        after(config.avatarURL ? 12 : 0, function () { steps([
            open,
            walkBack,
            easeOut,
            function (next) { boomCase("look at", function () { return wall(1.0); }, true, next, "look at"); },
            function (next) { boomCase("glass", function () { return wall(1.0, 0.3); }, false, next); },
            function (next) {
                boomCase("small ball", function () {
                    return add({ type: "Sphere", name: "camera-collision ball", position: Vec3.sum(eye(), { x: 0, y: 0, z: 0.8 }),
                                 dimensions: { x: 0.3, y: 0.3, z: 0.3 }, dynamic: true, gravity: { x: 0, y: 0, z: 0 },
                                 collisionless: false, color: { red: 40, green: 160, blue: 40 } });
                }, false, next);
            },
            function (next) {
                boomCase("big box", function () {
                    return add({ type: "Box", name: "camera-collision box", position: Vec3.sum(eye(), { x: 0, y: 0, z: 1.6 }),
                                 dimensions: { x: 1.5, y: 1.5, z: 1.5 }, dynamic: true, gravity: { x: 0, y: 0, z: 0 },
                                 collisionless: false, color: { red: 40, green: 40, blue: 160 } });
                }, true, next);
            },
            function (next) {
                Menu.setIsOptionChecked(MENU_OPTION, false);
                boomCase("off", function () { return wall(1.0); }, false, function () {
                    Menu.setIsOptionChecked(MENU_OPTION, true);
                    next();
                });
            },
            strafeIn,
            strafeInClose,
            strafeAlong,
            turnAtEnd,
            angledIntoWall,
            scriptBoom,
            pictures,
            finish
        ]); });   // after the worn avatar has loaded
    }

    after(2, function () { start(0); });
}());
