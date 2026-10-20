//
//  EditorBrushOutline.h
//  interface/src/editor
//
//  The outline of the terrain brush: a ring laid on the ground under the pointer, the size of the brush, drawn over
//  everything so it is seen in a hollow as well as on a hill. Unity projects its brush the same way.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorBrushOutline_h
#define hifi_EditorBrushOutline_h

#include <memory>
#include <mutex>

#include <QtCore/QVector>

#include <glm/glm.hpp>

#include <render/Forward.h>
#include <render/Item.h>
#include <render/Scene.h>

#include "EditorGizmoLines.h"

class EditorBrushOutlineShared {
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

class EditorBrushOutlineRenderData {
public:
    using Payload = render::Payload<EditorBrushOutlineRenderData>;
    using Pointer = Payload::DataPointer;
    explicit EditorBrushOutlineRenderData(const std::shared_ptr<EditorBrushOutlineShared>& shared) : _shared(shared) {}
    std::shared_ptr<EditorBrushOutlineShared> _shared;
};

namespace render {
    template <> const ItemKey payloadGetKey(const EditorBrushOutlineRenderData::Pointer& data);
    template <> const Item::Bound payloadGetBound(const EditorBrushOutlineRenderData::Pointer& data);
    template <> void payloadRender(const EditorBrushOutlineRenderData::Pointer& data, RenderArgs* args);
}

class EditorBrushOutline {
public:
    // the ring's points in the world, in order, closed; width in metres; the lines turn to face toCamera
    void set(const QVector<glm::vec3>& ring, const glm::vec4& color, float width, const glm::vec3& toCamera);
    // several closed rings at once: a brush shape with holes or pieces
    void set(const QVector<QVector<glm::vec3>>& rings, const glm::vec4& color, float width, const glm::vec3& toCamera);
    void clear();

private:
    void ensureItem();
    void publish(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high);

    std::shared_ptr<EditorBrushOutlineShared> _shared { std::make_shared<EditorBrushOutlineShared>() };
    render::ItemID _item { render::Item::INVALID_ITEM_ID };
    bool _empty { true };
};

#endif // hifi_EditorBrushOutline_h
