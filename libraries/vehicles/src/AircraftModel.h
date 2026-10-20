//
//  AircraftModel.h
//  libraries/vehicles/src
//
//  Powered flight (PHYSICS.md 4.8 and round 8): a tilt-thrust aircraft as its entity describes it, and the forces on it
//  this substep. Plain numbers, no Jolt and no entities, the same code on the physics server and on every client, as
//  the airship's model (AirshipModel) is; the physics (VehicleSimulation) fills the spec from the vehicle property
//  group, asks for the forces each substep and adds them to the body.
//
//  The model, in the aircraft's own axes (forward -z, up +y, right +x, as every entity):
//  - thrust: throttle times maxThrust at the thrust point, along forward when the nacelles are level (tilt 0) and
//    along up when they stand vertical (tilt 1), through the angles between. The tilt is state: the pilot's lift
//    input moves it, up toward vertical while positive and down toward level while negative, at a fixed rate, and
//    it stays where it is let go, so a transition takes seconds, as it does, and a key can be held for it;
//  - the wing: two half-panels at the wing point, each lifting with its angle of attack (the relative wind's angle
//    to the chord in the plane of symmetry), a thin-aerofoil slope for the wing's aspect ratio up to the stall and
//    a flat plate beyond it; induced drag from the lift and a profile drag besides. The ailerons deflect the
//    half-panels against each other by the steering input, so steering rolls the aircraft; the half-panels at their
//    spanwise points also give the roll its damping;
//  - the tail: the airship's fins (AirshipModel::finForce): a horizontal tailplane with the elevator (pitch input)
//    and a vertical fin with the rudder (part of the steering input), both at the fin point aft; the flow's own
//    angle at them is what makes the nose follow the velocity;
//  - drag: half rho v|v| times a drag area per axis at the centre of mass, and an angular drag area for the spin;
//  - hover: with the nacelles tilted up there is no airflow over the surfaces, so the controls act as the rotors'
//    differential thrust would: a pitch torque from the pitch input, a yaw torque from the steering input, and a
//    levelling torque toward upright with damping of the spin, all in proportion to the tilt and to the thrust in
//    use (fly-by-wire, as on every real tilt-rotor).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AircraftModel_h
#define hifi_AircraftModel_h

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AirshipModel.h"
#include "VehicleInput.h"

struct AircraftSpec {
    float mass { 0.0f };                        // kg (the body's mass)
    glm::vec3 centerOfMass { 0.0f };            // m, local
    float maxThrust { 0.0f };                   // N at full throttle
    glm::vec3 thrustPoint { 0.0f };             // m, local
    float wingArea { 0.0f };                    // m^2, both halves together
    float wingSpan { 0.0f };                    // m, tip to tip
    glm::vec3 wingPoint { 0.0f };               // m, local: the wing's aerodynamic centre (a quarter chord back)
    float elevatorArea { 0.0f };                // m^2, the horizontal tail
    float rudderArea { 0.0f };                  // m^2, the vertical fin
    glm::vec3 finPoint { 0.0f };                // m, local: where the tail acts (aft, +z)
    float maxFinAngle { 25.0f };                // degrees of control deflection at full input
    glm::vec3 dragArea { 0.0f };                // m^2 per local axis, the body's
    glm::vec3 angularDragArea { 0.0f };         // m^5 per local axis

    bool isValid() const { return wingArea > 0.0f && wingSpan > 0.0f && maxThrust > 0.0f; }
    float aspectRatio() const { return wingArea > 0.0f ? wingSpan * wingSpan / wingArea : 0.0f; }
};

// what changes in flight, carried in the vehicle's state and corrected from the authority's
struct AircraftState {
    float tilt { 0.0f };        // 0 the nacelles level (forward flight), 1 vertical (hover)
};

struct AircraftForces {
    std::vector<AirshipForce> forces;   // each at a point, both in world coordinates
    glm::vec3 torque { 0.0f };          // world
    // for telemetry
    float airDensity { 0.0f };
    float thrust { 0.0f };              // N
    float tilt { 0.0f };                // as applied this substep
    float airspeed { 0.0f };            // m/s of the relative wind at the wing
    float angleOfAttack { 0.0f };       // rad, positive when the wind comes from below
    float sideslip { 0.0f };            // rad, positive when the wind comes from the right
    float lift { 0.0f };                // N, the wing's, both halves
    float drag { 0.0f };                // N, the wing's
    float liftCoefficient { 0.0f };     // the wing's, averaged over the halves
};

namespace AircraftModel {
    const float TILT_RATE = 0.35f;                  // of the full tilt per second: hover to forward flight in 3 s
    const float STALL_ANGLE = 0.26f;                // rad (15 degrees): the aerofoil's slope holds to here
    const float PROFILE_DRAG = 0.03f;               // the wing's drag coefficient at zero lift
    const float OSWALD = 0.8f;                      // the span efficiency in the induced drag
    const float AILERON_FRACTION = 0.4f;            // of maxFinAngle: the half-panels' deflection at full steering
    const float RUDDER_FRACTION = 0.5f;             // of maxFinAngle: the rudder's deflection at full steering
    const float HOVER_PITCH_LEVER = 0.5f;           // of the tail's distance: the pitch torque per unit thrust in hover
    const float HOVER_YAW_LEVER = 0.25f;            // of the span: the yaw torque per unit thrust in hover
    const float HOVER_LEVEL_LEVER = 0.5f;           // of the span: the levelling torque per unit thrust in hover
    const float HOVER_DAMPING_LEVER = 0.3f;         // of the span, per rad/s of spin

    // the lift coefficient of the wing at this angle of attack (rad), for this aspect ratio
    float liftCoefficient(float angleOfAttack, float aspectRatio);
    // the drag coefficient that goes with it
    float dragCoefficient(float angleOfAttack, float liftCoefficient, float aspectRatio);

    // the tilt moves by the lift input this substep: positive toward vertical, negative toward level, 0 holds
    void integrateTilt(const AircraftSpec& spec, AircraftState& state, const VehicleInput& input, float dt);

    // the forces on the aircraft this substep, in world coordinates: the body's rotation, the velocity of its centre
    // of mass and its angular velocity in the world, the centre of mass's world position (altitude is its y),
    // gravity (a positive magnitude) and the wind's world velocity
    AircraftForces forces(const AircraftSpec& spec, const AircraftState& state, const VehicleInput& input,
                          const glm::vec3& position, const glm::quat& rotation, const glm::vec3& velocity,
                          const glm::vec3& angularVelocity, float gravity, const glm::vec3& wind = glm::vec3(0.0f));
}

#endif // hifi_AircraftModel_h
