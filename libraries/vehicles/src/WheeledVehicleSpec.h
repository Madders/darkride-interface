//
//  WheeledVehicleSpec.h
//  libraries/vehicles/src
//
//  A wheeled vehicle as its entity describes it (PHYSICS.md 4.8, round 6): the numbers of the vehicle property group
//  parsed into one plain structure the physics builds its Jolt vehicle from, the same on the physics server and on
//  every client. The wheel and seat lists are JSON with Unity's WheelCollider names, so a Unity vehicle carries across
//  (TERRAIN.md 9 is the terrain's side of that). Plain data and parsing: no Jolt, no entities.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_WheeledVehicleSpec_h
#define hifi_WheeledVehicleSpec_h

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <QtCore/QString>

struct WheelSpec {
    QString name;
    glm::vec3 center { 0.0f };          // m, local: the suspension's top attachment point (fully compressed)
    float radius { 0.35f };             // m
    float width { 0.2f };               // m
    float mass { 20.0f };               // kg, for the wheel's inertia (a solid cylinder's)
    float suspensionDistance { 0.2f };  // m of travel below center
    float targetPosition { 0.5f };      // the rest point as a fraction of the travel (Unity's; 0 fully extended)
    float spring { 0.0f };              // N/m; 0 means the vehicle's frequency and damping ratio set the spring
    float damper { 0.0f };              // N s/m
    float steer { 0.0f };               // fraction of maxSteerAngle, negative to counter-steer
    int drive { -1 };                   // 1 driven, 0 not, -1 as the vehicle's driveType says
    float brake { 1.0f };               // fraction of maxBrakeTorque
    float handBrake { 0.0f };           // fraction of maxHandBrakeTorque
    bool enableSuspensionForcePoint { true };   // forces act at the wheel's neutral centre (steadier than the contact)
};

struct SeatSpec {
    QString name;
    glm::vec3 position { 0.0f };        // m, local: where the avatar's hips go
    float yaw { 0.0f };                 // degrees about the entity's up; 0 faces the vehicle's forward (-z)
    bool driver { false };
};

class WheeledVehicleSpec {
public:
    enum class DriveType { REAR, FRONT, ALL };

    float mass { 0.0f };                    // kg; 0 means the entity's density and volume
    glm::vec3 centerOfMass { 0.0f };        // m, local
    std::vector<WheelSpec> wheels;
    std::vector<SeatSpec> seats;
    float maxSteerAngle { 30.0f };          // degrees
    float maxTorque { 500.0f };             // N m
    float minRPM { 1000.0f };
    float maxRPM { 6000.0f };
    std::vector<float> gearRatios;          // empty for the engine's default
    float reverseGearRatio { 2.9f };
    float finalDriveRatio { 3.42f };
    float maxBrakeTorque { 1500.0f };       // N m per wheel
    float maxHandBrakeTorque { 4000.0f };   // N m per wheel with handBrake 1
    DriveType driveType { DriveType::REAR };
    float suspensionFrequency { 1.5f };     // Hz
    float suspensionDamping { 0.5f };       // ratio

    // The JSON lists. Each returns false, leaving the list empty, for JSON that is not a list of objects; a wheel without a
    // center or a seat without a position is skipped. The error, if asked for, says what was wrong.
    static bool parseWheels(const QString& json, std::vector<WheelSpec>& wheels, QString* error = nullptr);
    static bool parseSeats(const QString& json, std::vector<SeatSpec>& seats, QString* error = nullptr);
    static bool parseGearRatios(const QString& json, std::vector<float>& ratios, QString* error = nullptr);
    static DriveType parseDriveType(const QString& name);

    // Wheels paired into axles by their z (within a tolerance): each pair is (left, right) by x, a lone wheel is (i, i).
    // The order is front (the smallest z, the vehicle's forward being -z) to back.
    std::vector<std::pair<int, int>> axles() const;
    // Whether the engine turns a wheel: its own say, else the driveType by axle (the frontmost axle is "front", the
    // rearmost "rear"; a vehicle with one axle drives it).
    bool isDriven(int wheelIndex) const;
    // The first seat marked driver, else -1.
    int driverSeat() const;
    // m: from the frontmost wheel to the rearmost along z; 0 with one axle.
    float wheelbase() const;
    // The steering the wheels take now, from what the driver asks (wanted, -1 to 1), what they took dt seconds ago
    // (current) and the vehicle's speed over the ground (m/s). The wheels turn toward the ask at a rate, not at once
    // (keyboard steering is all or nothing): full lock in STEER_IN_TIME, back toward the centre in STEER_OUT_TIME. And
    // the lock is held under the angle whose turn would need STEER_GRIP g of sideways grip at this speed, atan(grip g
    // L / v^2), the standard speed-sensitive steering: full lock at walking pace, a gentle arc at top speed, never more
    // turn than the tyres hold (kart 2 at full lock spun from 7 m/s without it; physics, 2026-10-26).
    float steeringFor(float current, float wanted, float speed, float dt) const;

    bool isValid() const { return !wheels.empty(); }
};

#endif // hifi_WheeledVehicleSpec_h
