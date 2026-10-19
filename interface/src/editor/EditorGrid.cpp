//
//  EditorGrid.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorGrid.h"

#include <cmath>

#include <GeometryCache.h>
#include <GLMHelpers.h>
#include <NumericalConstants.h>
#include <PerfStat.h>
#include <StencilMaskPass.h>
#include <Transform.h>
#include <shaders/Shaders.h>

#include "Application.h"

namespace {
    // matches EditorGridParams in editorGrid.slf
    struct EditorGridParams {
        glm::vec4 spacing;
        glm::vec4 minorColor;
        glm::vec4 majorColor;
        glm::vec4 axisXColor;
        glm::vec4 axisZColor;
        glm::vec4 edgeColor;
    };

    // dark lines with a faint light edge either side: the line shows on bright sand, the edge on dark grass
    // (light grey lines alone vanished on the sand)
    const glm::vec4 MINOR_COLOR { 0.0f, 0.0f, 0.0f, 0.5f };
    const glm::vec4 MAJOR_COLOR { 0.0f, 0.0f, 0.0f, 0.7f };
    const glm::vec4 EDGE_COLOR { 1.0f, 1.0f, 1.0f, 0.08f };
    const glm::vec4 AXIS_X_COLOR { 0.88f, 0.26f, 0.24f, 0.9f };
    const glm::vec4 AXIS_Z_COLOR { 0.24f, 0.52f, 0.88f, 0.9f };

    // the quad reaches a little past the fade distance so its edge is never seen
    const float QUAD_OVERSIZE = 1.1f;
}

namespace render {
    template <> const ItemKey payloadGetKey(const EditorGridRenderData::Pointer& data) {
        return ItemKey::Builder::transparentShape().withTagBits(ItemKey::TAG_BITS_0).withoutShadowCaster();
    }

    template <> const Item::Bound payloadGetBound(const EditorGridRenderData::Pointer& data) {
        // the quad is centred under the camera, so bound it there
        float half = data->settings().fadeDistance * QUAD_OVERSIZE;
        return Item::Bound(data->lastEye() - glm::vec3(half), 2.0f * half);
    }

    template <> void payloadRender(const EditorGridRenderData::Pointer& data, RenderArgs* args) {
        if (data) {
            data->render(args);
        }
    }
}

EditorGridRenderData::Settings EditorGridRenderData::settings() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _settings;
}

void EditorGridRenderData::setSettings(const Settings& settings) {
    std::lock_guard<std::mutex> lock(_mutex);
    _settings = settings;
}

glm::vec3 EditorGridRenderData::lastEye() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _lastEye;
}

void EditorGridRenderData::ensurePipelines() {
    if (_pipeline) {
        return;
    }
    using namespace shader::render_utils::program;

    auto makeState = [] {
        auto state = std::make_shared<gpu::State>();
        // occluded by the world, never occludes it
        state->setDepthTest(true, false, gpu::GREATER_EQUAL);
        state->setBlendFunction(true,
            gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
            gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
        state->setCullMode(gpu::State::CULL_NONE);
        PrepareStencil::testMask(*state);
        return state;
    };

    _pipeline = gpu::Pipeline::create(gpu::Shader::createProgram(editorGrid), makeState());
    _pipelineForward = gpu::Pipeline::create(gpu::Shader::createProgram(editorGrid_forward), makeState());

    EditorGridParams params;
    _paramsBuffer = std::make_shared<gpu::Buffer>(sizeof(EditorGridParams), (const gpu::Byte*)&params);
    _quadId = DependencyManager::get<GeometryCache>()->allocateID();
}

void EditorGridRenderData::render(RenderArgs* args) {
    Settings settings = this->settings();
    if (!settings.visible || !args || !args->_batch || !args->hasViewFrustum()) {
        return;
    }
    PerformanceTimer perfTimer("editorGrid");

    ensurePipelines();
    auto& batch = *args->_batch;
    bool forward = args->_renderMethod == render::Args::RenderMethod::FORWARD;
    batch.setPipeline(forward ? _pipelineForward : _pipeline);

    EditorGridParams params;
    params.spacing = glm::vec4(settings.minorSpacing, settings.majorSpacing, settings.fadeDistance, settings.pixelsPerPoint);
    params.minorColor = MINOR_COLOR;
    params.majorColor = MAJOR_COLOR;
    params.axisXColor = AXIS_X_COLOR;
    params.axisZColor = AXIS_Z_COLOR;
    params.edgeColor = EDGE_COLOR;
    _paramsBuffer->setSubData(0, params);
    batch.setUniformBuffer(0, _paramsBuffer, 0, sizeof(EditorGridParams));

    // one quad centred under the camera, snapped to the major spacing so the lines stay put in the world
    glm::vec3 eye = args->getViewFrustum().getPosition();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _lastEye = eye;
    }
    float half = settings.fadeDistance * QUAD_OVERSIZE;
    float snap = settings.majorSpacing;
    glm::vec3 center(std::round(eye.x / snap) * snap, settings.height, std::round(eye.z / snap) * snap);

    Transform transform;
    transform.setTranslation(center);
    transform.setRotation(glm::angleAxis(-PI_OVER_TWO, Vectors::UNIT_X));   // lay the XY quad flat on XZ
    batch.setModelTransform(transform);

    static const glm::vec2 MIN_TEX_COORD(0.0f, 0.0f);
    static const glm::vec2 MAX_TEX_COORD(1.0f, 1.0f);
    DependencyManager::get<GeometryCache>()->renderQuad(batch, glm::vec2(-half), glm::vec2(half),
        MIN_TEX_COORD, MAX_TEX_COORD, glm::vec4(1.0f), _quadId);
}

void EditorGrid::setVisible(bool visible) {
    auto settings = _data->settings();
    settings.visible = visible;
    _data->setSettings(settings);
    if (visible) {
        ensureItem();
    }
}

void EditorGrid::setHeight(float height) {
    auto settings = _data->settings();
    settings.height = height;
    _data->setSettings(settings);
}

void EditorGrid::setPixelsPerPoint(float pixelsPerPoint) {
    auto settings = _data->settings();
    if (settings.pixelsPerPoint != pixelsPerPoint) {
        settings.pixelsPerPoint = pixelsPerPoint;
        _data->setSettings(settings);
    }
}

void EditorGrid::ensureItem() {
    if (render::Item::isValidID(_item)) {
        return;
    }
    auto scene = qApp->getMain3DScene();
    if (!scene) {
        return;
    }
    _item = scene->allocateID();
    render::Transaction transaction;
    transaction.resetItem(_item, std::make_shared<EditorGridRenderData::Payload>(_data));
    scene->enqueueTransaction(transaction);
}
