//
//  TrackTests.cpp
//  tests/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TrackTests.h"

#include <cmath>

#include <glm/gtx/norm.hpp>

#include <TrackCurve.h>
#include <TrackFollower.h>

// Add additional qtest functionality (the include order is important!)
#include <test-utils/QTestExtensions.h>

QTEST_MAIN(TrackTests)

namespace {
    const float STEP = 1.0f / 90.0f;   // the physics substep

    bool near(const glm::vec3& a, const glm::vec3& b, float tolerance) {
        return glm::distance(a, b) <= tolerance;
    }

    TrackCurve square(bool closed) {
        // a 20 m square, anticlockwise seen from above, starting at the +x +z corner
        std::vector<glm::vec3> points = {
            { 10.0f, 0.0f, 10.0f }, { 10.0f, 0.0f, -10.0f }, { -10.0f, 0.0f, -10.0f }, { -10.0f, 0.0f, 10.0f }
        };
        TrackCurve curve;
        curve.build(points, {}, closed);
        return curve;
    }

    TrackCurve straight(float length) {
        TrackCurve curve;
        curve.build({ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -length } }, {}, false);
        return curve;
    }
}

void TrackTests::straightTrack() {
    TrackCurve curve = straight(100.0f);
    QVERIFY(curve.isValid());
    QVERIFY(!curve.isClosed());
    QVERIFY(std::fabs(curve.length() - 100.0f) < 1.0e-3f);
    QVERIFY(std::fabs(curve.arcLengthAtPoint(1) - 100.0f) < 1.0e-3f);

    TrackCurve::Pose start = curve.poseAt(0.0f);
    QVERIFY(near(start.position, glm::vec3(0.0f), 1.0e-4f));
    QVERIFY(near(start.tangent, glm::vec3(0.0f, 0.0f, -1.0f), 1.0e-4f));
    QVERIFY(near(start.up, glm::vec3(0.0f, 1.0f, 0.0f), 1.0e-4f));
    // forward down -Z with +Y up is the identity rotation
    QVERIFY(std::fabs(std::fabs(start.rotation.w) - 1.0f) < 1.0e-5f);

    TrackCurve::Pose middle = curve.poseAt(50.0f);
    QVERIFY(near(middle.position, glm::vec3(0.0f, 0.0f, -50.0f), 1.0e-3f));

    // beyond the ends an open track clamps
    QVERIFY(near(curve.poseAt(150.0f).position, glm::vec3(0.0f, 0.0f, -100.0f), 1.0e-3f));
    QVERIFY(near(curve.poseAt(-5.0f).position, glm::vec3(0.0f), 1.0e-3f));

    QVERIFY(std::fabs(curve.nearest(glm::vec3(1.0f, 0.5f, -30.0f)) - 30.0f) < 1.0e-2f);

    // two coincident points are not a track
    TrackCurve bad;
    bad.build({ { 1.0f, 2.0f, 3.0f }, { 1.0f, 2.0f, 3.0f } }, {}, false);
    QVERIFY(!bad.isValid());
}

void TrackTests::closedLoop() {
    TrackCurve curve = square(true);
    QVERIFY(curve.isValid());
    QVERIFY(curve.isClosed());
    QCOMPARE(curve.segmentCount(), (size_t)4);
    // a Catmull-Rom through the corners bulges a little outside the square: longer than the inscribed circle, not
    // wildly longer than the square
    QVERIFY(curve.length() > 62.8f);
    QVERIFY(curve.length() < 100.0f);

    // the curve passes through every control point at that point's arc length, and nearest() finds it there
    for (size_t i = 0; i < curve.pointCount(); ++i) {
        float s = curve.arcLengthAtPoint(i);
        QVERIFY(near(curve.positionAt(s), curve.points()[i], 2.0e-3f));
        float found = curve.nearest(curve.points()[i]);
        float difference = std::fabs(found - s);
        difference = std::min(difference, curve.length() - difference);
        QVERIFY(difference < 1.0e-2f);
    }
    // the loop closes
    QVERIFY(near(curve.positionAt(curve.length()), curve.positionAt(0.0f), 1.0e-3f));
    QVERIFY(near(curve.positionAt(curve.length() + 7.0f), curve.positionAt(7.0f), 1.0e-3f));
    // a flat track is level everywhere
    for (float s = 0.0f; s < curve.length(); s += 0.5f) {
        TrackCurve::Pose pose = curve.poseAt(s);
        QVERIFY(near(pose.up, glm::vec3(0.0f, 1.0f, 0.0f), 1.0e-4f));
        QVERIFY(std::fabs(pose.position.y) < 1.0e-4f);
    }
    // the polyline for drawing ends where it began
    std::vector<glm::vec3> line = curve.polyline(0.5f);
    QVERIFY(line.size() > 100);
    QVERIFY(near(line.front(), line.back(), 1.0e-3f));
    // and the same vertices as poses, level on a flat track
    std::vector<TrackCurve::Pose> poses = curve.poses(0.5f);
    QCOMPARE(poses.size(), line.size());
    for (size_t i = 0; i < poses.size(); ++i) {
        QVERIFY(near(poses[i].position, line[i], 1.0e-5f));
        QVERIFY(near(poses[i].up, glm::vec3(0.0f, 1.0f, 0.0f), 1.0e-4f));
    }
}

void TrackTests::banking() {
    TrackCurve curve;
    curve.build({ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -100.0f } }, { 30.0f, 30.0f }, false);
    TrackCurve::Pose pose = curve.poseAt(50.0f);
    glm::vec3 expectedUp = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, -1.0f)) * glm::vec3(0.0f, 1.0f, 0.0f);
    QVERIFY(near(pose.up, expectedUp, 1.0e-4f));
    // the rotation's +Y is the banked up and its -Z the tangent
    QVERIFY(near(pose.rotation * glm::vec3(0.0f, 1.0f, 0.0f), expectedUp, 1.0e-4f));
    QVERIFY(near(pose.rotation * glm::vec3(0.0f, 0.0f, -1.0f), pose.tangent, 1.0e-4f));

    // a bank at one end only is interpolated along
    TrackCurve ramp;
    ramp.build({ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -100.0f } }, { 0.0f, 30.0f }, false);
    QVERIFY(near(ramp.poseAt(0.0f).up, glm::vec3(0.0f, 1.0f, 0.0f), 1.0e-4f));
    QVERIFY(near(ramp.poseAt(100.0f).up, expectedUp, 1.0e-3f));
}

void TrackTests::shuttleSchedule() {
    // an open 200 m track: the ends are stops; 3 s at the start, straight back at the far end
    TrackCurve curve = straight(200.0f);
    TrackFollower::Schedule schedule;
    schedule.speed = 5.0f;
    schedule.acceleration = 1.0f;
    schedule.dwell = { 3.0f, 0.0f };

    TrackFollower follower;
    follower.reset(curve, schedule, 0.0f, 1);
    // placed at the first station, the train waits its 3 s there first
    QCOMPARE(follower.state().station, 0);
    QVERIFY(follower.isDwelling());

    // off at 3 s; accelerate 5 s over 12.5 m, cruise 175 m in 35 s, brake 5 s: there at 48 s
    float peak = 0.0f;
    float time = 0.0f;
    bool arrived = false;
    float arrivalTime = 0.0f;
    while (time < 60.0f) {
        follower.advance(curve, schedule, STEP);
        time += STEP;
        peak = std::max(peak, follower.state().speed);
        if (!arrived && follower.state().direction < 0) {
            arrived = true;
            arrivalTime = time;
        }
    }
    QVERIFY(std::fabs(peak - 5.0f) < 1.0e-3f);
    QVERIFY(arrived);
    QVERIFY(std::fabs(arrivalTime - 48.0f) < 0.5f);
    // on the way back now, no dwell at that end
    QCOMPARE(follower.state().station, 0);
    QVERIFY(follower.state().s < 200.0f);
    QVERIFY(follower.state().s > 100.0f);

    // back at the start by 93 s, standing for 3 s, then off again
    while (time < 94.0f) {
        follower.advance(curve, schedule, STEP);
        time += STEP;
    }
    QVERIFY(follower.isDwelling());
    QVERIFY(std::fabs(follower.state().s) < 1.0e-3f);
    QVERIFY(follower.state().speed == 0.0f);
    while (time < 97.0f) {
        follower.advance(curve, schedule, STEP);
        time += STEP;
    }
    QVERIFY(!follower.isDwelling());
    QCOMPARE(follower.state().direction, 1);
    QVERIFY(follower.state().speed > 0.0f);
}

void TrackTests::loopStations() {
    // a loop with two stations, at points 0 and 2
    TrackCurve curve = square(true);
    TrackFollower::Schedule schedule;
    schedule.speed = 5.0f;
    schedule.acceleration = 2.0f;
    schedule.dwell = { 2.0f, 0.0f, 2.0f, 0.0f };

    TrackFollower follower;
    follower.reset(curve, schedule, 0.0f, 1);
    // placed at station 0, it dwells there first, then heads for station 2
    QCOMPARE(follower.state().station, 0);
    QVERIFY(follower.isDwelling());

    float halfway = curve.arcLengthAtPoint(2);
    bool stoppedHalfway = false;
    float time = 0.0f;
    while (time < 40.0f) {
        follower.advance(curve, schedule, STEP);
        time += STEP;
        if (follower.isDwelling() && std::fabs(follower.state().s - halfway) < 1.0e-3f) {
            stoppedHalfway = true;
        }
    }
    QVERIFY(stoppedHalfway);
    // and round again: the direction never changes on a loop, the stations alternate
    QCOMPARE(follower.state().direction, 1);
    QVERIFY(follower.state().station == 0 || follower.state().station == 2);

    // a loop with no stations circulates at the cruise
    TrackFollower::Schedule free;
    free.speed = 5.0f;
    TrackFollower runner;
    runner.reset(curve, free, 0.0f, 1);
    QCOMPARE(runner.state().station, -1);
    for (int i = 0; i < 90 * 30; ++i) {
        runner.advance(curve, free, STEP);
    }
    QVERIFY(std::fabs(runner.state().speed - 5.0f) < 1.0e-3f);
}

void TrackTests::carVelocities() {
    TrackCurve curve = straight(200.0f);
    TrackFollower::Schedule schedule;
    schedule.speed = 5.0f;
    schedule.acceleration = 1.0f;
    TrackFollower follower;
    follower.reset(curve, schedule, 0.0f, 1);
    for (int i = 0; i < 90 * 10; ++i) {
        follower.advance(curve, schedule, STEP);
    }
    glm::vec3 position, linear, angular;
    glm::quat rotation;
    follower.carPose(curve, 0.0f, position, rotation, linear, angular);
    QVERIFY(near(linear, glm::vec3(0.0f, 0.0f, -5.0f), 1.0e-3f));
    QVERIFY(glm::length(angular) < 1.0e-4f);
    // a second car 8 m back sits 8 m behind along the track
    glm::vec3 position2, linear2, angular2;
    glm::quat rotation2;
    follower.carPose(curve, -8.0f, position2, rotation2, linear2, angular2);
    QVERIFY(near(position2, position + glm::vec3(0.0f, 0.0f, 8.0f), 1.0e-3f));

    // on a loop the car turns: the angular velocity is about the up axis, and it is v / r on a corner
    TrackCurve loop = square(true);
    TrackFollower::Schedule free;
    free.speed = 5.0f;
    TrackFollower runner;
    runner.reset(loop, free, 0.0f, 1);
    for (int i = 0; i < 90 * 20; ++i) {
        runner.advance(loop, free, STEP);
    }
    float maxTurn = 0.0f;
    for (int i = 0; i < 90 * 20; ++i) {
        runner.advance(loop, free, STEP);
        runner.carPose(loop, 0.0f, position, rotation, linear, angular);
        maxTurn = std::max(maxTurn, glm::length(angular));
        QVERIFY(std::fabs(glm::length(linear) - 5.0f) < 1.0e-3f);
        if (glm::length(angular) > 1.0e-3f) {
            QVERIFY(std::fabs(std::fabs(angular.y) - glm::length(angular)) < 1.0e-3f);
        }
    }
    QVERIFY(maxTurn > 0.2f);
    QVERIFY(maxTurn < 2.0f);
}

void TrackTests::determinism() {
    TrackCurve curve = square(true);
    TrackFollower::Schedule schedule;
    schedule.dwell = { 1.0f, 0.0f, 1.0f, 0.0f };
    TrackFollower a, b;
    a.reset(curve, schedule, 3.0f, 1);
    b.reset(curve, schedule, 3.0f, 1);
    for (int i = 0; i < 90 * 60; ++i) {
        a.advance(curve, schedule, STEP);
        b.advance(curve, schedule, STEP);
        QCOMPARE(a.state().s, b.state().s);
        QCOMPARE(a.state().speed, b.state().speed);
        QCOMPARE(a.state().dwellRemaining, b.state().dwellRemaining);
    }
    // the state round-trips
    TrackFollower c;
    c.setState(a.state());
    c.advance(curve, schedule, STEP);
    a.advance(curve, schedule, STEP);
    QCOMPARE(c.state().s, a.state().s);
}

// Round 5: a train state handed from one follower to another and run on in the same substeps lands on the same place;
// a state run on in one long step would not (the profile integrates per step), which is why the server and its clients
// both step the state in 1/90 s and why a client catching up uses a whole number of substeps.
void TrackTests::stateHandover() {
    TrackCurve curve = square(true);
    TrackFollower::Schedule schedule;
    schedule.speed = 5.0f;
    schedule.acceleration = 2.0f;
    schedule.dwell = { 3.0f, 0.0f, 3.0f, 0.0f };

    // the server: a train started at the first station, run for 20 s in substeps
    TrackFollower server;
    server.reset(curve, schedule, 0.0f);
    const int STEPS_BEFORE = 90 * 20;
    for (int i = 0; i < STEPS_BEFORE; ++i) {
        server.advance(curve, schedule, STEP);
    }
    TrackFollower::State published = server.state();   // what goes on the wire
    QVERIFY(published.speed > 0.0f);                   // moving between stations by then

    // the server goes on for 1.5 s (a heartbeat and a half of latency, say)
    const int STEPS_AFTER = 135;
    for (int i = 0; i < STEPS_AFTER; ++i) {
        server.advance(curve, schedule, STEP);
    }

    // a client that adopts the published state and catches up in the same substeps agrees to the micrometre
    TrackFollower client;
    client.setState(published);
    for (int i = 0; i < STEPS_AFTER; ++i) {
        client.advance(curve, schedule, STEP);
    }
    QVERIFY(std::fabs(client.state().s - server.state().s) < 1.0e-4f);
    QVERIFY(std::fabs(client.state().speed - server.state().speed) < 1.0e-5f);
    QCOMPARE(client.state().station, server.state().station);
    QCOMPARE(client.state().direction, server.state().direction);

    // the same catch-up in one step lands elsewhere while the speed changes: the reason for the substeps
    TrackFollower lazy;
    lazy.setState(published);
    lazy.advance(curve, schedule, STEPS_AFTER * STEP);
    bool speedChanged = std::fabs(published.speed - server.state().speed) > 1.0e-3f;
    if (speedChanged) {
        QVERIFY(std::fabs(lazy.state().s - server.state().s) > 1.0e-3f);
    }

    // a state adopted while the train stands keeps its dwell: the client leaves the station when the server does
    TrackFollower standing;
    standing.reset(curve, schedule, 0.0f);   // at station 0 with its 3 s dwell
    TrackFollower::State atRest = standing.state();
    QVERIFY(atRest.dwellRemaining > 0.0f);
    TrackFollower follower;
    follower.setState(atRest);
    int stepsToDepart = 0;
    while (follower.state().speed == 0.0f && stepsToDepart < 90 * 10) {
        follower.advance(curve, schedule, STEP);
        standing.advance(curve, schedule, STEP);
        ++stepsToDepart;
    }
    QVERIFY(stepsToDepart > 0 && stepsToDepart < 90 * 4);
    QVERIFY(std::fabs(follower.state().s - standing.state().s) < 1.0e-5f);
}
