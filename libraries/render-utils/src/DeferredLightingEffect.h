//
//  DeferredLightingEffect.h
//  interface/src/renderer
//
//  Created by Andrzej Kapolka on 9/11/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DeferredLightingEffect_h
#define hifi_DeferredLightingEffect_h

#include <QVector>

#include <DependencyManager.h>
#include <NumericalConstants.h>

#include "graphics/Light.h"
#include "graphics/Geometry.h"

#include <procedural/ProceduralSkybox.h>

#include <render/CullTask.h>

#include "DeferredFrameTransform.h"
#include "DeferredFramebuffer.h"
#include "LightingModel.h"

#include "LightStage.h"
#include "LightClusters.h"
#include "BackgroundStage.h"
#include "HazeStage.h"

#include "SurfaceGeometryPass.h"
#include "SubsurfaceScattering.h"
#include "AmbientOcclusionEffect.h"

// THis is where we currently accumulate the local lights, let s change that sooner than later
class DeferredLightingEffect : public Dependency {
    SINGLETON_DEPENDENCY
    
public:
    void init();
 
    // the key light and the ambient light of the frame's zone, with the zone's probe volume (or an empty one) at
    // probeVolumeSlot: the material, forward and translucent programs' slot by default (-1), the deferred directional
    // pass gives its own (render_utils::slot::texture::ProbeVolumeDeferred)
    static void setupKeyLightBatch(const RenderArgs* args, gpu::Batch& batch, int probeVolumeSlot = -1);
    static void setupKeyLightBatch(const RenderArgs* args, gpu::Batch& batch, const LightStage::Frame& lightFrame, int probeVolumeSlot = -1);
    static void unsetKeyLightBatch(gpu::Batch& batch, int probeVolumeSlot = -1);
    // a 1 x 9 x 1 black probe volume, for the shaders' sampler when the zone has none
    static const gpu::TexturePointer& getEmptyProbeVolume();
    // the probe volume the key light setup last bound (the zone's, or the empty one): the material binding puts it
    // back after the material texture table, whose entries cover its slot (RenderPipelines::bindMaterials)
    static const gpu::TexturePointer& getFrameProbeVolume();

    // localShadows: the spot lights casting shadows this frame, or null for none
    static void setupLocalLightsBatch(gpu::Batch& batch, const LightClustersPointer& lightClusters, const LocalShadowsPointer& localShadows);
    static void unsetLocalLightsBatch(gpu::Batch& batch);

    // The frame's lighting for the forward pipelines: the view's light clusters, as its clustering pass built them,
    // and its shadow frame (the sun's cascades, the lights' shadow maps and cookies), as its shadow tasks left it.
    // Each view publishes them before it draws (SetFrameLighting, in the forward task and the deferred one: the
    // in-front and HUD layers of a deferred frame draw with the forward pipelines too), and the forward pipelines'
    // batch setter binds them at every pipeline change
    void setFrameLighting(const LightClustersPointer& lightClusters, const LightStage::ShadowFramePointer& shadowFrame,
                          const LightingModelPointer& lightingModel);
    // Binds what the forward shaders light with: the light array and the clusters (a grid of one empty cluster
    // before any view has published), the lights' shadow atlas, cookies and table, and the sun's cascades with
    // their parameters (a cascade count of 0 with no shadow this frame, or shadows off: the shaders then skip the
    // sampling). Bound again at every pipeline change, since a program drawn in between may keep its own maps and
    // buffers at the same slots (the terrain's arrays at the shadow texture units, its chunk at the parameters' slot)
    void setupFrameLightingBatch(gpu::Batch& batch, const RenderArgs* args) const;
    // A procedural's draw sets a pipeline of its own (Procedural::prepare), which the forward pipelines' batch setter
    // never sees: whether the view lights in place (the forward path, not its shadow pass), so that an opaque
    // procedural draws its forward variant, and the lights that variant reads, the key light and the ambient with the
    // frame's above
    static bool isForwardLit(const RenderArgs* args);
    void setupForwardLightingBatch(const RenderArgs* args, gpu::Batch& batch) const;

private:
    static gpu::TexturePointer _frameProbeVolume;
    DeferredLightingEffect() = default;

    LightClustersPointer _frameLightClusters;
    LightStage::ShadowFramePointer _frameShadowFrame;
    bool _frameShadowsEnabled { false };

    graphics::MeshPointer _pointLightMesh;
    graphics::MeshPointer getPointLightMesh();
    graphics::MeshPointer _spotLightMesh;
    graphics::MeshPointer getSpotLightMesh();

    gpu::PipelinePointer _directionalSkyboxLight;
    gpu::PipelinePointer _directionalAmbientSphereLight;

    gpu::PipelinePointer _directionalSkyboxLightShadow;
    gpu::PipelinePointer _directionalAmbientSphereLightShadow;

    gpu::PipelinePointer _localLight;
    gpu::PipelinePointer _localLightOutline;

    friend class LightClusteringPass;
    friend class RenderDeferredSetup;
    friend class RenderDeferredLocals;
    friend class RenderDeferredCleanup;
};

// Publishes the view's light clusters and shadow frame to the DeferredLightingEffect for the forward pipelines
class SetFrameLighting {
public:
    using Inputs = render::VaryingSet3<LightClustersPointer, LightStage::ShadowFramePointer, LightingModelPointer>;
    using JobModel = render::Job::ModelI<SetFrameLighting, Inputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);
};

class PrepareDeferred {
public:
    // Inputs: primaryFramebuffer and lightingModel
    using Inputs = render::VaryingSet2 <gpu::FramebufferPointer, LightingModelPointer>;
    // Output: DeferredFramebuffer, LightingFramebuffer
    using Outputs = render::VaryingSet2<DeferredFramebufferPointer, gpu::FramebufferPointer>;

    using JobModel = render::Job::ModelIO<PrepareDeferred, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& outputs);

    DeferredFramebufferPointer _deferredFramebuffer;
};

class RenderDeferredSetup {
public:

    void run(const render::RenderContextPointer& renderContext,
        const DeferredFrameTransformPointer& frameTransform,
        const DeferredFramebufferPointer& deferredFramebuffer,
        const LightingModelPointer& lightingModel,
        const LightStage::FramePointer& lightFrame,
        const LightStage::ShadowFramePointer& shadowFrame,
        const HazeStage::FramePointer& hazeFrame,
        const SurfaceGeometryFramebufferPointer& surfaceGeometryFramebuffer,
        const AmbientOcclusionFramebufferPointer& ambientOcclusionFramebuffer,
        const SubsurfaceScatteringResourcePointer& subsurfaceScatteringResource,
        const LightClustersPointer& lightClusters);
};

class RenderDeferredLocals {
public:
    using JobModel = render::Job::ModelI<RenderDeferredLocals, DeferredFrameTransformPointer>;
    
    void run(const render::RenderContextPointer& renderContext,
        const DeferredFrameTransformPointer& frameTransform,
        const DeferredFramebufferPointer& deferredFramebuffer,
        const LightingModelPointer& lightingModel,
        const SurfaceGeometryFramebufferPointer& surfaceGeometryFramebuffer,
        const LightClustersPointer& lightClusters,
        const LightStage::ShadowFramePointer& shadowFrame);

    gpu::BufferView _localLightsBuffer;

    RenderDeferredLocals();
};


class RenderDeferredCleanup {
public:
    using JobModel = render::Job::Model<RenderDeferredCleanup>;
    
    void run(const render::RenderContextPointer& renderContext);
};

using RenderDeferredConfig = render::GPUJobConfig;

class RenderDeferred {
public:
    using ExtraDeferredBuffer = render::VaryingSet3<SurfaceGeometryFramebufferPointer, AmbientOcclusionFramebufferPointer, SubsurfaceScatteringResourcePointer>;
    using Inputs = render::VaryingSet8<
        DeferredFrameTransformPointer, DeferredFramebufferPointer, ExtraDeferredBuffer, LightingModelPointer, LightClustersPointer, LightStage::FramePointer, LightStage::ShadowFramePointer, HazeStage::FramePointer>;

    using Config = RenderDeferredConfig;
    using JobModel = render::Job::ModelI<RenderDeferred, Inputs, Config>;

    RenderDeferred();

    void configure(const Config& config);

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);
    
    RenderDeferredSetup setupJob;
    RenderDeferredLocals lightsJob;
    RenderDeferredCleanup cleanupJob;

protected:
    gpu::RangeTimerPointer _gpuTimer;

private:
};

class DefaultLightingSetup {
public:
    using JobModel = render::Job::Model<DefaultLightingSetup>;

    void run(const render::RenderContextPointer& renderContext);

protected:
    graphics::LightPointer _defaultLight;
    LightStage::Index _defaultLightID{ LightStage::INVALID_INDEX };
    graphics::SunSkyStagePointer _defaultBackground;
    BackgroundStage::Index _defaultBackgroundID{ BackgroundStage::INVALID_INDEX };
    graphics::HazePointer _defaultHaze{ nullptr };
    HazeStage::Index _defaultHazeID{ HazeStage::INVALID_INDEX };
    graphics::SkyboxPointer _defaultSkybox { new ProceduralSkybox() };
    NetworkTexturePointer _defaultSkyboxNetworkTexture;
    NetworkTexturePointer _defaultAmbientNetworkTexture;
    gpu::TexturePointer _defaultAmbientTexture;
};

#endif // hifi_DeferredLightingEffect_h
