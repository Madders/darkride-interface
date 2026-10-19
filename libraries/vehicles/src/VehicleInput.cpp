//
//  VehicleInput.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "VehicleInput.h"

#include <cstring>

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
}

QByteArray VehicleInputPacket::pack(const QUuid& vehicleID, const std::vector<VehicleInput>& inputs) {
    QByteArray bytes;
    bytes.append(vehicleID.toRfc4122());
    size_t first = inputs.size() > (size_t)MAX_INPUTS ? inputs.size() - MAX_INPUTS : 0;
    uint8_t count = (uint8_t)(inputs.size() - first);
    put(bytes, count);
    for (size_t i = first; i < inputs.size(); ++i) {
        const VehicleInput& input = inputs[i];
        put(bytes, input.sequence);
        put(bytes, input.throttle);
        put(bytes, input.steering);
        put(bytes, input.brake);
        put(bytes, input.handBrake);
        put(bytes, input.pitch);
        put(bytes, input.lift);
        put(bytes, input.trim);
    }
    return bytes;
}

bool VehicleInputPacket::unpack(const QByteArray& payload, QUuid& vehicleID, std::vector<VehicleInput>& inputs) {
    inputs.clear();
    if (payload.size() < UUID_BYTES + 1) {
        return false;
    }
    vehicleID = QUuid::fromRfc4122(payload.left(UUID_BYTES));
    int offset = UUID_BYTES;
    uint8_t count = 0;
    if (!get(payload, offset, count) || count > MAX_INPUTS) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        VehicleInput input;
        if (!get(payload, offset, input.sequence) || !get(payload, offset, input.throttle) || !get(payload, offset, input.steering)
                || !get(payload, offset, input.brake) || !get(payload, offset, input.handBrake)
                || !get(payload, offset, input.pitch) || !get(payload, offset, input.lift) || !get(payload, offset, input.trim)) {
            inputs.clear();
            return false;
        }
        inputs.push_back(input);
    }
    return true;
}
