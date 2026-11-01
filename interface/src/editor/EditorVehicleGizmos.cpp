//
//  EditorVehicleGizmos.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorVehicleGizmos.h"

#include <cfloat>
#include <cmath>

#include <QtCore/QVector>

#include <GeometryCache.h>
#include <GLMHelpers.h>
#include <NumericalConstants.h>
#include <PerfStat.h>
#include <RenderHifi.h>

#include "Application.h"

namespace {
    const int CIRCLE_SEGMENTS = 32;
    const float SEAT_SIZE = 0.4f;          // m, the square under the hips
    const float SEAT_ARROW = 0.5f;         // m, the facing
    const float SEAT_ARROW_HEAD = 0.12f;   // m

    // the collider green Unity draws a WheelCollider in; the chosen part in the selection's yellow
    const glm::vec4 WHEEL_COLOR(0.45f, 0.9f, 0.45f, 1.0f);
    const glm::vec4 TRAVEL_COLOR(0.3f, 0.65f, 0.3f, 1.0f);
    const glm::vec4 SEAT_COLOR(0.55f, 0.8f, 1.0f, 1.0f);
    const glm::vec4 DRIVER_COLOR(0.8f, 0.92f, 1.0f, 1.0f);
    const glm::vec4 CHOSEN_COLOR(1.0f, 0.92f, 0.25f, 1.0f);

    struct Lines {
        const EditorVehicleGizmoVehicle& vehicle;
        QVector<glm::vec3>& points;
        QVector<glm::vec4>& colors;

        glm::vec3 world(const glm::vec3& local) const {
            return vehicle.position + vehicle.rotation * local;
        }
        void line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color) {
            points << world(a) << world(b);
            colors << color << color;
        }
        // a circle of `radius` round `center`, in the plane across `axis` (a unit local axis)
        void circle(const glm::vec3& center, const glm::vec3& axis, float radius, const glm::vec4& color) {
            glm::vec3 u = std::abs(axis.y) < 0.9f ? glm::normalize(glm::cross(axis, glm::vec3(0.0f, 1.0f, 0.0f)))
                                                  : glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 v = glm::cross(axis, u);
            glm::vec3 previous;
            for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
                float angle = (float)i / CIRCLE_SEGMENTS * TWO_PI;
                glm::vec3 point = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
                if (i > 0) {
                    line(previous, point, color);
                }
                previous = point;
            }
        }
        void cross(const glm::vec3& center, float size, const glm::vec4& color) {
            float half = size * 0.5f;
            line(center - glm::vec3(half, 0.0f, 0.0f), center + glm::vec3(half, 0.0f, 0.0f), color);
            line(center - glm::vec3(0.0f, half, 0.0f), center + glm::vec3(0.0f, half, 0.0f), color);
            line(center - glm::vec3(0.0f, 0.0f, half), center + glm::vec3(0.0f, 0.0f, half), color);
        }
    };

    void appendWheel(Lines& lines, const EditorVehicleGizmoWheel& wheel) {
        const glm::vec3 axle(1.0f, 0.0f, 0.0f);   // the wheels spin about the vehicle's x
        const glm::vec3 down(0.0f, -1.0f, 0.0f);
        glm::vec4 color = wheel.chosen ? CHOSEN_COLOR : WHEEL_COLOR;
        glm::vec4 travelColor = wheel.chosen ? CHOSEN_COLOR : TRAVEL_COLOR;
        glm::vec3 rest = wheel.center + down * (wheel.travel * wheel.rest);
        // the tyre: a circle at each side, joined at four points, and the axle between them
        glm::vec3 side = axle * (wheel.width * 0.5f);
        lines.circle(rest - side, axle, wheel.radius, color);
        lines.circle(rest + side, axle, wheel.radius, color);
        for (int i = 0; i < 4; ++i) {
            float angle = (float)i * 0.5f * (float)M_PI;
            glm::vec3 rim(0.0f, std::cos(angle) * wheel.radius, std::sin(angle) * wheel.radius);
            lines.line(rest - side + rim, rest + side + rim, color);
        }
        lines.line(rest - side, rest + side, color);
        // the suspension's travel, from the top (fully compressed) to the bottom (fully extended), with a tick at each end
        glm::vec3 bottom = wheel.center + down * wheel.travel;
        glm::vec3 tick = axle * (wheel.width * 0.6f);
        lines.line(wheel.center, bottom, travelColor);
        lines.line(wheel.center - tick, wheel.center + tick, travelColor);
        lines.line(bottom - tick, bottom + tick, travelColor);
    }

    void appendSeat(Lines& lines, const EditorVehicleGizmoSeat& seat) {
        glm::vec4 color = seat.chosen ? CHOSEN_COLOR : (seat.driver ? DRIVER_COLOR : SEAT_COLOR);
        float half = SEAT_SIZE * 0.5f;
        const glm::vec3& p = seat.position;
        glm::vec3 corners[4] = { p + glm::vec3(-half, 0.0f, -half), p + glm::vec3(half, 0.0f, -half),
                                 p + glm::vec3(half, 0.0f, half), p + glm::vec3(-half, 0.0f, half) };
        for (int i = 0; i < 4; ++i) {
            lines.line(corners[i], corners[(i + 1) % 4], color);
        }
        // the facing: a yaw about the vehicle's up turns its forward (-z)
        float yaw = glm::radians(seat.yaw);
        glm::vec3 forward(-std::sin(yaw), 0.0f, -std::cos(yaw));
        glm::vec3 right(std::cos(yaw), 0.0f, -std::sin(yaw));
        glm::vec3 tip = p + forward * SEAT_ARROW;
        lines.line(p, tip, color);
        lines.line(tip, tip - forward * SEAT_ARROW_HEAD + right * SEAT_ARROW_HEAD, color);
        lines.line(tip, tip - forward * SEAT_ARROW_HEAD - right * SEAT_ARROW_HEAD, color);
    }
}

void EditorVehicleGizmosShared::setVehicles(const std::vector<EditorVehicleGizmoVehicle>& vehicles) {
    std::lock_guard<std::mutex> lock(_mutex);
    _vehicles = vehicles;
}

render::Item::Bound EditorVehicleGizmosShared::bound() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_vehicles.empty()) {
        return render::Item::Bound(glm::vec3(0.0f), 0.0f);
    }
    glm::vec3 low(FLT_MAX);
    glm::vec3 high(-FLT_MAX);
    for (const auto& vehicle : _vehicles) {
        float reach = 1.0f;
        for (const auto& wheel : vehicle.wheels) {
            reach = glm::max(reach, glm::length(wheel.center) + wheel.radius + wheel.travel + wheel.width);
        }
        for (const auto& seat : vehicle.seats) {
            reach = glm::max(reach, glm::length(seat.position) + SEAT_ARROW + SEAT_SIZE);
        }
        for (const auto& point : vehicle.points) {
            reach = glm::max(reach, glm::length(point.position) + point.size);
        }
        low = glm::min(low, vehicle.position - glm::vec3(reach));
        high = glm::max(high, vehicle.position + glm::vec3(reach));
    }
    return render::Item::Bound(low, high - low);
}

void EditorVehicleGizmosShared::render(RenderArgs* args) {
    if (!args || !args->_batch) {
        return;
    }
    std::vector<EditorVehicleGizmoVehicle> vehicles;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        vehicles = _vehicles;
    }
    if (vehicles.empty()) {
        return;
    }
    PerformanceTimer perfTimer("editorVehicleGizmos");
    QVector<glm::vec3> points;
    QVector<glm::vec4> colors;
    for (const auto& vehicle : vehicles) {
        Lines lines { vehicle, points, colors };
        for (const auto& wheel : vehicle.wheels) {
            appendWheel(lines, wheel);
        }
        for (const auto& seat : vehicle.seats) {
            appendSeat(lines, seat);
        }
        for (const auto& point : vehicle.points) {
            lines.cross(point.position, point.size, point.color);
        }
    }
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (_linesId < 0) {
        _linesId = geometryCache->allocateID();
    }
    auto& batch = *args->_batch;
    geometryCache->bindSimpleProgram(batch, false, false, true, false, true, true, graphics::MaterialKey::CULL_NONE);
    batch.setModelTransform(Transform());
    geometryCache->updateVertices(_linesId, points, colors);
    geometryCache->renderVertices(batch, gpu::LINES, _linesId);
}

namespace render {
    template <> const ItemKey payloadGetKey(const EditorVehicleGizmosRenderData::Pointer& data) {
        return ItemKey::Builder::opaqueShape().withLayer(render::hifi::LAYER_3D_FRONT)
            .withTagBits(ItemKey::TAG_BITS_0).withoutShadowCaster();
    }
    template <> const Item::Bound payloadGetBound(const EditorVehicleGizmosRenderData::Pointer& data) {
        return data ? data->_shared->bound() : Item::Bound();
    }
    template <> void payloadRender(const EditorVehicleGizmosRenderData::Pointer& data, RenderArgs* args) {
        if (data) {
            data->_shared->render(args);
        }
    }
}

void EditorVehicleGizmos::ensureItem() {
    if (render::Item::isValidID(_item)) {
        return;
    }
    auto scene = qApp->getMain3DScene();
    if (!scene) {
        return;
    }
    render::Transaction transaction;
    _item = scene->allocateID();
    transaction.resetItem(_item, std::make_shared<EditorVehicleGizmosRenderData::Payload>(
        std::make_shared<EditorVehicleGizmosRenderData>(_shared)));
    scene->enqueueTransaction(transaction);
}

void EditorVehicleGizmos::setVehicles(const std::vector<EditorVehicleGizmoVehicle>& vehicles) {
    if (vehicles.empty() && _empty) {
        return;
    }
    _empty = vehicles.empty();
    _shared->setVehicles(vehicles);
    if (!_empty) {
        ensureItem();
    }
    // the item's bound moved with the vehicle: let the scene know
    if (render::Item::isValidID(_item)) {
        if (auto scene = qApp->getMain3DScene()) {
            render::Transaction transaction;
            transaction.updateItem(_item);
            scene->enqueueTransaction(transaction);
        }
    }
}
