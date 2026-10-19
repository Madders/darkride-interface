//
//  monorail-bot.js
//  tests-manual/physics-baseline/bots
//
//  A load-test bot for PHYSICS.md round 5's acceptance test: an assignment-client agent with an avatar. A rider waits
//  for the test's cabin (track.js makes it), parents itself to the cabin and sits in a seat, so the avatar mixer carries
//  its parent and local position to every client as it does for a real rider; a walker walks a circle on the ground
//  beside the station. Neither has physics: these are the "twenty riders and twenty below" that load the mixers and
//  the clients' composition, while the real clients (track.js, observer.js) measure the ride.
//
//  The role comes from the script URL's query (?role=rider or ?role=walker), else from the session id's parity. The
//  seat and the place on the circle come from the session id too, so the bots spread out without talking to each other.
//
//  Run from the domain's persistent scripts (run-monorail.sh writes them into the physics sandbox's config), or by hand:
//    assignment-client -t 2 -a 127.0.0.1 --server-port 42102   with the URL as the assignment's payload
//
//  Distributed under the Apache License, Version 2.0.
//

(function () {
    var CABIN_NAME = "baseline-cabin";
    var POLL_SECONDS = 1.0;
    var SEATS_X = [-1.2, -0.4, 0.4, 1.2];        // across the 4 m cabin
    var SEATS_Z = [-3.0, -1.5, 0.0, 1.5, 3.0];   // along the 8 m cabin
    var SEAT_Y = 1.05;                           // the cabin's floor top (0.25 above its centre) plus a standing avatar's 0.8
    var CIRCLE_CENTRE = { x: 10, y: 0.8, z: -10 };   // on the ground beside the station side of the track
    var CIRCLE_RADIUS = 6.0;
    var WALK_SPEED = 1.2;                        // m/s
    var VIEW_RADIUS = 300;

    var url = Script.resolvePath("");
    var match = /[?&]role=(\w+)/.exec(url);
    var id = Avatar.sessionUUID || "";
    var hash = 0;
    for (var i = 0; i < id.length; i++) { hash = (hash * 31 + id.charCodeAt(i)) >>> 0; }
    var role = match ? match[1] : (hash % 2 === 0 ? "rider" : "walker");
    var seat = hash % (SEATS_X.length * SEATS_Z.length);
    var phase = (hash % 360) * Math.PI / 180;

    var cabin = null;
    var aboard = false;
    var t = 0;
    var sincePoll = 0;

    function log(s) { print("MONORAIL-BOT " + role + " " + s); }

    Agent.isAvatar = true;
    Avatar.displayName = (role === "rider" ? "rider " : "walker ") + (hash % 1000);
    EntityViewer.setPosition({ x: 0, y: 0, z: 0 });
    EntityViewer.setCenterRadius(VIEW_RADIUS);
    EntityViewer.queryOctree();

    if (role === "walker") {
        Avatar.position = Vec3.sum(CIRCLE_CENTRE, { x: CIRCLE_RADIUS * Math.cos(phase), y: 0, z: CIRCLE_RADIUS * Math.sin(phase) });
    } else {
        Avatar.position = { x: 6 + (seat % 5), y: 0.8, z: 6 + Math.floor(seat / 5) };   // waiting beside the platform
    }
    log("started, session " + id + ", seat " + seat + ", phase " + phase.toFixed(2));

    function findCabin() {
        // the one aboard if it is still there, else the newest of that name (run-monorail.sh clears earlier runs'
        // cabins from the world before a run; this is the fallback)
        var found = Entities.findEntitiesByName(CABIN_NAME, { x: 0, y: 0, z: 0 }, VIEW_RADIUS);
        if (cabin && found.indexOf(cabin) >= 0) { return cabin; }
        var best = null, bestAge = Infinity;
        found.forEach(function (id) {
            var age = Entities.getEntityProperties(id, ["age"]).age;
            if (age !== undefined && age < bestAge) { bestAge = age; best = id; }
        });
        return best;
    }

    function board() {
        var local = { x: SEATS_X[seat % SEATS_X.length], y: SEAT_Y, z: SEATS_Z[Math.floor(seat / SEATS_X.length)] };
        Avatar.parentID = cabin;
        Avatar.parentJointIndex = 65535;
        Avatar.localPosition = local;
        Avatar.localOrientation = Quat.IDENTITY;
        aboard = true;
        log("aboard " + cabin + " at " + JSON.stringify(local) + "; parentID now " + Avatar.parentID +
            ", world " + JSON.stringify(Avatar.position));
    }

    function alight() {
        Avatar.parentID = Uuid.NULL;
        Avatar.position = { x: 6 + (seat % 5), y: 0.8, z: 6 + Math.floor(seat / 5) };
        aboard = false;
        log("cabin gone; off");
    }

    Script.update.connect(function (dt) {
        t += dt;
        sincePoll += dt;
        if (sincePoll >= POLL_SECONDS) {
            sincePoll = 0;
            EntityViewer.queryOctree();
            var found = findCabin();
            if (role === "rider") {
                if (found && !aboard) {
                    cabin = found;
                    board();
                } else if (!found && aboard) {
                    alight();
                } else if (found && aboard && Avatar.parentID != cabin) {
                    log("parent lost (" + Avatar.parentID + "); boarding again");
                    cabin = found;
                    board();
                }
            }
        }
        if (role === "walker") {
            var a = phase + (WALK_SPEED / CIRCLE_RADIUS) * t;
            Avatar.position = Vec3.sum(CIRCLE_CENTRE, { x: CIRCLE_RADIUS * Math.cos(a), y: 0, z: CIRCLE_RADIUS * Math.sin(a) });
            Avatar.orientation = Quat.fromPitchYawRollDegrees(0, -a * 180 / Math.PI, 0);
        }
    });

    Script.scriptEnding.connect(function () {
        if (aboard) { Avatar.parentID = Uuid.NULL; }
    });
}());
