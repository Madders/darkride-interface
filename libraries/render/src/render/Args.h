//
//  Created by Brad Hefta-Gaub on 10/29/14.
//  Copyright 2013-2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_render_Args_h
#define hifi_render_Args_h

#include <functional>
#include <memory>
#include <stack>

#include <GLMHelpers.h>
#include <ViewFrustum.h>
#include <StencilMaskMode.h>

#include <gpu/Forward.h>
#include "Forward.h"


class AABox;

namespace render {
    class ShapePlumber;
    class RenderDetails {
    public:
        enum Type {
            ITEM,
            SHADOW,
            OTHER
        };

        struct Item {
            int _considered = 0;
            int _outOfView = 0;
            int _tooSmall = 0;
            int _rendered = 0;
        };

        int _materialSwitches = 0;
        int _trianglesRendered = 0;
        // GPU instancing (INSTANCING.md): the draws that took an instance list, and the instances they drew
        int _instancedDrawcalls = 0;
        int _instancesDrawn = 0;
        // shadow maps given to spot and point lights this frame (one for a spot light, six for a point light), how
        // many of those maps were drawn again rather than kept, and how many of those had their static casters'
        // map drawn again too (the rest copied it and drew only the dynamic casters)
        int _localShadowMaps = 0;
        int _localShadowMapsDrawn = 0;
        int _localShadowStaticMapsDrawn = 0;
        // mirrors drawn this frame (a view each, render-utils Mirrors.h) and the pixels their pictures add up to
        int _mirrorsDrawn = 0;
        int _mirrorPixels = 0;

        Item _item;
        Item _shadow;
        Item _other;

        Item& edit(Type type) {
            switch (type) {
                case SHADOW:
                    return _shadow;
                case ITEM:
                    return _item;
                default:
                    return _other;
            }
        }
    };


    class Args {
    public:
        enum RenderMode { DEFAULT_RENDER_MODE, SHADOW_RENDER_MODE, MIRROR_RENDER_MODE, SECONDARY_CAMERA_RENDER_MODE };
        enum DisplayMode { MONO, STEREO_MONITOR, STEREO_HMD };
        enum RenderMethod { DEFERRED, FORWARD };
        enum DebugFlags {
            RENDER_DEBUG_NONE = 0,
            RENDER_DEBUG_HULLS = 1
        };

        Args() {}

        Args(const gpu::ContextPointer& context,
            float sizeScale = 1.0f,
            int boundaryLevelAdjust = 0,
            float lodAngleHalfTan = 0.1f,
            RenderMode renderMode = DEFAULT_RENDER_MODE,
            DisplayMode displayMode = MONO,
            RenderMethod renderMethod = DEFERRED,
            DebugFlags debugFlags = RENDER_DEBUG_NONE,
            gpu::Batch* batch = nullptr) :
            _context(context),
            _sizeScale(sizeScale),
            _boundaryLevelAdjust(boundaryLevelAdjust),
            _lodAngleHalfTan(lodAngleHalfTan),
            _lodAngleHalfTanSq(lodAngleHalfTan * lodAngleHalfTan),
            _renderMode(renderMode),
            _displayMode(displayMode),
            _renderMethod(renderMethod),
            _debugFlags(debugFlags),
            _batch(batch) {
        }

        bool hasViewFrustum() const { return _viewFrustums.size() > 0; }
        void setViewFrustum(const ViewFrustum& viewFrustum) {
            while (_viewFrustums.size() > 0) {
                _viewFrustums.pop();
            }
            _viewFrustums.push(viewFrustum);
        }
        const ViewFrustum& getViewFrustum() const { assert(_viewFrustums.size() > 0); return _viewFrustums.top(); }
        void pushViewFrustum(const ViewFrustum& viewFrustum) { _viewFrustums.push(viewFrustum); }
        void popViewFrustum() { _viewFrustums.pop(); }

        bool isStereo() const { return _displayMode != MONO; }

        std::shared_ptr<gpu::Context> _context;
        std::shared_ptr<gpu::Framebuffer> _blitFramebuffer;
        std::shared_ptr<render::ShapePipeline> _shapePipeline;
        // while a shape item draws: the plumber its pass picks pipelines from, for an item whose own pipeline (a custom
        // shader) can't draw and falls back to a plain one (MeshPartPayload::bindPlainPipeline)
        const render::ShapePlumber* _shapePlumber { nullptr };
        std::stack<ViewFrustum> _viewFrustums;
        glm::ivec4 _viewport { 0.0f, 0.0f, 1.0f, 1.0f };
        glm::vec3 _boomOffset { 0.0f, 0.0f, 1.0f };

        float _sizeScale { 1.0f };
        int _boundaryLevelAdjust { 0 };
        float _lodAngleHalfTan{ 0.1f };
        float _lodAngleHalfTanSq{ _lodAngleHalfTan  * _lodAngleHalfTan };
        // the impostor switch (IMPOSTORS.md 2.4, ImpostorSwitch): an entity whose bounding sphere subtends less than
        // this (the square of the angle's half tangent, as _lodAngleHalfTanSq) is drawn as its card; 0 shows no card
        float _impostorAngleHalfTanSq{ 0.0f };
        // the impostor card's debug view (LODManager.impostorDebug, impostor.slf): 0 draws the card, other values paint
        // what the shader reads
        int _impostorDebug{ 0 };
        // the impostor card's march (LODManager.impostorMarch, impostor.slf): 0 as first built, 1 with the exact
        // savings, 2 also stepping by the screen's pixels, at least this many pixel footprints a step
        int _impostorMarch{ 2 };
        float _impostorMarchFootprints{ 0.25f };
        // the impostor cross-fade forced to a share (LODManager.impostorCrossfade, a test hook): below 0 the switch
        // times its own; 0 draws the model alone, 1 the card alone, between them both, dissolved
        float _impostorCrossfade{ -1.0f };
        // automatic LODs (AUTOMATIC-LODS.md 3.3, LODManager.meshLodErrorPixels): a model's part draws the coarsest level
        // of its mesh whose error looks no bigger than this many pixels from the view's camera; 0 draws every mesh in full
        float _meshLodErrorPixels{ 0.0f };
        // a level every mesh draws (LODManager.meshLodForceLevel, a test hook): below 0 the error decides; 0 is full
        int _meshLodForceLevel{ -1 };
        // the camera a shadow map's casters choose their level from while a light's view is pushed (the shadow
        // setups set it): its position and its pixels per radian at the view's centre; 0 for none yet
        glm::vec3 _shadowLodEye;
        float _shadowLodPixelsPerRadian{ 0.0f };

        RenderMode _renderMode { DEFAULT_RENDER_MODE };
        DisplayMode _displayMode { MONO };
        RenderMethod _renderMethod { DEFERRED };
        DebugFlags _debugFlags { RENDER_DEBUG_NONE };
        gpu::Batch* _batch = nullptr;

        // the finished picture is shown flipped left to right, as a mirror shows you: the selfie view (Caity's design,
        // 2026-10-28: like FaceTime's own picture) and the full-screen mirror camera. ToneMapAndResample flips the
        // picture; the 2D UI, drawn after it, stays readable; the pick ray flips with it (ViewState::computePickRay).
        // Never a stereo view
        bool _mirrored { false };

        uint32_t _globalShapeKey { 0 };
        uint32_t _itemShapeKey { 0 };
        bool _enableTexturing { true };
        bool _enableBlendshape { true };
        bool _enableSkinning { true };

        bool _enableFade { false };

        // GPU instancing (INSTANCING.md 3): the draw jobs group the items that can share a draw (Developer > Render >
        // GPU Instancing); each grouping pass is numbered, so an item knows the state it settled for it is still current
        bool _enableInstancing { true };
        uint32_t _instancingPass { 0 };

        RenderDetails _details;
        render::ScenePointer _scene;
        int8_t _cameraMode { -1 };

        std::function<void(gpu::Batch&, const gpu::TexturePointer&)> _hudOperator { nullptr };
        gpu::TexturePointer _hudTexture { nullptr };

        bool _takingSnapshot { false };
        StencilMaskMode _stencilMaskMode { StencilMaskMode::NONE };
        std::function<void(gpu::Batch&)> _stencilMaskOperator;

        float _visionSqueezeX { 0.0f };
        float _visionSqueezeY { 0.0f };
        float _visionSqueezeTransition { 0.15f };
        int _visionSqueezePerEye { 0 };
        float _visionSqueezeGroundPlaneY { 0.0f };
        float _visionSqueezeSpotlightSize { 0.02f };
    };

}

using RenderArgs = render::Args;

#endif // hifi_render_Args_h
