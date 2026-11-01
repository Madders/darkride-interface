//
//  WheeledVehicleSpec.cpp
//  libraries/vehicles/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WheeledVehicleSpec.h"

#include <algorithm>
#include <cmath>
#include <map>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>

namespace {
    const float AXLE_TOLERANCE = 0.05f;   // m: wheels this close in z share an axle

    bool parseList(const QString& json, QJsonArray& list, QString* error) {
        list = QJsonArray();
        QString trimmed = json.trimmed();
        if (trimmed.isEmpty()) {
            return true;   // nothing is a valid empty list
        }
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            if (error) {
                *error = parseError.errorString();
            }
            return false;
        }
        if (!document.isArray()) {
            if (error) {
                *error = "not a list";
            }
            return false;
        }
        list = document.array();
        return true;
    }

    bool readVec3(const QJsonValue& value, glm::vec3& out) {
        if (value.isArray()) {
            QJsonArray array = value.toArray();
            if (array.size() < 3) {
                return false;
            }
            out = glm::vec3((float)array[0].toDouble(), (float)array[1].toDouble(), (float)array[2].toDouble());
            return true;
        }
        if (value.isObject()) {
            QJsonObject object = value.toObject();
            if (!object.contains("x") || !object.contains("y") || !object.contains("z")) {
                return false;
            }
            out = glm::vec3((float)object["x"].toDouble(), (float)object["y"].toDouble(), (float)object["z"].toDouble());
            return true;
        }
        return false;
    }

    float readFloat(const QJsonObject& object, const char* key, float fallback) {
        QJsonValue value = object.value(QLatin1String(key));
        return value.isDouble() ? (float)value.toDouble() : fallback;
    }

    // true/false, or a number (0 is false): Unity exporters write both
    bool readBool(const QJsonObject& object, const char* key, bool fallback) {
        QJsonValue value = object.value(QLatin1String(key));
        if (value.isBool()) {
            return value.toBool();
        }
        if (value.isDouble()) {
            return value.toDouble() != 0.0;
        }
        return fallback;
    }
}

bool WheeledVehicleSpec::parseWheels(const QString& json, std::vector<WheelSpec>& wheels, QString* error) {
    wheels.clear();
    QJsonArray list;
    if (!parseList(json, list, error)) {
        return false;
    }
    for (const QJsonValue& item : list) {
        if (!item.isObject()) {
            continue;
        }
        QJsonObject object = item.toObject();
        WheelSpec wheel;
        if (!readVec3(object.value("center"), wheel.center)) {
            if (error) {
                *error = "a wheel has no center";
            }
            continue;
        }
        wheel.name = object.value("name").toString();
        wheel.radius = glm::max(0.01f, readFloat(object, "radius", wheel.radius));
        wheel.width = glm::max(0.01f, readFloat(object, "width", wheel.width));
        wheel.mass = glm::max(0.1f, readFloat(object, "mass", wheel.mass));
        wheel.suspensionDistance = glm::max(0.0f, readFloat(object, "suspensionDistance", wheel.suspensionDistance));
        wheel.targetPosition = glm::clamp(readFloat(object, "targetPosition", wheel.targetPosition), 0.05f, 0.95f);
        wheel.spring = glm::max(0.0f, readFloat(object, "spring", wheel.spring));
        wheel.damper = glm::max(0.0f, readFloat(object, "damper", wheel.damper));
        wheel.steer = glm::clamp(readFloat(object, "steer", wheel.steer), -1.0f, 1.0f);
        if (object.contains("drive")) {
            wheel.drive = readBool(object, "drive", false) ? 1 : 0;
        }
        wheel.brake = glm::clamp(readFloat(object, "brake", wheel.brake), 0.0f, 1.0f);
        wheel.handBrake = glm::clamp(readFloat(object, "handBrake", wheel.handBrake), 0.0f, 1.0f);
        wheel.enableSuspensionForcePoint = readBool(object, "suspensionForcePoint", wheel.enableSuspensionForcePoint);
        if (wheel.name.isEmpty()) {
            wheel.name = QString("wheel%1").arg(wheels.size());
        }
        wheels.push_back(wheel);
    }
    return true;
}

bool WheeledVehicleSpec::parseSeats(const QString& json, std::vector<SeatSpec>& seats, QString* error) {
    seats.clear();
    QJsonArray list;
    if (!parseList(json, list, error)) {
        return false;
    }
    for (const QJsonValue& item : list) {
        if (!item.isObject()) {
            continue;
        }
        QJsonObject object = item.toObject();
        SeatSpec seat;
        if (!readVec3(object.value("position"), seat.position)) {
            if (error) {
                *error = "a seat has no position";
            }
            continue;
        }
        seat.name = object.value("name").toString();
        seat.yaw = readFloat(object, "yaw", 0.0f);
        seat.driver = readBool(object, "driver", false);
        if (seat.name.isEmpty()) {
            seat.name = QString("seat%1").arg(seats.size());
        }
        seats.push_back(seat);
    }
    return true;
}

bool WheeledVehicleSpec::parseGearRatios(const QString& json, std::vector<float>& ratios, QString* error) {
    ratios.clear();
    QJsonArray list;
    if (!parseList(json, list, error)) {
        return false;
    }
    for (const QJsonValue& item : list) {
        if (item.isDouble() && item.toDouble() > 0.0) {
            ratios.push_back((float)item.toDouble());
        }
    }
    return true;
}

WheeledVehicleSpec::DriveType WheeledVehicleSpec::parseDriveType(const QString& name) {
    QString lower = name.trimmed().toLower();
    if (lower == "front" || lower == "fwd") {
        return DriveType::FRONT;
    }
    if (lower == "all" || lower == "awd" || lower == "4wd" || lower == "4x4") {
        return DriveType::ALL;
    }
    return DriveType::REAR;
}

std::vector<std::pair<int, int>> WheeledVehicleSpec::axles() const {
    // group by z, front to back
    std::vector<int> order(wheels.size());
    for (size_t i = 0; i < wheels.size(); ++i) {
        order[i] = (int)i;
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return wheels[a].center.z < wheels[b].center.z; });

    std::vector<std::pair<int, int>> result;
    size_t i = 0;
    while (i < order.size()) {
        int first = order[i];
        if (i + 1 < order.size() && fabsf(wheels[order[i + 1]].center.z - wheels[first].center.z) <= AXLE_TOLERANCE) {
            int second = order[i + 1];
            // (left, right) by x
            if (wheels[first].center.x <= wheels[second].center.x) {
                result.emplace_back(first, second);
            } else {
                result.emplace_back(second, first);
            }
            i += 2;
        } else {
            result.emplace_back(first, first);
            i += 1;
        }
    }
    return result;
}

bool WheeledVehicleSpec::isDriven(int wheelIndex) const {
    if (wheelIndex < 0 || wheelIndex >= (int)wheels.size()) {
        return false;
    }
    const WheelSpec& wheel = wheels[wheelIndex];
    if (wheel.drive >= 0) {
        return wheel.drive == 1;
    }
    if (driveType == DriveType::ALL) {
        return true;
    }
    std::vector<std::pair<int, int>> pairs = axles();
    if (pairs.empty()) {
        return false;
    }
    const std::pair<int, int>& axle = driveType == DriveType::FRONT ? pairs.front() : pairs.back();
    return axle.first == wheelIndex || axle.second == wheelIndex;
}

int WheeledVehicleSpec::driverSeat() const {
    for (size_t i = 0; i < seats.size(); ++i) {
        if (seats[i].driver) {
            return (int)i;
        }
    }
    return -1;
}

float WheeledVehicleSpec::wheelbase() const {
    if (wheels.empty()) {
        return 0.0f;
    }
    float front = wheels[0].center.z;
    float back = front;
    for (const WheelSpec& wheel : wheels) {
        front = std::min(front, wheel.center.z);
        back = std::max(back, wheel.center.z);
    }
    return back - front;
}

float WheeledVehicleSpec::steeringFor(float current, float wanted, float speed, float dt) const {
    const float STEER_IN_TIME = 0.2f;    // s from straight ahead to full lock
    const float STEER_OUT_TIME = 0.12f;  // s from full lock back to straight ahead
    // g: at the tyres' grip or under (kart 2's hold about 1.1 g on a 0.9 ground); the kart test's 1.5 slid 35 degrees at
    // 16 m/s and 3 spun, 0.8 and 1.0 held on both a 0.9 and a 0.5 ground
    const float STEER_GRIP = 1.0f;
    const float GRAVITY = 9.8f;
    wanted = glm::clamp(wanted, -1.0f, 1.0f);
    float lock = glm::radians(maxSteerAngle);
    float length = wheelbase();
    if (lock > 0.0f && length > 0.0f && speed > 0.0f) {
        float most = atanf(STEER_GRIP * GRAVITY * length / (speed * speed));
        float fraction = std::min(1.0f, most / lock);
        wanted = glm::clamp(wanted, -fraction, fraction);
    }
    // out from the centre at the slower rate; back toward it, or across it, at the quicker
    bool outward = wanted * current >= 0.0f && fabsf(wanted) > fabsf(current);
    float step = std::max(0.0f, dt) / (outward ? STEER_IN_TIME : STEER_OUT_TIME);
    return current + glm::clamp(wanted - current, -step, step);
}
