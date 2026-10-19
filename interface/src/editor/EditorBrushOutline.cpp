//
//  EditorBrushOutline.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorBrushOutline.h"

#include <DependencyManager.h>
#include <GeometryCache.h>

#include "Application.h"

// --- render thread ------------------------------------------------------------------------------

void EditorBrushOutlineShared::setLines(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high) {
    std::lock_guard<std::mutex> lock(_mutex);
    _lines = lines;
    _low = low;
    _high = high;
}

render::Item::Bound EditorBrushOutlineShared::bound() const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_lines.isEmpty()) {
        return render::Item::Bound();
    }
    return render::Item::Bound(_low, _high - _low);
}

void EditorBrushOutlineShared::render(RenderArgs* args) {
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
    template <> const ItemKey payloadGetKey(const EditorBrushOutlineRenderData::Pointer& data) {
        return ItemKey::Builder::opaqueShape().withLayer(render::hifi::LAYER_3D_FRONT)
            .withTagBits(ItemKey::TAG_BITS_0).withoutShadowCaster();
    }
    template <> const Item::Bound payloadGetBound(const EditorBrushOutlineRenderData::Pointer& data) {
        return data ? data->_shared->bound() : Item::Bound();
    }
    template <> void payloadRender(const EditorBrushOutlineRenderData::Pointer& data, RenderArgs* args) {
        if (data) {
            data->_shared->render(args);
        }
    }
}

// --- main thread --------------------------------------------------------------------------------

void EditorBrushOutline::set(const QVector<glm::vec3>& ring, const glm::vec4& color, float width, const glm::vec3& toCamera) {
    set(QVector<QVector<glm::vec3>>{ ring }, color, width, toCamera);
}

void EditorBrushOutline::set(const QVector<QVector<glm::vec3>>& rings, const glm::vec4& color, float width, const glm::vec3& toCamera) {
    EditorGizmoLines lines(toCamera);
    bool any = false;
    glm::vec3 low(0.0f);
    glm::vec3 high(0.0f);
    for (const QVector<glm::vec3>& ring : rings) {
        if (ring.size() < 3) {
            continue;
        }
        lines.addLine(ring, true, width, color);
        for (const glm::vec3& point : ring) {
            low = any ? glm::min(low, point) : point;
            high = any ? glm::max(high, point) : point;
            any = true;
        }
    }
    if (!any) {
        clear();
        return;
    }
    publish(lines, low - width, high + width);
}

void EditorBrushOutline::clear() {
    publish(EditorGizmoLines(), glm::vec3(0.0f), glm::vec3(0.0f));
}

void EditorBrushOutline::ensureItem() {
    if (render::Item::isValidID(_item)) {
        return;
    }
    auto scene = qApp->getMain3DScene();
    if (!scene) {
        return;
    }
    render::Transaction transaction;
    _item = scene->allocateID();
    transaction.resetItem(_item, std::make_shared<EditorBrushOutlineRenderData::Payload>(
        std::make_shared<EditorBrushOutlineRenderData>(_shared)));
    scene->enqueueTransaction(transaction);
}

void EditorBrushOutline::publish(const EditorGizmoLines& lines, const glm::vec3& low, const glm::vec3& high) {
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
