//
//  EditorVehicleGizmos.h
//  interface/src/editor
//
//  Wireframes for a selected vehicle (PHYSICS.md 4.8), the way Unity draws a WheelCollider: each wheel as a cylinder
//  at its rest position with a line for the suspension's travel, each seat as a square with an arrow for its facing,
//  and crosses for the points the physics acts at (the centre of mass; an airship's centre of buoyancy, thrust and fin
//  points; an aircraft's wing point). The part selected in the Inspector's list is drawn bright. World-space lines in
//  one draw call, in the front layer, as EditorBounds draws its boxes.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorVehicleGizmos_h
#define hifi_EditorVehicleGizmos_h

#include <memory>
#include <mutex>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <render/Forward.h>
#include <render/Item.h>
#include <render/Scene.h>

struct EditorVehicleGizmoWheel {
    glm::vec3 center { 0.0f };     // local: the suspension's top (fully compressed)
    float radius { 0.35f };
    float width { 0.2f };
    float travel { 0.2f };         // the suspension's distance below center
    float rest { 0.5f };           // where it rests, as a fraction of the travel down from center
    bool chosen { false };

    bool operator==(const EditorVehicleGizmoWheel& other) const {
        return center == other.center && radius == other.radius && width == other.width && travel == other.travel
            && rest == other.rest && chosen == other.chosen;
    }
};

struct EditorVehicleGizmoSeat {
    glm::vec3 position { 0.0f };   // local: the hips
    float yaw { 0.0f };            // degrees about the vehicle's up, 0 facing its forward (-z)
    bool driver { false };
    bool chosen { false };

    bool operator==(const EditorVehicleGizmoSeat& other) const {
        return position == other.position && yaw == other.yaw && driver == other.driver && chosen == other.chosen;
    }
};

struct EditorVehicleGizmoPoint {
    glm::vec3 position { 0.0f };   // local
    float size { 0.2f };           // the cross's arms, tip to tip
    glm::vec4 color { 1.0f };

    bool operator==(const EditorVehicleGizmoPoint& other) const {
        return position == other.position && size == other.size && color == other.color;
    }
};

struct EditorVehicleGizmoVehicle {
    glm::vec3 position { 0.0f };   // the entity's, in the world
    glm::quat rotation;
    std::vector<EditorVehicleGizmoWheel> wheels;
    std::vector<EditorVehicleGizmoSeat> seats;
    std::vector<EditorVehicleGizmoPoint> points;

    bool operator==(const EditorVehicleGizmoVehicle& other) const {
        return position == other.position && rotation == other.rotation && wheels == other.wheels
            && seats == other.seats && points == other.points;
    }
};

class EditorVehicleGizmosShared {
public:
    void setVehicles(const std::vector<EditorVehicleGizmoVehicle>& vehicles);
    void render(RenderArgs* args);
    render::Item::Bound bound() const;

private:
    mutable std::mutex _mutex;
    std::vector<EditorVehicleGizmoVehicle> _vehicles;
    int _linesId { -1 };
};

class EditorVehicleGizmosRenderData {
public:
    using Payload = render::Payload<EditorVehicleGizmosRenderData>;
    using Pointer = Payload::DataPointer;
    explicit EditorVehicleGizmosRenderData(const std::shared_ptr<EditorVehicleGizmosShared>& shared) : _shared(shared) {}
    std::shared_ptr<EditorVehicleGizmosShared> _shared;
};

namespace render {
    template <> const ItemKey payloadGetKey(const EditorVehicleGizmosRenderData::Pointer& data);
    template <> const Item::Bound payloadGetBound(const EditorVehicleGizmosRenderData::Pointer& data);
    template <> void payloadRender(const EditorVehicleGizmosRenderData::Pointer& data, RenderArgs* args);
}

class EditorVehicleGizmos {
public:
    // replaces what is drawn; an empty list draws nothing
    void setVehicles(const std::vector<EditorVehicleGizmoVehicle>& vehicles);

private:
    void ensureItem();
    std::shared_ptr<EditorVehicleGizmosShared> _shared { std::make_shared<EditorVehicleGizmosShared>() };
    render::ItemID _item { render::Item::INVALID_ITEM_ID };
    bool _empty { true };
};

#endif // hifi_EditorVehicleGizmos_h
