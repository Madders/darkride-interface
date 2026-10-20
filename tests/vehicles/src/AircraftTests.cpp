//
//  AircraftTests.cpp
//  tests/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AircraftTests.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <AircraftModel.h>
#include <VehicleInput.h>

#include <test-utils/QTestExtensions.h>

QTEST_MAIN(AircraftTests)

namespace {
    const float G = 9.8f;
    const glm::vec3 UP(0.0f, 1.0f, 0.0f);
    const glm::vec3 RIGHT(1.0f, 0.0f, 0.0f);
    const glm::vec3 FORWARD(0.0f, 0.0f, -1.0f);

    // the test craft: 1,200 kg on a 16 m^2 wing of 10 m span (aspect ratio 6.25), 20 kN of thrust, a tail 4.5 m aft
    AircraftSpec craft() {
        AircraftSpec spec;
        spec.mass = 1200.0f;
        spec.centerOfMass = glm::vec3(0.0f, -0.2f, 0.0f);
        spec.maxThrust = 20000.0f;
        spec.thrustPoint = glm::vec3(0.0f, 0.3f, 0.0f);
        spec.wingArea = 16.0f;
        spec.wingSpan = 10.0f;
        spec.wingPoint = glm::vec3(0.0f, 0.2f, 0.2f);
        spec.elevatorArea = 3.0f;
        spec.rudderArea = 1.5f;
        spec.finPoint = glm::vec3(0.0f, 0.5f, 4.5f);
        spec.maxFinAngle = 25.0f;
        spec.dragArea = glm::vec3(3.0f, 6.0f, 0.8f);
        spec.angularDragArea = glm::vec3(600.0f, 600.0f, 300.0f);
        return spec;
    }

    glm::vec3 netForce(const AircraftForces& f) {
        glm::vec3 sum(0.0f);
        for (const AirshipForce& force : f.forces) {
            sum += force.force;
        }
        return sum;
    }

    // the torque about the centre of mass (at `position`) from the forces at their points, plus the model's own
    glm::vec3 netTorque(const AircraftForces& f, const glm::vec3& position) {
        glm::vec3 sum = f.torque;
        for (const AirshipForce& force : f.forces) {
            sum += glm::cross(force.point - position, force.force);
        }
        return sum;
    }
}

void AircraftTests::liftAndDragCoefficients() {
    float ar = 6.25f;
    // the slope: 2 pi AR / (AR + 2) per radian; at 5 degrees
    float slope = 2.0f * glm::pi<float>() * ar / (ar + 2.0f);
    float cl5 = AircraftModel::liftCoefficient(glm::radians(5.0f), ar);
    QVERIFY(fabsf(cl5 - slope * glm::radians(5.0f)) < 1.0e-4f);
    QVERIFY(cl5 > 0.3f && cl5 < 0.5f);
    // symmetric, and the stall drops it
    QVERIFY(fabsf(AircraftModel::liftCoefficient(-glm::radians(5.0f), ar) + cl5) < 1.0e-5f);
    float clStall = AircraftModel::liftCoefficient(glm::radians(14.0f), ar);
    float clPast = AircraftModel::liftCoefficient(glm::radians(20.0f), ar);
    QVERIFY(clPast < clStall);
    QVERIFY(clPast > 0.0f);
    // drag: the profile at zero lift, the induced part with the lift, more past the stall
    QVERIFY(fabsf(AircraftModel::dragCoefficient(0.0f, 0.0f, ar) - AircraftModel::PROFILE_DRAG) < 1.0e-6f);
    float cd5 = AircraftModel::dragCoefficient(glm::radians(5.0f), cl5, ar);
    QVERIFY(cd5 > AircraftModel::PROFILE_DRAG && cd5 < 0.1f);
    QVERIFY(AircraftModel::dragCoefficient(glm::radians(20.0f), clPast, ar) > cd5);
}

void AircraftTests::hoverAgainstWeight() {
    AircraftSpec spec = craft();
    AircraftState state;
    state.tilt = 1.0f;
    VehicleInput input;
    input.throttle = spec.mass * G / spec.maxThrust;   // the thrust that just carries the weight
    glm::vec3 position(0.0f, 10.0f, 0.0f);
    AircraftForces f = AircraftModel::forces(spec, state, input, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    glm::vec3 net = netForce(f);
    // straight up, equal to the weight (the body's own gravity is not in the model's forces)
    QVERIFY(fabsf(net.y - spec.mass * G) < 1.0f);
    QVERIFY(fabsf(net.x) < 1.0e-3f && fabsf(net.z) < 1.0e-3f);
    QVERIFY(fabsf(f.tilt - 1.0f) < 1.0e-6f);
    // level and still: no torque from the hover controls
    QVERIFY(glm::length(netTorque(f, position)) < 1.0f);
}

void AircraftTests::levelFlight() {
    // at 35 m/s the wing at the angle of attack whose lift carries the weight: lift = weight, drag far less, and the
    // tail, feeling the same flow, pushes the nose down (stability: the tail is aft and the body is pitched up)
    AircraftSpec spec = craft();
    AircraftState state;   // tilt 0: forward flight
    VehicleInput input;
    float speed = 35.0f;
    float rho = AirshipModel::airDensity(100.0f);
    float q = 0.5f * rho * speed * speed * spec.wingArea;
    float needed = spec.mass * G / q;   // the lift coefficient wanted
    float ar = spec.aspectRatio();
    float slope = 2.0f * glm::pi<float>() * ar / (ar + 2.0f);
    float alpha = needed / slope;
    QVERIFY(alpha < AircraftModel::STALL_ANGLE);
    // the body pitched up by alpha flying level: velocity along the world's forward
    glm::quat rotation = glm::angleAxis(alpha, RIGHT);   // nose up about +x
    glm::vec3 velocity = FORWARD * speed;
    glm::vec3 position(0.0f, 100.0f, 0.0f);
    AircraftForces f = AircraftModel::forces(spec, state, input, position, rotation, velocity, glm::vec3(0.0f), G);
    QVERIFY(fabsf(f.angleOfAttack - alpha) < 0.01f);
    QVERIFY(fabsf(f.lift - spec.mass * G) / (spec.mass * G) < 0.05f);
    QVERIFY(f.drag < 0.15f * f.lift);
    glm::vec3 net = netForce(f);
    // the wing and the tail carry the weight between them (the tail at this incidence lifts a little too)
    QVERIFY(net.y > 0.8f * spec.mass * G);
    // the tail's lift aft of the centre of mass pitches the nose down: a torque about -x
    glm::vec3 torque = netTorque(f, position);
    QVERIFY(torque.x < 0.0f);
}

void AircraftTests::aileronsRoll() {
    AircraftSpec spec = craft();
    AircraftState state;
    VehicleInput input;
    input.steering = 1.0f;   // roll right
    glm::vec3 velocity = FORWARD * 35.0f;
    glm::vec3 position(0.0f, 100.0f, 0.0f);
    AircraftForces f = AircraftModel::forces(spec, state, input, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    glm::vec3 torque = netTorque(f, position);
    // right wing down is a turn about -z, which brings the right wing (+x) toward down (-y) (the right-hand rule
    // about +z turns +x toward +y): the left aileron's extra lift at -x gives (-x) x (+y) = -z, the right's loss at +x
    // gives (+x) x (-y) = -z
    QVERIFY(torque.z < -1000.0f);
    // and the rudder, deflected with the steering, yaws the nose right: the tail pushed left (-x) at +z aft gives a
    // torque (+z) x (-x) = -y
    QVERIFY(torque.y < 0.0f);
    VehicleInput left;
    left.steering = -1.0f;
    AircraftForces g = AircraftModel::forces(spec, state, left, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    QVERIFY(netTorque(g, position).z > 1000.0f);
}

void AircraftTests::elevatorPitches() {
    AircraftSpec spec = craft();
    AircraftState state;
    VehicleInput input;
    input.pitch = 1.0f;   // nose up
    glm::vec3 velocity = FORWARD * 35.0f;
    glm::vec3 position(0.0f, 100.0f, 0.0f);
    AircraftForces f = AircraftModel::forces(spec, state, input, position, glm::quat(), velocity, glm::vec3(0.0f), G);
    // the tail pushed down aft: (+z) x (-y) = +x, the nose up
    QVERIFY(netTorque(f, position).x > 1000.0f);
}

void AircraftTests::tiltTransition() {
    AircraftSpec spec = craft();
    AircraftState state;
    VehicleInput toHover;
    toHover.lift = 1.0f;
    float dt = 1.0f / 90.0f;
    int steps = 0;
    while (state.tilt < 1.0f && steps < 10000) {
        AircraftModel::integrateTilt(spec, state, toHover, dt);
        ++steps;
    }
    float seconds = steps * dt;
    QVERIFY(fabsf(seconds - 1.0f / AircraftModel::TILT_RATE) < 0.05f);
    // let go, it stays; held down, it comes back at the same rate
    VehicleInput hold;
    for (int i = 0; i < 90; ++i) {
        AircraftModel::integrateTilt(spec, state, hold, dt);
    }
    QVERIFY(fabsf(state.tilt - 1.0f) < 1.0e-6f);
    VehicleInput toForward;
    toForward.lift = -1.0f;
    for (int i = 0; i < 90; ++i) {
        AircraftModel::integrateTilt(spec, state, toForward, dt);
    }
    QVERIFY(fabsf(state.tilt - (1.0f - AircraftModel::TILT_RATE)) < 0.01f);
}

void AircraftTests::hoverControls() {
    AircraftSpec spec = craft();
    AircraftState state;
    state.tilt = 1.0f;
    glm::vec3 position(0.0f, 10.0f, 0.0f);
    // pitch up in hover: a torque about +x; yaw right (steering) about -y; a bank is levelled
    VehicleInput input;
    input.throttle = 0.6f;
    input.pitch = 1.0f;
    AircraftForces f = AircraftModel::forces(spec, state, input, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    QVERIFY(netTorque(f, position).x > 1000.0f);
    VehicleInput yaw;
    yaw.throttle = 0.6f;
    yaw.steering = 1.0f;
    AircraftForces g = AircraftModel::forces(spec, state, yaw, position, glm::quat(), glm::vec3(0.0f), glm::vec3(0.0f), G);
    QVERIFY(netTorque(g, position).y < -1000.0f);
    VehicleInput still;
    still.throttle = 0.6f;
    glm::quat banked = glm::angleAxis(glm::radians(20.0f), -glm::vec3(0.0f, 0.0f, 1.0f));   // right wing down
    AircraftForces h = AircraftModel::forces(spec, state, still, position, banked, glm::vec3(0.0f), glm::vec3(0.0f), G);
    glm::vec3 torque = netTorque(h, position);
    // the levelling torque turns the right wing back up: about +z
    QVERIFY(torque.z > 1000.0f);
    // and the thrust still points along the tilted body's up
    glm::vec3 net = netForce(h);
    QVERIFY(glm::dot(glm::normalize(net), banked * UP) > 0.99f);
}
