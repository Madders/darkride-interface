//
//  EditorTrackGizmos.h
//  interface/src/editor
//
//  The Track entity in build mode (PHYSICS.md 4.8, the monorail): its curve drawn through its control points with a
//  tick now and then for the banked up, a ring at each station, a diamond at each control point, and, for the one
//  track that is selected on its own, handles on those diamonds: the chosen point is bright and the Move gizmo
//  stands on it. The curve is sampled from the same TrackCurve the physics drives, so what is drawn is what a car
//  rides. World-space lines in one draw call in the front layer, as the light gizmos are.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorTrackGizmos_h
#define hifi_EditorTrackGizmos_h

#include <memory>
#include <mutex>
#include <vector>

#include <QtCore/QUuid>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <RegisteredMetaTypes.h>
#include <TrackCurve.h>
#include <render/Forward.h>
#include <render/Item.h>
#include <render/Scene.h>

#include "EditorGizmoLines.h"

struct EditorTrackGizmoTrack {
    QUuid id;
    glm::vec3 position { 0.0f };    // the entity's, in the world: the curve's origin
    glm::quat rotation;
    std::vector<glm::vec3> points;  // the control points, in the track's own space
    std::vector<float> bank;        // degrees per point
    std::vector<float> dwell;       // seconds per point; more than zero is a station
    bool closed { false };
    bool selected { false };
};

class EditorTrackGizmosShared {
public:
    void setLines(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high);
    void render(RenderArgs* args);
    render::Item::Bound bound() const;

private:
    mutable std::mutex _mutex;
    EditorGizmoLines _lines;
    glm::vec3 _low { 0.0f };
    glm::vec3 _high { 0.0f };
    int _bufferId { -1 };
};

class EditorTrackGizmosRenderData {
public:
    using Payload = render::Payload<EditorTrackGizmosRenderData>;
    using Pointer = Payload::DataPointer;
    explicit EditorTrackGizmosRenderData(const std::shared_ptr<EditorTrackGizmosShared>& shared) : _shared(shared) {}
    std::shared_ptr<EditorTrackGizmosShared> _shared;
};

namespace render {
    template <> const ItemKey payloadGetKey(const EditorTrackGizmosRenderData::Pointer& data);
    template <> const Item::Bound payloadGetBound(const EditorTrackGizmosRenderData::Pointer& data);
    template <> void payloadRender(const EditorTrackGizmosRenderData::Pointer& data, RenderArgs* args);
}

class EditorTrackGizmos {
public:
    // the tracks to show and the one whose points get handles (a null id for none) with the point chosen in it (-1
    // for none), seen from the camera; pixelSize is the size of a viewport pixel one metre in front of the camera
    void update(const std::vector<EditorTrackGizmoTrack>& tracks, const QUuid& handlesFor, int chosenPoint,
                const glm::vec3& cameraPosition, const glm::vec3& viewDirection, float pixelSize);
    void clear();

    // the handle (a control point's index) the ray meets first, or -1
    int pointAt(const PickRay& ray) const;
    // where the ray passes nearest the handle track's curve, within a few pixels of it: the arc length there and
    // the point of the curve, in the world. For inserting a control point by a click on the curve.
    bool curveAt(const PickRay& ray, float& arcLength, glm::vec3& worldPoint) const;
    void setHover(int point);
    void setActive(int point);

    // the track that has handles, as last shown
    bool handleTrack(EditorTrackGizmoTrack& track) const;
    // its curve, built from the points as last shown
    const TrackCurve& handleCurve() const { return _handleCurve; }
    // a control point of the handle track, in the world
    glm::vec3 pointInWorld(int index) const;

private:
    struct Curve {
        std::vector<glm::vec3> points;
        std::vector<float> bank;
        bool closed { false };
        TrackCurve curve;
        std::vector<TrackCurve::Pose> poses;   // sampled for drawing, in the track's own space
    };
    void rebuild();
    void sampleCurve(const EditorTrackGizmoTrack& track, Curve& curve) const;
    float pixelsAt(const glm::vec3& point) const;   // the size of a pixel there
    void ensureItem();
    void publish(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high);

    std::vector<EditorTrackGizmoTrack> _tracks;
    std::vector<Curve> _curves;   // one per track, kept between updates so an unchanged curve is not resampled
    EditorTrackGizmoTrack _handleTrack;
    bool _hasHandleTrack { false };
    TrackCurve _handleCurve;
    std::vector<glm::vec3> _handlePolyline;   // the handle track's curve in the world, for curveAt
    int _chosen { -1 };
    int _hover { -1 };
    int _active { -1 };
    glm::vec3 _cameraPosition;
    glm::vec3 _viewDirection { 0.0f, 0.0f, -1.0f };
    float _pixelSize { 0.001f };

    std::shared_ptr<EditorTrackGizmosShared> _shared { std::make_shared<EditorTrackGizmosShared>() };
    render::ItemID _item { render::Item::INVALID_ITEM_ID };
    bool _empty { true };
};

#endif // hifi_EditorTrackGizmos_h
