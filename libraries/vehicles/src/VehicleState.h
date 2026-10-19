//
//  VehicleState.h
//  libraries/vehicles/src
//
//  The physics server's state stream (PHYSICS.md 4.5's second step, built in round 9): one vehicle's state as the server
//  simulated it at a substep, sent straight to each client that asked for that vehicle, unreliably, at a fixed rate,
//  beside the entity server's relay (kept at a low rate for persistence, late joiners and scripts). The driver's client
//  reconciles its prediction against it; everyone else interpolates between two of them (SnapshotBuffer).
//
//  Two packets: VehicleState, from the server to a client, a count and the states; and VehicleStateRequest, from a
//  client to the server, the ids of the vehicles the client simulates (the subscription: sent every second and when the
//  set changes; the server streams to a client for a few seconds after its last request). Both carry plain
//  little-endian fields, as VehicleInput does.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_VehicleState_h
#define hifi_VehicleState_h

#include <cstdint>
#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QUuid>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "VehicleInput.h"

struct VehicleState {
    QUuid id;
    uint64_t time { 0 };        // usec: the substep the state is for, on the sender's clock until the receiver converts it
    uint32_t step { 0 };        // the sender's substep number
    // the body's pose and velocities in the vehicle's frame's coordinates (the domain's for a vehicle in no frame)
    glm::vec3 position { 0.0f };
    glm::quat rotation;
    glm::vec3 velocity { 0.0f };
    glm::vec3 angularVelocity { 0.0f };
    VehicleInput input;         // the input applied at that substep, with its sequence (the driver's tick it was for)
    // the input buffer's view at that substep (PHYSICS.md round 10): the driver's tick the substep applied (0 when no
    // driver is aligned; the driver's record of that tick is what the state is reconciled against) and the margin,
    // how many ticks of input past it had arrived (negative: none for it; the driver's lead controller reads it)
    uint32_t tick { 0 };
    int8_t margin { 0 };
    // what the model carries from one substep to the next
    float engineRPM { 0.0f };
    int8_t gear { 0 };
    float ballast { 0.0f };     // kg aboard an airship
    float fill { 1.0f };        // of its envelope
    float tilt { 0.0f };        // an aircraft's nacelles, 0 level to 1 vertical

    // the state between two, alpha of the way from a to b: the continuous parts blended (the rotation slerped), the
    // discrete parts (the id, the gear, the input) from a
    static VehicleState interpolate(const VehicleState& a, const VehicleState& b, float alpha);
};

namespace VehicleStatePacket {
    const int MAX_STATES = 8;   // per packet; a thousand bytes
    const int STATE_BYTES = 16 + 8 + 4 + 12 + 16 + 12 + 12 + VehicleInputPacket::INPUT_BYTES + 4 + 1 + 12 + 4 + 1;

    // the payload: a count, then the states; at most MAX_STATES of them (the rest are left out)
    QByteArray pack(const std::vector<VehicleState>& states);
    // false for a payload that is not one of ours
    bool unpack(const QByteArray& payload, std::vector<VehicleState>& states);
}

namespace VehicleStateRequestPacket {
    const int MAX_IDS = 64;

    QByteArray pack(const std::vector<QUuid>& ids);
    bool unpack(const QByteArray& payload, std::vector<QUuid>& ids);
}

#endif // hifi_VehicleState_h
