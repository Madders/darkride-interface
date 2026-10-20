//
//  AirshipTests.cpp
//  tests/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AirshipTests.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <AirshipModel.h>
#include <VehicleInput.h>

// Add additional qtest functionality (the include order is important!)
#include <test-utils/QTestExtensions.h>

QTEST_MAIN(AirshipTests)

namespace {
    const float G = 9.8f;
    const glm::vec3 UP(0.0f, 1.0f, 0.0f);
    const glm::vec3 RIGHT(1.0f, 0.0f, 0.0f);
    const glm::vec3 FORWARD(0.0f, 0.0f, -1.0f);

    // the test ship: a 36 m envelope of 1,500 m^3 of helium over a 1,369 kg hull with 200 kg of ballast, which is
    // neutral at sea level (the lift is 15,382 N, the hull and ballast weigh 15,376 N)
    AirshipSpec ship() {
        AirshipSpec spec;
        spec.mass = 1369.0f;
        spec.centerOfMass = glm::vec3(0.0f, -0.5f, 0.0f);
        spec.envelopeVolume = 1500.0f;
        spec.centerOfBuoyancy = glm::vec3(0.0f, 7.0f, 0.0f);
        spec.maxThrust = 4000.0f;
        spec.maxVerticalThrust = 3000.0f;
        spec.thrustPoint = glm::vec3(0.0f, 0.0f, 2.0f);
        spec.dragArea = glm::vec3(160.0f, 160.0f, 12.0f);
        spec.angularDragArea = glm::vec3(4.0e5f, 4.0e5f, 2.0e4f);
        spec.rudderArea = 12.0f;
        spec.elevatorArea = 12.0f;
        spec.finPoint = glm::vec3(0.0f, 7.0f, 17.0f);
        spec.ballastRate = 20.0f;
        spec.ventRate = 0.01f;
        return spec;
    }

    AirshipState loaded() {
        AirshipState state;
        state.ballast = 200.0f;
        state.fill = 1.0f;
        return state;
    }

    glm::vec3 netForce(const AirshipForces& f) {
        glm::vec3 sum(0.0f);
        for (const AirshipForce& force : f.forces) {
            sum += force.force;
        }
        return sum;
    }

    // the torque about the centre of mass (the position the forces were computed for)
    glm::vec3 netTorque(const AirshipForces& f, const glm::vec3& position) {
        glm::vec3 sum = f.torque;
        for (const AirshipForce& force : f.forces) {
            sum += glm::cross(force.point - position, force.force);
        }
        return sum;
    }
}

void AirshipTests::atmosphere() {
    QVERIFY(fabsf(AirshipModel::airDensity(0.0f) - 1.225f) < 1.0e-4f);
    QVERIFY(fabsf(AirshipModel::airDensity(1000.0f) - 1.112f) < 2.0e-3f);
    QVERIFY(fabsf(AirshipModel::airDensity(5000.0f) - 0.736f) < 2.0e-3f);
    QCOMPARE(AirshipModel::airDensity(-100.0f), AirshipModel::airDensity(0.0f));   // below sea level counts as sea level
    QVERIFY(AirshipModel::airDensity(20000.0f) > 0.0f);
}

void AirshipTests::buoyancyAgainstWeight() {
    AirshipSpec spec = ship();
    AirshipState state = loaded();
    VehicleInput idle;
    glm::vec3 position(0.0f, -7.5f, 0.0f);   // the centre of mass, with the envelope's centre (7.5 m above it) at sea level
    AirshipForces f = AirshipModel::forces(spec, state, idle, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    float lift = 1500.0f * (1.225f - 0.1786f) * G;
    QVERIFY(fabsf(f.buoyancy - lift) < 1.0f);
    QVERIFY(fabsf(f.ballastWeight - 200.0f * G) < 0.01f);
    // the forces the model adds, plus the hull's own weight (the body's mass under gravity), are nearly nothing
    glm::vec3 net = netForce(f) - UP * (spec.mass * G);
    QVERIFY(fabsf(net.y) < 10.0f);
    QVERIFY(fabsf(net.x) < 1.0e-3f && fabsf(net.z) < 1.0e-3f);
    // standing still, nothing turns it
    QVERIFY(glm::length(netTorque(f, position)) < 1.0e-2f);
    // at 1,000 m the same ship is heavy by about 170 kg
    AirshipForces high = AirshipModel::forces(spec, state, idle, position + UP * 1000.0f, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    QVERIFY(high.buoyancy < f.buoyancy);
    QVERIFY(fabsf((f.buoyancy - high.buoyancy) / G - 170.0f) < 10.0f);
    // the vertical thruster lifts, the propellers push forward at the thrust point
    VehicleInput thrusting;
    thrusting.lift = 1.0f;
    thrusting.throttle = 0.5f;
    AirshipForces t = AirshipModel::forces(spec, state, thrusting, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    glm::vec3 netT = netForce(t) - netForce(f);
    QVERIFY(fabsf(netT.y - 3000.0f) < 1.0e-2f);
    QVERIFY(fabsf(netT.z + 2000.0f) < 1.0e-2f);
}

void AirshipTests::rudderAndElevator() {
    AirshipSpec spec = ship();
    AirshipState state = loaded();
    glm::vec3 position(0.0f, 20.0f, 0.0f);
    glm::vec3 velocity = FORWARD * 10.0f;   // cruising ahead at 10 m/s
    VehicleInput straight;
    AirshipForces base = AirshipModel::forces(spec, state, straight, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    glm::vec3 baseTorque = netTorque(base, position);

    // the rudder: steering right pushes the tail left and turns the nose right (a negative turn about up)
    VehicleInput right;
    right.steering = 1.0f;
    AirshipForces r = AirshipModel::forces(spec, state, right, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    glm::vec3 rudderTorque = netTorque(r, position) - baseTorque;
    QVERIFY(rudderTorque.y < -1000.0f);
    // (the deflected fin's force leans back a little, and the fins stand above the centre of mass: a touch of pitch)
    QVERIFY(fabsf(rudderTorque.x) < 0.3f * fabsf(rudderTorque.y));
    // and by the book: half rho v^2 S C, C = slope x sin(25 deg) capped at the stall, along the deflected normal (so
    // cos(25 deg) of it is across the ship), at a 17 m arm
    float coefficient = glm::min(AirshipModel::FIN_LIFT_SLOPE * sinf(glm::radians(25.0f)), AirshipModel::FIN_STALL);
    float expected = 0.5f * base.airDensity * 100.0f * 12.0f * coefficient * cosf(glm::radians(25.0f)) * 17.0f;
    QVERIFY(fabsf(-rudderTorque.y - expected) < 0.05f * expected);
    VehicleInput left;
    left.steering = -1.0f;
    AirshipForces l = AirshipModel::forces(spec, state, left, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    QVERIFY((netTorque(l, position) - baseTorque).y > 1000.0f);

    // the elevator: pitch up pushes the tail down and raises the nose (a positive turn about right)
    VehicleInput up;
    up.pitch = 1.0f;
    AirshipForces u = AirshipModel::forces(spec, state, up, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    glm::vec3 elevatorTorque = netTorque(u, position) - baseTorque;
    QVERIFY(elevatorTorque.x > 1000.0f);
    QVERIFY(fabsf(elevatorTorque.y) < 0.3f * fabsf(elevatorTorque.x));
    VehicleInput down;
    down.pitch = -1.0f;
    AirshipForces d = AirshipModel::forces(spec, state, down, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    QVERIFY((netTorque(d, position) - baseTorque).x < -1000.0f);

    // standing still the fins do nothing
    AirshipForces still = AirshipModel::forces(spec, state, right, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    QVERIFY(glm::length(netTorque(still, position)) < 1.0e-2f);
}

void AirshipTests::weathercock() {
    AirshipSpec spec = ship();
    AirshipState state = loaded();
    VehicleInput idle;
    glm::vec3 position(0.0f, 20.0f, 0.0f);
    // drifting to the right while moving ahead: the wind comes from ahead and the right, the nose must turn right
    glm::vec3 velocity = FORWARD * 10.0f + RIGHT * 3.0f;
    AirshipForces f = AirshipModel::forces(spec, state, idle, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    QVERIFY(f.sideslip > 0.0f);
    glm::vec3 torque = netTorque(f, position);
    QVERIFY(torque.y < -100.0f);
    // climbing with the nose level: the wind comes from ahead and above and presses the tail down, so the nose rises
    // to point along the velocity
    velocity = FORWARD * 10.0f + UP * 3.0f;
    f = AirshipModel::forces(spec, state, idle, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    QVERIFY(f.incidence < 0.0f);
    torque = netTorque(f, position);
    QVERIFY(torque.x > 100.0f);
    // spinning in still air, the angular drag opposes the spin about every axis
    glm::vec3 spin(0.1f, -0.2f, 0.3f);
    f = AirshipModel::forces(spec, state, idle, position, glm::quat(), glm::vec3(0.0f), spin, G);
    QVERIFY(f.torque.x < 0.0f && f.torque.y > 0.0f && f.torque.z < 0.0f);
}

void AirshipTests::pendulum() {
    AirshipSpec spec = ship();
    AirshipState state = loaded();
    VehicleInput idle;
    glm::vec3 position(0.0f, 20.0f, 0.0f);
    // rolled 10 degrees to the right (a negative turn about forward, which is -z: a positive angle about +z)
    glm::quat rolled = glm::angleAxis(glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    AirshipForces f = AirshipModel::forces(spec, state, idle, position, rolled, glm::vec3(0.0f), glm::vec3(0.0f), G);
    glm::vec3 torque = netTorque(f, position);
    // buoyancy at the envelope's centre, 7.5 m above the centre of mass, rights the ship: the torque turns it back
    QVERIFY(torque.z < -1000.0f);
    float expected = f.buoyancy * 7.5f * sinf(glm::radians(10.0f));
    QVERIFY(fabsf(-torque.z - expected) < 0.02f * expected);
    // pitched 10 degrees nose up (a positive turn about right) it is pitched back down
    glm::quat pitched = glm::angleAxis(glm::radians(10.0f), RIGHT);
    f = AirshipModel::forces(spec, state, idle, position, pitched, glm::vec3(0.0f), glm::vec3(0.0f), G);
    torque = netTorque(f, position);
    QVERIFY(torque.x < -1000.0f);
}

void AirshipTests::drag() {
    AirshipSpec spec = ship();
    AirshipState state = loaded();
    VehicleInput idle;
    glm::vec3 position(0.0f, 20.0f, 0.0f);
    glm::vec3 velocity = FORWARD * 10.0f;
    AirshipForces f = AirshipModel::forces(spec, state, idle, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    glm::vec3 net = netForce(f);
    // the drag is against the motion: half rho v^2 x 12 m^2 along +z
    float expected = 0.5f * f.airDensity * 100.0f * 12.0f;
    QVERIFY(fabsf(net.z - expected) < 0.01f * expected);
    QVERIFY(fabsf(f.airspeed - 10.0f) < 1.0e-3f);
    // the same ship in a 10 m/s tail wind feels no drag
    AirshipForces calm = AirshipModel::forces(spec, state, idle, position, glm::quat(), velocity, glm::vec3(0.0f), G, velocity);
    QVERIFY(fabsf(netForce(calm).z) < 1.0e-2f);
    QVERIFY(calm.airspeed < 1.0e-3f);
    // sideways it is a bluff body: thirteen times the drag
    AirshipForces side = AirshipModel::forces(spec, state, idle, position, glm::quat(), RIGHT * 10.0f, glm::vec3(0.0f), G);
    QVERIFY(netForce(side).x < -12.0f * expected);
    // the derived areas for a 36 x 9 x 9 m hull are of the right order
    glm::vec3 area = AirshipModel::defaultDragArea(glm::vec3(9.0f, 9.0f, 36.0f));
    QVERIFY(area.x > 100.0f && area.z < 20.0f);
    QVERIFY(AirshipModel::defaultAngularDragArea(glm::vec3(9.0f, 9.0f, 36.0f)).x > 1.0e5f);
}

void AirshipTests::ballastAndVenting() {
    AirshipSpec spec = ship();
    AirshipState state = loaded();
    VehicleInput drop;
    drop.trim = 1.0f;
    for (int i = 0; i < 90; ++i) {
        AirshipModel::integrateTrim(spec, state, drop, 1.0f / 90.0f);
    }
    QVERIFY(fabsf(state.ballast - 180.0f) < 0.01f);   // 20 kg/s for a second
    QCOMPARE(state.fill, 1.0f);
    VehicleInput vent;
    vent.trim = -0.5f;
    for (int i = 0; i < 90; ++i) {
        AirshipModel::integrateTrim(spec, state, vent, 1.0f / 90.0f);
    }
    QVERIFY(fabsf(state.fill - 0.995f) < 1.0e-4f);
    QVERIFY(fabsf(state.ballast - 180.0f) < 0.01f);
    // neither goes below nothing
    state.ballast = 1.0f;
    AirshipModel::integrateTrim(spec, state, drop, 10.0f);
    QCOMPARE(state.ballast, 0.0f);
    state.fill = 0.001f;
    AirshipModel::integrateTrim(spec, state, vent, 10.0f);
    QCOMPARE(state.fill, 0.0f);
    // a lighter ship: less ballast, more lift left over
    AirshipState light = loaded();
    light.ballast = 100.0f;
    VehicleInput idle;
    glm::vec3 position(0.0f, -7.5f, 0.0f);
    AirshipForces heavy = AirshipModel::forces(spec, loaded(), idle, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    AirshipForces lighter = AirshipModel::forces(spec, light, idle, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    QVERIFY(fabsf((netForce(lighter).y - netForce(heavy).y) - 100.0f * G) < 0.01f);
}

void AirshipTests::inputPacket() {
    // the three airship controls travel with the four of a car, and come back as they went
    std::vector<VehicleInput> inputs;
    for (uint32_t i = 1; i <= 3; ++i) {
        VehicleInput input;
        input.sequence = 100 + i;
        input.throttle = 0.5f;
        input.steering = -0.25f * i;
        input.pitch = 0.1f * i;
        input.lift = -1.0f;
        input.trim = i == 2 ? 1.0f : 0.0f;
        inputs.push_back(input);
    }
    QUuid id = QUuid::createUuid();
    QByteArray bytes = VehicleInputPacket::pack(id, inputs);
    QCOMPARE(bytes.size(), 16 + 1 + 3 * VehicleInputPacket::INPUT_BYTES);
    QUuid outID;
    std::vector<VehicleInput> out;
    QVERIFY(VehicleInputPacket::unpack(bytes, outID, out));
    QCOMPARE(outID, id);
    QCOMPARE((int)out.size(), 3);
    QCOMPARE(out[1].sequence, (uint32_t)102);
    QCOMPARE(out[1].pitch, 0.2f);
    QCOMPARE(out[1].lift, -1.0f);
    QCOMPARE(out[1].trim, 1.0f);
    QCOMPARE(out[2].trim, 0.0f);
    QVERIFY(!out[0].isIdle());
    QVERIFY(!out[0].sameControls(out[1]));
}
