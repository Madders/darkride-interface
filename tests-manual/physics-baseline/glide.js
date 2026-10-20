//
//  glide.js
//  tests-manual/physics-baseline
//
//  How evenly a keyframed entity glides, per game frame: a box with a velocity of 5 m/s is sampled every 2 ms
//  (faster than any game loop), the distinct positions are the game frames, and the summary gives the step
//  between frames (mean, rms deviation, worst) and the game loop rate. With the simulation presented by
//  interpolation (PhysicsEngine::stepSimulation) the steps are even at any display rate.
//    interface --allowMultipleInstances --url file://<dir>/world.json --testScript <dir>/glide.js
//
(function () {
    var SPEED = 5.0;
    var SECONDS = 6.0;
    var samples = [];
    var box = null;
    var t0 = Date.now();
    function log(s) { print("PHYSICS-GLIDE " + s); }
    function start() {
        box = Entities.addEntity({
            type: "Box", name: "glide-box", shapeType: "box", lifetime: 60, dynamic: false, collisionless: true,
            position: { x: -15, y: 1.5, z: 3 }, dimensions: { x: 1, y: 1, z: 1 }, color: { red: 200, green: 120, blue: 40 },
            velocity: { x: SPEED, y: 0, z: 0 }, damping: 0, gravity: { x: 0, y: 0, z: 0 }
        });
        log("box " + box + " at " + SPEED + " m/s, game loop " + (Test.getGameLoopRate ? Test.getGameLoopRate().toFixed(0) : "?") + " Hz");
        var timer = Script.setInterval(function () {
            var p = Entities.getEntityProperties(box, ["position"]).position;
            samples.push([Date.now() - t0, p.x, p.y, p.z]);
            if (Date.now() - t0 > SECONDS * 1000) {
                Script.clearInterval(timer);
                finish();
            }
        }, 2);
    }
    function finish() {
        // distinct positions = game frames; skip the first and last second (spawn, lifetime)
        var frames = [];
        for (var i = 1; i < samples.length; i++) {
            if (samples[i][1] !== samples[i - 1][1] || samples[i][3] !== samples[i - 1][3]) {
                frames.push(samples[i]);
            }
        }
        var steps = [], gaps = [];
        for (var k = 1; k < frames.length; k++) {
            if (frames[k][0] < 1500 || frames[k][0] > (SECONDS - 1) * 1000) { continue; }
            var dx = frames[k][1] - frames[k - 1][1], dz = frames[k][3] - frames[k - 1][3];
            steps.push(1000 * Math.sqrt(dx * dx + dz * dz));
            gaps.push(frames[k][0] - frames[k - 1][0]);
        }
        var mean = 0, max = 0;
        for (var j = 0; j < steps.length; j++) { mean += steps[j]; if (steps[j] > max) { max = steps[j]; } }
        mean /= steps.length;
        var rms = 0;
        for (j = 0; j < steps.length; j++) { rms += (steps[j] - mean) * (steps[j] - mean); }
        rms = Math.sqrt(rms / steps.length);
        var over = 0;
        for (j = 0; j < steps.length; j++) { if (steps[j] > 1.5 * mean || steps[j] < 0.5 * mean) { over++; } }
        var gmean = 0; for (j = 0; j < gaps.length; j++) { gmean += gaps[j]; } gmean /= gaps.length;
        log("game loop " + (Test.getGameLoopRate ? Test.getGameLoopRate().toFixed(0) : "?") + " Hz; " + frames.length + " frames seen in " +
            samples.length + " samples; step per frame mean " + mean.toFixed(1) + " mm, rms deviation " + rms.toFixed(1) +
            " mm (" + (100 * rms / mean).toFixed(0) + "%), max " + max.toFixed(1) + " mm, " + over + " of " + steps.length +
            " beyond half or one and a half times the mean; frame gap mean " + gmean.toFixed(1) + " ms");
        log("first 24 steps: " + steps.slice(0, 24).map(function (s) { return s.toFixed(0); }).join(" "));
        Entities.deleteEntity(box);
        Script.setTimeout(function () { Test.quit(); }, 500);
    }
    Script.setTimeout(start, 4000);
}());
