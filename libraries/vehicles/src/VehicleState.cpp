//
//  VehicleState.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "VehicleState.h"

#include <cstring>

#include <glm/gtc/quaternion.hpp>

namespace {
    const int UUID_BYTES = 16;

    template <typename T> void put(QByteArray& bytes, const T& value) {
        bytes.append(reinterpret_cast<const char*>(&value), sizeof(T));
    }
    template <typename T> bool get(const QByteArray& bytes, int& offset, T& value) {
        if (offset + (int)sizeof(T) > bytes.size()) {
            return false;
        }
        memcpy(&value, bytes.constData() + offset, sizeof(T));
        offset += sizeof(T);
        return true;
    }
    void putVec3(QByteArray& bytes, const glm::vec3& v) { put(bytes, v.x); put(bytes, v.y); put(bytes, v.z); }
    void putQuat(QByteArray& bytes, const glm::quat& q) { put(bytes, q.x); put(bytes, q.y); put(bytes, q.z); put(bytes, q.w); }
    bool getVec3(const QByteArray& bytes, int& offset, glm::vec3& v) {
        return get(bytes, offset, v.x) && get(bytes, offset, v.y) && get(bytes, offset, v.z);
    }
    bool getQuat(const QByteArray& bytes, int& offset, glm::quat& q) {
        return get(bytes, offset, q.x) && get(bytes, offset, q.y) && get(bytes, offset, q.z) && get(bytes, offset, q.w);
    }
    void putInput(QByteArray& bytes, const VehicleInput& input) {
        put(bytes, input.sequence);
        put(bytes, input.throttle);
        put(bytes, input.steering);
        put(bytes, input.brake);
        put(bytes, input.handBrake);
        put(bytes, input.pitch);
        put(bytes, input.lift);
        put(bytes, input.trim);
    }
    bool getInput(const QByteArray& bytes, int& offset, VehicleInput& input) {
        return get(bytes, offset, input.sequence) && get(bytes, offset, input.throttle) && get(bytes, offset, input.steering)
            && get(bytes, offset, input.brake) && get(bytes, offset, input.handBrake) && get(bytes, offset, input.pitch)
            && get(bytes, offset, input.lift) && get(bytes, offset, input.trim);
    }
}

VehicleState VehicleState::interpolate(const VehicleState& a, const VehicleState& b, float alpha) {
    VehicleState s = a;
    s.time = a.time + (uint64_t)((double)(b.time - a.time) * (double)alpha);
    s.position = glm::mix(a.position, b.position, alpha);
    s.rotation = glm::normalize(glm::slerp(a.rotation, b.rotation, alpha));
    s.velocity = glm::mix(a.velocity, b.velocity, alpha);
    s.angularVelocity = glm::mix(a.angularVelocity, b.angularVelocity, alpha);
    s.engineRPM = glm::mix(a.engineRPM, b.engineRPM, alpha);
    s.ballast = glm::mix(a.ballast, b.ballast, alpha);
    s.fill = glm::mix(a.fill, b.fill, alpha);
    s.tilt = glm::mix(a.tilt, b.tilt, alpha);
    return s;
}

QByteArray VehicleStatePacket::pack(const std::vector<VehicleState>& states) {
    QByteArray bytes;
    uint8_t count = (uint8_t)(states.size() > (size_t)MAX_STATES ? MAX_STATES : states.size());
    put(bytes, count);
    for (int i = 0; i < count; ++i) {
        const VehicleState& s = states[i];
        bytes.append(s.id.toRfc4122());
        put(bytes, s.time);
        put(bytes, s.step);
        putVec3(bytes, s.position);
        putQuat(bytes, s.rotation);
        putVec3(bytes, s.velocity);
        putVec3(bytes, s.angularVelocity);
        putInput(bytes, s.input);
        put(bytes, s.engineRPM);
        put(bytes, s.gear);
        put(bytes, s.ballast);
        put(bytes, s.fill);
        put(bytes, s.tilt);
        put(bytes, s.tick);
        put(bytes, s.margin);
    }
    return bytes;
}

bool VehicleStatePacket::unpack(const QByteArray& payload, std::vector<VehicleState>& states) {
    states.clear();
    int offset = 0;
    uint8_t count = 0;
    if (!get(payload, offset, count) || count > MAX_STATES) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        VehicleState s;
        if (offset + UUID_BYTES > payload.size()) {
            states.clear();
            return false;
        }
        s.id = QUuid::fromRfc4122(payload.mid(offset, UUID_BYTES));
        offset += UUID_BYTES;
        if (!get(payload, offset, s.time) || !get(payload, offset, s.step) || !getVec3(payload, offset, s.position)
                || !getQuat(payload, offset, s.rotation) || !getVec3(payload, offset, s.velocity)
                || !getVec3(payload, offset, s.angularVelocity) || !getInput(payload, offset, s.input)
                || !get(payload, offset, s.engineRPM) || !get(payload, offset, s.gear) || !get(payload, offset, s.ballast)
                || !get(payload, offset, s.fill) || !get(payload, offset, s.tilt) || !get(payload, offset, s.tick)
                || !get(payload, offset, s.margin)) {
            states.clear();
            return false;
        }
        states.push_back(s);
    }
    return true;
}

QByteArray VehicleStateRequestPacket::pack(const std::vector<QUuid>& ids) {
    QByteArray bytes;
    uint8_t count = (uint8_t)(ids.size() > (size_t)MAX_IDS ? MAX_IDS : ids.size());
    put(bytes, count);
    for (int i = 0; i < count; ++i) {
        bytes.append(ids[i].toRfc4122());
    }
    return bytes;
}

bool VehicleStateRequestPacket::unpack(const QByteArray& payload, std::vector<QUuid>& ids) {
    ids.clear();
    int offset = 0;
    uint8_t count = 0;
    if (!get(payload, offset, count) || count > MAX_IDS || payload.size() < offset + count * UUID_BYTES) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        ids.push_back(QUuid::fromRfc4122(payload.mid(offset, UUID_BYTES)));
        offset += UUID_BYTES;
    }
    return true;
}
