//
//  AircraftModel.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AircraftModel.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

namespace {
    const glm::vec3 UP(0.0f, 1.0f, 0.0f);
    const glm::vec3 RIGHT(1.0f, 0.0f, 0.0f);
    const glm::vec3 FORWARD(0.0f, 0.0f, -1.0f);

    float clamp1(float value) {
        return glm::clamp(value, -1.0f, 1.0f);
    }
}

float AircraftModel::liftCoefficient(float alpha, float aspectRatio) {
    // thin aerofoil theory with the finite span's correction: 2 pi AR / (AR + 2) per radian, to the stall; past it a
    // flat plate, sin(2 alpha), which is what a stalled wing is
    float slope = 2.0f * glm::pi<float>() * aspectRatio / (aspectRatio + 2.0f);
    if (fabsf(alpha) < STALL_ANGLE) {
        return slope * alpha;
    }
    return sinf(2.0f * alpha);
}

float AircraftModel::dragCoefficient(float alpha, float liftCoefficient, float aspectRatio) {
    float induced = aspectRatio > 0.0f ? liftCoefficient * liftCoefficient / (glm::pi<float>() * aspectRatio * OSWALD) : 0.0f;
    float stalled = fabsf(alpha) < STALL_ANGLE ? 0.0f : 1.2f * sinf(alpha) * sinf(alpha);
    return PROFILE_DRAG + induced + stalled;
}

void AircraftModel::integrateTilt(const AircraftSpec& spec, AircraftState& state, const VehicleInput& input, float dt) {
    state.tilt = glm::clamp(state.tilt + clamp1(input.lift) * TILT_RATE * dt, 0.0f, 1.0f);
}

AircraftForces AircraftModel::forces(const AircraftSpec& spec, const AircraftState& state, const VehicleInput& input,
                                     const glm::vec3& position, const glm::quat& rotation, const glm::vec3& velocity,
                                     const glm::vec3& angularVelocity, float gravity, const glm::vec3& wind) {
    AircraftForces out;
    if (!spec.isValid()) {
        return out;
    }
    glm::vec3 up = rotation * UP;
    glm::vec3 forward = rotation * FORWARD;
    glm::vec3 right = rotation * RIGHT;
    glm::quat toLocal = glm::inverse(rotation);
    float tilt = glm::clamp(state.tilt, 0.0f, 1.0f);
    out.tilt = tilt;

    // the points, in the world; `position` is the centre of mass, the local points are measured from the entity's centre
    auto worldPoint = [&](const glm::vec3& local) { return position + rotation * (local - spec.centerOfMass); };
    glm::vec3 thrustPoint = worldPoint(spec.thrustPoint);
    glm::vec3 finPoint = worldPoint(spec.finPoint);
    float rho = AirshipModel::airDensity(position.y);
    out.airDensity = rho;

    // the thrust, along the nacelles: level at tilt 0, vertical at tilt 1
    float angle = tilt * glm::half_pi<float>();
    glm::vec3 thrustDirection = glm::normalize(forward * cosf(angle) + up * sinf(angle));
    out.thrust = glm::clamp(input.throttle, 0.0f, 1.0f) * spec.maxThrust;
    if (out.thrust > 0.0f) {
        out.forces.push_back({ thrustDirection * out.thrust, thrustPoint });
    }

    // the wing in two halves, each at its own point with its own relative wind (the spin's part damps the roll) and
    // its own aileron deflection
    float aspectRatio = spec.aspectRatio();
    float aileron = clamp1(input.steering) * glm::radians(spec.maxFinAngle) * AILERON_FRACTION;
    float liftSum = 0.0f, dragSum = 0.0f, coefficientSum = 0.0f;
    glm::vec3 wingWind(0.0f);
    for (int side = 0; side < 2; ++side) {
        float sign = side == 0 ? 1.0f : -1.0f;   // right, then left
        glm::vec3 localPoint = spec.wingPoint + RIGHT * (sign * 0.25f * spec.wingSpan);
        glm::vec3 point = worldPoint(localPoint);
        glm::vec3 relativeWind = wind - (velocity + glm::cross(angularVelocity, point - position));
        glm::vec3 localWind = toLocal * relativeWind;
        if (side == 0) {
            wingWind = relativeWind;
        }
        // the flow in the plane of symmetry: along +z when flying forward, from below when localWind.y > 0
        glm::vec3 flow(0.0f, localWind.y, localWind.z);
        float speed2 = glm::length2(flow);
        if (speed2 < 1.0e-4f) {
            continue;
        }
        float speed = sqrtf(speed2);
        // steering right lowers the right aileron's lift and raises the left's: the aircraft rolls right
        float alpha = atan2f(flow.y, flow.z) - sign * aileron;
        float cl = liftCoefficient(alpha, aspectRatio);
        float cd = dragCoefficient(alpha, cl, aspectRatio);
        float q = 0.5f * rho * speed2 * 0.5f * spec.wingArea;
        glm::vec3 flowDirection = flow / speed;
        glm::vec3 liftDirection(0.0f, flowDirection.z, -flowDirection.y);   // perpendicular to the flow, toward +y in level flight
        glm::vec3 force = rotation * (liftDirection * (q * cl) + flowDirection * (q * cd));
        out.forces.push_back({ force, point });
        liftSum += q * cl;
        dragSum += q * cd;
        coefficientSum += 0.5f * cl;
    }
    out.lift = liftSum;
    out.drag = dragSum;
    out.liftCoefficient = coefficientSum;
    glm::vec3 localWingWind = toLocal * wingWind;
    out.airspeed = glm::length(wingWind);
    if (out.airspeed > 1.0e-3f) {
        out.angleOfAttack = atan2f(localWingWind.y, localWingWind.z);
        out.sideslip = atan2f(-localWingWind.x, localWingWind.z);
    }

    // the tail: the tailplane with the elevator (pitch up turns it so the tail is pushed down), the fin with the rudder
    // (steering right pushes the tail left), both feeling the flow's own angle as well
    glm::vec3 windAtFins = wind - (velocity + glm::cross(angularVelocity, finPoint - position));
    float maxAngle = glm::radians(spec.maxFinAngle);
    glm::vec3 elevator = AirshipModel::finForce(up, right, -clamp1(input.pitch) * maxAngle, windAtFins, spec.elevatorArea, rho);
    glm::vec3 rudder = AirshipModel::finForce(right, up, clamp1(input.steering) * maxAngle * RUDDER_FRACTION, windAtFins, spec.rudderArea, rho);
    glm::vec3 tail = elevator + rudder;
    if (glm::length2(tail) > 0.0f) {
        out.forces.push_back({ tail, finPoint });
    }

    // the body's drag at the centre of mass, per local axis, and the angular drag
    glm::vec3 localWind = toLocal * (wind - velocity);
    glm::vec3 drag(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        drag[axis] = 0.5f * rho * spec.dragArea[axis] * localWind[axis] * fabsf(localWind[axis]);
    }
    if (glm::length2(drag) > 0.0f) {
        out.forces.push_back({ rotation * drag, position });
    }
    glm::vec3 localSpin = toLocal * angularVelocity;
    glm::vec3 torque(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        torque[axis] = -0.5f * rho * spec.angularDragArea[axis] * localSpin[axis] * fabsf(localSpin[axis]);
    }

    // hover: the rotors' differential thrust stands in for the surfaces, in proportion to the tilt and the thrust in use
    if (tilt > 0.0f && out.thrust > 0.0f) {
        float authority = tilt * out.thrust;
        float tailArm = fabsf(spec.finPoint.z - spec.centerOfMass.z);
        torque.x += clamp1(input.pitch) * authority * HOVER_PITCH_LEVER * tailArm;
        torque.y += -clamp1(input.steering) * authority * HOVER_YAW_LEVER * spec.wingSpan;
        // toward upright: the torque that turns the body's up onto the world's
        glm::vec3 worldUpLocal = toLocal * UP;
        glm::vec3 levelling = glm::cross(UP, worldUpLocal);
        torque += levelling * (authority * HOVER_LEVEL_LEVER * spec.wingSpan);
        torque -= localSpin * (authority * HOVER_DAMPING_LEVER * spec.wingSpan);
    }
    out.torque = rotation * torque;
    return out;
}
