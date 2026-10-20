//
//  AirshipModel.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AirshipModel.h"

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>

namespace {
    const glm::vec3 UP(0.0f, 1.0f, 0.0f);
    const glm::vec3 RIGHT(1.0f, 0.0f, 0.0f);
    const glm::vec3 FORWARD(0.0f, 0.0f, -1.0f);
    const float MAX_ALTITUDE = 11000.0f;   // the troposphere

    float clamp1(float value) {
        return glm::clamp(value, -1.0f, 1.0f);
    }
}

float AirshipModel::airDensity(float altitude) {
    // the international standard atmosphere's troposphere: rho = rho0 (1 - L h / T0)^(g / (R L) - 1)
    float h = glm::clamp(altitude, 0.0f, MAX_ALTITUDE);
    return SEA_LEVEL_AIR_DENSITY * powf(1.0f - 2.25577e-5f * h, 4.25588f);
}

glm::vec3 AirshipModel::finForce(const glm::vec3& normal, const glm::vec3& hinge, float deflection, const glm::vec3& relativeWind,
                                 float area, float airDensity) {
    float speed2 = glm::length2(relativeWind);
    if (area <= 0.0f || speed2 < 1.0e-6f) {
        return glm::vec3(0.0f);
    }
    // the plate's normal, turned about its hinge by the deflection
    glm::vec3 n = glm::normalize(glm::angleAxis(deflection, glm::normalize(hinge)) * normal);
    float speed = sqrtf(speed2);
    // the sine of the angle of attack: the wind's component along the normal; it pushes the plate the way it blows
    float sinAlpha = glm::dot(relativeWind, n) / speed;
    float coefficient = glm::clamp(FIN_LIFT_SLOPE * sinAlpha, -FIN_STALL, FIN_STALL);
    return (0.5f * airDensity * speed2 * area * coefficient) * n;
}

glm::vec3 AirshipModel::defaultDragArea(const glm::vec3& d) {
    // a streamlined body along z: a tenth of the frontal area ahead, half the side areas across
    return glm::vec3(0.5f * d.y * d.z, 0.5f * d.x * d.z, 0.1f * d.x * d.y);
}

glm::vec3 AirshipModel::defaultAngularDragArea(const glm::vec3& d) {
    // a slab of width w and length L turning about its centre: torque = rho/2 w L^4 / 32 w^2 per unit Cd
    float length4 = d.z * d.z * d.z * d.z;
    return glm::vec3(d.x * length4 / 32.0f, d.y * length4 / 32.0f, d.x * d.y * d.y * d.y * d.y / 32.0f);
}

void AirshipModel::integrateTrim(const AirshipSpec& spec, AirshipState& state, const VehicleInput& input, float dt) {
    float trim = clamp1(input.trim);
    if (trim > 0.0f) {
        state.ballast = glm::max(0.0f, state.ballast - trim * spec.ballastRate * dt);
    } else if (trim < 0.0f) {
        state.fill = glm::clamp(state.fill + trim * spec.ventRate * dt, 0.0f, 1.0f);
    }
}

AirshipForces AirshipModel::forces(const AirshipSpec& spec, const AirshipState& state, const VehicleInput& input,
                                   const glm::vec3& position, const glm::quat& rotation, const glm::vec3& velocity,
                                   const glm::vec3& angularVelocity, float gravity, const glm::vec3& wind) {
    AirshipForces out;
    if (!spec.isValid()) {
        return out;
    }
    glm::vec3 up = rotation * UP;
    glm::vec3 forward = rotation * FORWARD;
    glm::vec3 right = rotation * RIGHT;
    glm::quat toLocal = glm::inverse(rotation);

    // the points, in the world; `position` is the centre of mass, the local points are measured from the entity's centre
    auto worldPoint = [&](const glm::vec3& local) { return position + rotation * (local - spec.centerOfMass); };
    glm::vec3 buoyancyPoint = worldPoint(spec.centerOfBuoyancy);
    glm::vec3 thrustPoint = worldPoint(spec.thrustPoint);
    glm::vec3 finPoint = worldPoint(spec.finPoint);

    // buoyancy, at the envelope's centre, from the air at its altitude
    float rho = airDensity(buoyancyPoint.y);
    out.airDensity = rho;
    out.buoyancy = spec.envelopeVolume * glm::clamp(state.fill, 0.0f, 1.0f) * (rho - spec.gasDensity) * gravity;
    out.forces.push_back({ glm::vec3(0.0f, out.buoyancy, 0.0f), buoyancyPoint });

    // the ballast's weight, at the centre of mass
    out.ballastWeight = glm::max(0.0f, state.ballast) * gravity;
    if (out.ballastWeight > 0.0f) {
        out.forces.push_back({ glm::vec3(0.0f, -out.ballastWeight, 0.0f), position });
    }

    // the propellers and the vertical thruster
    out.thrust = clamp1(input.throttle) * spec.maxThrust;
    out.verticalThrust = clamp1(input.lift) * spec.maxVerticalThrust;
    if (out.thrust != 0.0f || out.verticalThrust != 0.0f) {
        out.forces.push_back({ forward * out.thrust + up * out.verticalThrust, thrustPoint });
    }

    // the relative wind at the envelope's centre and at the fins (the spin adds to it there)
    glm::vec3 windAtBuoyancy = wind - (velocity + glm::cross(angularVelocity, buoyancyPoint - position));
    glm::vec3 windAtFins = wind - (velocity + glm::cross(angularVelocity, finPoint - position));
    glm::vec3 localWind = toLocal * windAtBuoyancy;
    out.airspeed = glm::length(windAtBuoyancy);
    if (out.airspeed > 1.0e-3f) {
        // the wind blows toward +z along the hull when the ship moves forward; from the right when localWind.x < 0
        out.sideslip = atan2f(-localWind.x, localWind.z);
        out.incidence = atan2f(localWind.y, localWind.z);
    }

    // drag at the envelope's centre: per local axis, against the motion through the air
    glm::vec3 drag(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        drag[axis] = 0.5f * rho * spec.dragArea[axis] * localWind[axis] * fabsf(localWind[axis]);
    }
    if (glm::length2(drag) > 0.0f) {
        out.forces.push_back({ rotation * drag, buoyancyPoint });
    }

    // the fins: the rudder stands up (normal right, hinge up), the elevator lies flat (normal up, hinge right);
    // steering right turns the rudder so the tail is pushed left; pitch up turns the elevator so the tail is pushed down
    float maxAngle = glm::radians(spec.maxFinAngle);
    glm::vec3 rudder = finForce(right, up, clamp1(input.steering) * maxAngle, windAtFins, spec.rudderArea, rho);
    glm::vec3 elevator = finForce(up, right, -clamp1(input.pitch) * maxAngle, windAtFins, spec.elevatorArea, rho);
    glm::vec3 finForces = rudder + elevator;
    if (glm::length2(finForces) > 0.0f) {
        out.forces.push_back({ finForces, finPoint });
    }

    // angular drag, per local axis
    glm::vec3 localSpin = toLocal * angularVelocity;
    glm::vec3 torque(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        torque[axis] = -0.5f * rho * spec.angularDragArea[axis] * localSpin[axis] * fabsf(localSpin[axis]);
    }
    out.torque = rotation * torque;
    return out;
}
