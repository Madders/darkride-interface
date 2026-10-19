//
//  AirshipModel.h
//  libraries/vehicles/src
//
//  The lighter-than-air vehicle (PHYSICS.md 4.8 and round 7): an airship as its entity describes it, and the forces on
//  it this substep. Plain numbers, no Jolt and no entities, the same code on the physics server and on every client:
//  the physics (VehicleSimulation) fills the spec from the vehicle property group, asks for the forces each substep and
//  adds them to the hull's body.
//
//  The model, in the ship's own axes (forward -z, up +y, right +x, as every entity):
//  - buoyancy: the envelope's volume times its fill, times the difference between the air's density at this altitude
//    (the standard atmosphere) and the lifting gas's, times g, straight up at the centre of buoyancy; the gas's own
//    weight is in that difference, the hull's weight is the body's mass under the entity's gravity, the ballast's a
//    force at the centre of mass. The centre of buoyancy above the centre of mass is what keeps the ship upright: the
//    two forces at two points are a pendulum;
//  - the propellers: throttle times maxThrust along forward, at the thrust point (low on the gondola it pitches the
//    nose a little, as it does); and a vertical thruster, lift times maxVerticalThrust along up, at the same point;
//  - the fins: a vertical fin (the rudder) and a horizontal one (the elevator) at the fin point aft, each a flat plate
//    at an angle to the relative wind: lift = half rho v^2 x area x slope x sin(angle), capped at the stall, along the
//    plate's normal and signed by which face the wind strikes. The angle is the flow's own (sideslip or incidence)
//    plus the pilot's deflection, so the same formula gives the weathercock stability (the nose follows the velocity)
//    and the control;
//  - drag: half rho v|v| times a drag area per axis, at the centre of buoyancy (it is the envelope's), and the same
//    per axis for the spin (an angular drag area, m^5);
//  - ballast and gas: the trim input drops ballast (lighter) or vents gas (heavier) at the spec's rates, both one way,
//    as on a real ship. The vertical thruster is the reversible way up and down.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AirshipModel_h
#define hifi_AirshipModel_h

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "VehicleInput.h"

struct AirshipSpec {
    float mass { 0.0f };                        // kg, the hull without ballast (the body's mass)
    glm::vec3 centerOfMass { 0.0f };            // m, local
    float envelopeVolume { 0.0f };              // m^3
    glm::vec3 centerOfBuoyancy { 0.0f };        // m, local: the envelope's centre
    float gasDensity { 0.1786f };               // kg/m^3 (helium 0.1786, hydrogen 0.0899)
    float maxThrust { 0.0f };                   // N, the propellers at full throttle
    float maxVerticalThrust { 0.0f };           // N, the vertical thruster at full lift
    glm::vec3 thrustPoint { 0.0f };             // m, local
    glm::vec3 dragArea { 0.0f };                // m^2 per local axis (drag coefficient times area)
    glm::vec3 angularDragArea { 0.0f };         // m^5 per local axis
    float rudderArea { 0.0f };                  // m^2
    float elevatorArea { 0.0f };                // m^2
    glm::vec3 finPoint { 0.0f };                // m, local: where the fins act (aft of the centre of buoyancy)
    float maxFinAngle { 25.0f };                // degrees of deflection at full input
    float ballastRate { 0.0f };                 // kg/s dropped at full trim
    float ventRate { 0.0f };                    // fraction of the envelope per second vented at full negative trim

    bool isValid() const { return envelopeVolume > 0.0f; }
};

// what changes in flight, carried in the vehicle's state and corrected from the authority's
struct AirshipState {
    float ballast { 0.0f };     // kg carried now
    float fill { 1.0f };        // the fraction of the envelope that is gas
};

// one force at a point, both in world coordinates
struct AirshipForce {
    glm::vec3 force { 0.0f };
    glm::vec3 point { 0.0f };
};

struct AirshipForces {
    std::vector<AirshipForce> forces;
    glm::vec3 torque { 0.0f };          // world
    // for telemetry
    float airDensity { 0.0f };
    float buoyancy { 0.0f };            // N up (net of the gas's weight)
    float ballastWeight { 0.0f };       // N down
    float thrust { 0.0f };              // N along forward
    float verticalThrust { 0.0f };      // N along up
    float airspeed { 0.0f };            // m/s of the relative wind at the centre of buoyancy
    float sideslip { 0.0f };            // rad, positive when the wind comes from the right
    float incidence { 0.0f };           // rad, positive when the wind comes from below
};

namespace AirshipModel {
    const float SEA_LEVEL_AIR_DENSITY = 1.225f;     // kg/m^3
    const float FIN_LIFT_SLOPE = 3.0f;              // per radian, a low aspect ratio fin
    const float FIN_STALL = 1.2f;                   // the lift coefficient's cap

    // the standard atmosphere's density at this altitude above sea level (the domain's y = 0), m
    float airDensity(float altitude);

    // the forces on the hull this substep, in world coordinates: the hull's rotation, the velocity of its centre of
    // mass and its angular velocity in the world, the centre of mass's world position (altitude is its y), gravity
    // (a positive magnitude) and the wind's world velocity
    AirshipForces forces(const AirshipSpec& spec, const AirshipState& state, const VehicleInput& input,
                         const glm::vec3& position, const glm::quat& rotation, const glm::vec3& velocity,
                         const glm::vec3& angularVelocity, float gravity, const glm::vec3& wind = glm::vec3(0.0f));

    // the ballast dropped and the gas vented by this substep's trim input
    void integrateTrim(const AirshipSpec& spec, AirshipState& state, const VehicleInput& input, float dt);

    // the force of one flat fin: its normal (world, undeflected), the hinge axis (world) it deflects about, the
    // deflection (rad), the relative wind at its point (world), the area and the density
    glm::vec3 finForce(const glm::vec3& normal, const glm::vec3& hinge, float deflection, const glm::vec3& relativeWind,
                       float area, float airDensity);

    // a reasonable drag area and angular drag area for a hull of these dimensions (x, y, z extents, the long axis z)
    glm::vec3 defaultDragArea(const glm::vec3& dimensions);
    glm::vec3 defaultAngularDragArea(const glm::vec3& dimensions);
}

#endif // hifi_AirshipModel_h
