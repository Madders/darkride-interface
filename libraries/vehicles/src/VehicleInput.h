//
//  VehicleInput.h
//  libraries/vehicles/src
//
//  A driver's input to a vehicle, one per physics substep (PHYSICS.md 4.6, round 6): what the driver's client applies at
//  once (client-side prediction) and sends to the physics server, which applies the same in order and reports the last
//  sequence number it reached in the vehicle's state, so the driver's client can reconcile from there. The packet
//  (PacketType::VehicleInput, version 2 since the airship's three controls) carries the vehicle's id and the last few
//  inputs, so one lost datagram loses nothing.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_VehicleInput_h
#define hifi_VehicleInput_h

#include <cstdint>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QUuid>

struct VehicleInput {
    uint32_t sequence { 0 };   // counts up by one per substep the driver's client applied it in
    float throttle { 0.0f };   // -1 reverse .. 1 forward
    float steering { 0.0f };   // -1 left .. 1 right
    float brake { 0.0f };      // 0 .. 1
    float handBrake { 0.0f };  // 0 .. 1
    // an airship's (PHYSICS.md round 7): the elevator, the vertical thruster, and ballast dropped (+) or gas vented (-)
    float pitch { 0.0f };      // -1 nose down .. 1 nose up
    float lift { 0.0f };       // -1 down .. 1 up
    float trim { 0.0f };       // -1 vent .. 1 drop ballast

    bool isIdle() const {
        return throttle == 0.0f && steering == 0.0f && brake == 0.0f && handBrake == 0.0f && pitch == 0.0f && lift == 0.0f && trim == 0.0f;
    }
    bool sameControls(const VehicleInput& other) const {
        return throttle == other.throttle && steering == other.steering && brake == other.brake && handBrake == other.handBrake
            && pitch == other.pitch && lift == other.lift && trim == other.trim;
    }
};

namespace VehicleInputPacket {
    const int MAX_INPUTS = 8;   // the last few go in every packet: the redundancy that makes loss harmless
    const int INPUT_BYTES = 4 + 7 * 4;

    // the payload: the vehicle's id, a count, then the inputs oldest first
    QByteArray pack(const QUuid& vehicleID, const std::vector<VehicleInput>& inputs);
    // false for a payload that is not one of ours
    bool unpack(const QByteArray& payload, QUuid& vehicleID, std::vector<VehicleInput>& inputs);
}

#endif // hifi_VehicleInput_h
