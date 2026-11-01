//
//  EditorTrackGizmos.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorTrackGizmos.h"

#include <cfloat>
#include <cmath>

#include <GeometryCache.h>
#include <GLMHelpers.h>
#include <NumericalConstants.h>
#include <RenderHifi.h>
#include <Transform.h>

#include "Application.h"

namespace {
    // sizes on screen, in viewport pixels (points on a Retina display)
    const float LINE_PIXELS = 2.0f;
    const float TICK_PIXELS = 1.5f;
    const float POINT_PIXELS = 7.0f;          // a control point's diamond, tip to tip
    const float CHOSEN_POINT_PIXELS = 10.0f;
    const float STATION_RADIUS_PIXELS = 9.0f;
    const float POINT_PICK_PIXELS = 9.0f;
    const float CURVE_PICK_PIXELS = 8.0f;
    const int STATION_SEGMENTS = 20;
    // along the track, in metres
    const float SAMPLE_SPACING = 0.5f;        // between the vertices of the drawn curve, about
    const float TICK_SPACING = 4.0f;          // between the ticks for the banked up, about
    // the most vertices and ticks a track is drawn with: a longer track gets them further apart (a mistyped point
    // made a curve of thousands of kilometres, sampled every half metre every frame, 10-21 review)
    const float MAX_SAMPLES = 4096.0f;
    const float MAX_TICKS = 512.0f;
    const float TICK_LENGTH = 0.6f;

    // teal, apart from the lights' yellow and the zones' green; dimmer for a track not selected
    const glm::vec3 TRACK_COLOR { 0.35f, 0.9f, 0.9f };
    const float UNSELECTED_BRIGHTNESS = 0.6f;
    const glm::vec3 STATION_COLOR { 1.0f, 0.75f, 0.35f };
    const glm::vec3 POINT_COLOR { 0.85f, 0.97f, 0.97f };
    const glm::vec3 LIT_POINT_COLOR { 0.96f, 0.95f, 0.2f };   // chosen, under the pointer, or being dragged, as the gizmo's

    // two directions across `normal`, at right angles to it and to each other
    void across(const glm::vec3& normal, glm::vec3& first, glm::vec3& second) {
        first = glm::normalize(glm::cross(normal, std::abs(normal.y) < 0.9f ? Vectors::UNIT_Y : Vectors::UNIT_X));
        second = glm::cross(normal, first);
    }

    // where the ray passes nearest the point, when it passes within `radius` of it in front of the ray's start
    bool rayNearPoint(const PickRay& ray, const glm::vec3& point, float radius, float& distanceAlong) {
        glm::vec3 direction = glm::normalize(ray.direction);
        float along = glm::dot(point - ray.origin, direction);
        if (along < 0.0f) {
            return false;
        }
        if (glm::length(ray.origin + direction * along - point) > radius) {
            return false;
        }
        distanceAlong = along;
        return true;
    }

    // the points of the ray and the segment a-b nearest each other: the ray's parameter and the segment's (0..1)
    void rayNearSegment(const PickRay& ray, const glm::vec3& a, const glm::vec3& b, float& alongRay, float& alongSegment) {
        glm::vec3 d = glm::normalize(ray.direction);
        glm::vec3 e = b - a;
        glm::vec3 r = ray.origin - a;
        float dd = 1.0f, de = glm::dot(d, e), ee = glm::dot(e, e), dr = glm::dot(d, r), er = glm::dot(e, r);
        float denominator = dd * ee - de * de;
        if (ee < 1.0e-8f) {
            alongSegment = 0.0f;
            alongRay = std::max(0.0f, -dr);
            return;
        }
        if (std::abs(denominator) < 1.0e-8f) {
            alongSegment = 0.0f;   // parallel: the segment's start
        } else {
            alongSegment = glm::clamp((dd * er - de * dr) / denominator, 0.0f, 1.0f);
        }
        alongRay = std::max(0.0f, de * alongSegment - dr);
    }
}


void EditorTrackGizmosShared::setLines(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high) {
    std::lock_guard<std::mutex> lock(_mutex);
    _lines = lines;
    _low = low;
    _high = high;
}

render::Item::Bound EditorTrackGizmosShared::bound() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_lines.isEmpty()) {
        return render::Item::Bound(glm::vec3(0.0f), 0.0f);
    }
    return render::Item::Bound(_low, _high - _low);
}

void EditorTrackGizmosShared::render(RenderArgs* args) {
    if (!args || !args->_batch) {
        return;
    }
    EditorGizmoLines lines;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        lines = _lines;
    }
    if (lines.isEmpty()) {
        return;
    }
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (_bufferId < 0) {
        _bufferId = geometryCache->allocateID();
    }
    auto& batch = *args->_batch;
    // the colour is in the vertices; drawn from both sides, in world space
    geometryCache->bindSimpleProgram(batch, false, false, true, false, true, true, graphics::MaterialKey::CULL_NONE);
    batch.setModelTransform(Transform());
    lines.draw(batch, *geometryCache, _bufferId);
}

namespace render {
    template <> const ItemKey payloadGetKey(const EditorTrackGizmosRenderData::Pointer& data) {
        return ItemKey::Builder::opaqueShape().withLayer(render::hifi::LAYER_3D_FRONT)
            .withTagBits(ItemKey::TAG_BITS_0).withoutShadowCaster();
    }
    template <> const Item::Bound payloadGetBound(const EditorTrackGizmosRenderData::Pointer& data) {
        return data ? data->_shared->bound() : Item::Bound();
    }
    template <> void payloadRender(const EditorTrackGizmosRenderData::Pointer& data, RenderArgs* args) {
        if (data) {
            data->_shared->render(args);
        }
    }
}


void EditorTrackGizmos::update(const std::vector<EditorTrackGizmoTrack>& tracks, const QUuid& handlesFor, int chosenPoint,
                               const glm::vec3& cameraPosition, const glm::vec3& viewDirection, float pixelSize) {
    _tracks = tracks;
    _cameraPosition = cameraPosition;
    _viewDirection = glm::normalize(viewDirection);
    _pixelSize = pixelSize;
    _curves.resize(_tracks.size());
    for (size_t i = 0; i < _tracks.size(); ++i) {
        sampleCurve(_tracks[i], _curves[i]);
    }
    _hasHandleTrack = false;
    _chosen = -1;
    if (!handlesFor.isNull()) {
        for (size_t i = 0; i < _tracks.size(); ++i) {
            if (_tracks[i].id == handlesFor) {
                _handleTrack = _tracks[i];
                _hasHandleTrack = true;
                _handleCurve = _curves[i].curve;
                _handlePolyline.clear();
                _handlePolyline.reserve(_curves[i].poses.size());
                for (const TrackCurve::Pose& pose : _curves[i].poses) {
                    _handlePolyline.push_back(_handleTrack.position + _handleTrack.rotation * pose.position);
                }
                _chosen = chosenPoint < (int)_handleTrack.points.size() ? chosenPoint : -1;
                break;
            }
        }
    }
    if (!_hasHandleTrack) {
        _hover = -1;
        _active = -1;
        _handlePolyline.clear();
    }
    rebuild();
}

void EditorTrackGizmos::clear() {
    _tracks.clear();
    _curves.clear();
    _hasHandleTrack = false;
    _handlePolyline.clear();
    _chosen = _hover = _active = -1;
    rebuild();
}

void EditorTrackGizmos::setHover(int point) {
    if (point != _hover) {
        _hover = point;
        rebuild();
    }
}

void EditorTrackGizmos::setActive(int point) {
    if (point != _active) {
        _active = point;
        rebuild();
    }
}

bool EditorTrackGizmos::handleTrack(EditorTrackGizmoTrack& track) const {
    if (_hasHandleTrack) {
        track = _handleTrack;
    }
    return _hasHandleTrack;
}

glm::vec3 EditorTrackGizmos::pointInWorld(int index) const {
    if (!_hasHandleTrack || index < 0 || index >= (int)_handleTrack.points.size()) {
        return glm::vec3(0.0f);
    }
    return _handleTrack.position + _handleTrack.rotation * _handleTrack.points[index];
}

float EditorTrackGizmos::pixelsAt(const glm::vec3& point) const {
    // the depth in front of the camera, not the distance, so that things keep their size out at the edges
    const float MIN_DEPTH = 0.05f;
    return _pixelSize * std::max(glm::dot(point - _cameraPosition, _viewDirection), MIN_DEPTH);
}

void EditorTrackGizmos::sampleCurve(const EditorTrackGizmoTrack& track, Curve& curve) const {
    if (curve.points == track.points && curve.bank == track.bank && curve.closed == track.closed && !curve.poses.empty()) {
        return;   // as sampled last time
    }
    curve.points = track.points;
    curve.bank = track.bank;
    curve.closed = track.closed;
    curve.curve.build(track.points, track.bank, track.closed);
    float spacing = std::max(SAMPLE_SPACING, curve.curve.length() / MAX_SAMPLES);
    curve.poses = curve.curve.isValid() ? curve.curve.poses(spacing) : std::vector<TrackCurve::Pose>();
}

int EditorTrackGizmos::pointAt(const PickRay& ray) const {
    int nearest = -1;
    if (!_hasHandleTrack) {
        return nearest;
    }
    float nearestAlong = FLT_MAX;
    for (int index = 0; index < (int)_handleTrack.points.size(); ++index) {
        glm::vec3 position = pointInWorld(index);
        float along;
        if (rayNearPoint(ray, position, POINT_PICK_PIXELS * pixelsAt(position), along) && along < nearestAlong) {
            nearestAlong = along;
            nearest = index;
        }
    }
    return nearest;
}

bool EditorTrackGizmos::curveAt(const PickRay& ray, float& arcLength, glm::vec3& worldPoint) const {
    if (!_hasHandleTrack || _handlePolyline.size() < 2 || !_handleCurve.isValid()) {
        return false;
    }
    float bestDistance = FLT_MAX;
    float bestAlongRay = FLT_MAX;
    size_t bestSegment = 0;
    float bestT = 0.0f;
    glm::vec3 direction = glm::normalize(ray.direction);
    for (size_t i = 0; i + 1 < _handlePolyline.size(); ++i) {
        float alongRay, t;
        rayNearSegment(ray, _handlePolyline[i], _handlePolyline[i + 1], alongRay, t);
        glm::vec3 onCurve = _handlePolyline[i] + (_handlePolyline[i + 1] - _handlePolyline[i]) * t;
        float distance = glm::length(ray.origin + direction * alongRay - onCurve);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestAlongRay = alongRay;
            bestSegment = i;
            bestT = t;
        }
    }
    glm::vec3 onCurve = _handlePolyline[bestSegment] + (_handlePolyline[bestSegment + 1] - _handlePolyline[bestSegment]) * bestT;
    if (bestDistance > CURVE_PICK_PIXELS * pixelsAt(onCurve) || bestAlongRay <= 0.0f) {
        return false;
    }
    // the arc length of the vertices is even along the curve; the exact spot is asked of the curve itself
    glm::vec3 local = glm::inverse(_handleTrack.rotation) * (onCurve - _handleTrack.position);
    arcLength = _handleCurve.nearest(local);
    worldPoint = _handleTrack.position + _handleTrack.rotation * _handleCurve.positionAt(arcLength);
    return true;
}

void EditorTrackGizmos::rebuild() {
    EditorGizmoLines lines;
    glm::vec3 low(FLT_MAX);
    glm::vec3 high(-FLT_MAX);
    auto include = [&](const glm::vec3& point, float reach) {
        low = glm::min(low, point - glm::vec3(reach));
        high = glm::max(high, point + glm::vec3(reach));
    };
    auto facingAt = [&](const glm::vec3& point) {
        glm::vec3 toCamera = _cameraPosition - point;
        return glm::length(toCamera) > 1.0e-6f ? glm::normalize(toCamera) : -_viewDirection;
    };

    for (size_t t = 0; t < _tracks.size(); ++t) {
        const EditorTrackGizmoTrack& track = _tracks[t];
        const Curve& curve = _curves[t];
        bool handles = _hasHandleTrack && track.id == _handleTrack.id;
        glm::vec3 brightness = track.selected ? TRACK_COLOR : TRACK_COLOR * UNSELECTED_BRIGHTNESS;
        glm::vec4 color(brightness, 1.0f);

        // the curve, widened to a set number of pixels where it is nearest the camera (one width for the whole
        // line keeps it one draw)
        if (curve.poses.size() >= 2) {
            QVector<glm::vec3> line;
            line.reserve((int)curve.poses.size());
            float nearestPixel = FLT_MAX;
            for (const TrackCurve::Pose& pose : curve.poses) {
                glm::vec3 world = track.position + track.rotation * pose.position;
                line << world;
                nearestPixel = std::min(nearestPixel, pixelsAt(world));
                include(world, 0.0f);
            }
            lines.setToCamera(facingAt(line[line.size() / 2]));
            float width = LINE_PIXELS * nearestPixel;
            // a closed track's polyline ends where it began: drawn open, it meets itself
            lines.addLine(line, false, width, color);

            // ticks for the banked up, every few metres: the lean of the beam is visible from afar
            float length = curve.curve.length();
            float tickSpacing = std::max(TICK_SPACING, length / MAX_TICKS);
            int ticks = std::max(1, (int)std::min(std::floor(length / tickSpacing), MAX_TICKS));
            for (int k = 0; k <= ticks; ++k) {
                float s = std::min(length, tickSpacing * (float)k);
                TrackCurve::Pose pose = curve.curve.poseAt(s);
                glm::vec3 world = track.position + track.rotation * pose.position;
                glm::vec3 up = track.rotation * pose.up;
                lines.addSegment(world, world + up * TICK_LENGTH, TICK_PIXELS * pixelsAt(world), color);
            }
        }

        // a ring at each station, a diamond at each control point; the handle track's points bright when chosen
        for (size_t i = 0; i < track.points.size(); ++i) {
            glm::vec3 world = track.position + track.rotation * track.points[i];
            float pixel = pixelsAt(world);
            glm::vec3 facing = facingAt(world);
            glm::vec3 first, second;
            across(facing, first, second);
            bool station = i < track.dwell.size() && track.dwell[i] > 0.0f;
            if (station) {
                QVector<glm::vec3> ring;
                float radius = STATION_RADIUS_PIXELS * pixel;
                for (int k = 0; k < STATION_SEGMENTS; ++k) {
                    float angle = TWO_PI * (float)k / (float)STATION_SEGMENTS;
                    ring << world + (first * std::cos(angle) + second * std::sin(angle)) * radius;
                }
                lines.setToCamera(facing);
                lines.addLine(ring, true, TICK_PIXELS * pixel, glm::vec4(STATION_COLOR, 1.0f));
                include(world, radius);
            }
            bool lit = handles && (_active >= 0 ? _active == (int)i : (_chosen == (int)i || _hover == (int)i));
            float half = 0.5f * (handles && _chosen == (int)i ? CHOSEN_POINT_PIXELS : POINT_PIXELS) * pixel;
            glm::vec4 pointColor(lit ? LIT_POINT_COLOR : (handles ? POINT_COLOR : brightness), 1.0f);
            lines.addQuad(world - first * half, world - second * half, world + first * half, world + second * half, pointColor);
            include(world, half);
        }
    }

    publish(lines, low, high);
}

void EditorTrackGizmos::ensureItem() {
    if (render::Item::isValidID(_item)) {
        return;
    }
    auto scene = qApp->getMain3DScene();
    if (!scene) {
        return;
    }
    render::Transaction transaction;
    _item = scene->allocateID();
    transaction.resetItem(_item, std::make_shared<EditorTrackGizmosRenderData::Payload>(
        std::make_shared<EditorTrackGizmosRenderData>(_shared)));
    scene->enqueueTransaction(transaction);
}

void EditorTrackGizmos::publish(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high) {
    if (lines.isEmpty() && _empty) {
        return;
    }
    _empty = lines.isEmpty();
    _shared->setLines(lines, low, high);
    if (!_empty) {
        ensureItem();
    }
    if (render::Item::isValidID(_item)) {
        if (auto scene = qApp->getMain3DScene()) {
            render::Transaction transaction;
            transaction.updateItem(_item);
            scene->enqueueTransaction(transaction);
        }
    }
}
