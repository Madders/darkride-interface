//
//  LightClusters.h
//
//  Created by Sam Gateau on 9/7/2016 (a frustum grid); Unity URP's Forward+ layout since 10/2026.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_render_utils_LightClusters_h
#define hifi_render_utils_LightClusters_h

#include <ViewFrustum.h>
#include <gpu/Buffer.h>
#include <render/Engine.h>
#include "LightStage.h"
#include "ReflectionProbeStage.h"
#include "DeferredFrameTransform.h"
#include "LightingModel.h"
#include "SurfaceGeometryPass.h"

// The frame's light culling for the light loops (the forward shaders', the deferred local-light pass's and the deferred
// translucents'), laid out as Unity URP's Forward+ (Drobot's tiles and z-bins, SIGGRAPH 2017; FORWARD-PLUS.md):
// - the list: the point and spot lights in view, sorted by where their depth range starts, each with one bit; up to
//   256 with 64 KB uniform blocks (the farthest-starting lights go when a view has more), URP's own 32 on OpenGL below;
// - the depth bins: logarithmic from the view's near clip to its far, as many as fit their block (up to 1024); each
//   holds the bits of the lights whose depth range crosses it, and a header naming its first and last light, so the
//   shader visits only the words between; a spot light's range is its cone's (its apex and its cap), not its sphere's;
// - the screen tiles: square, 16 px doubled until every eye's tiles fit their block; each holds the bits of the
//   lights whose sphere (a spot's cone's enclosing sphere) covers it on screen, found row by row with the sphere cut
//   down to the row's nearer boundary plane, as the frustum grid of 2016 did; per eye in stereo, by each eye's own
//   projection.
// The reflection probes in view (ReflectionProbeStage) are in the same list, as Unity's: their bits after the
// lights' from the next whole word, a bin's second header naming the first and last of them, sorted by importance
// (then the smallest first) so a fragment blends them in that order; their data rides in the parameters block
// (ReflectionProbe.slh).
// Built on the CPU every frame (no depth buffer needed, so both render methods build it before they draw); the
// shaders' side is LightClusterGrid.slh, the layout they share LightClusterGrid_shared.slh.
class LightClusters {
public:
    using LightID = LightStage::Index;

    // the C++ / GLSL shared layout of what the shaders read
#include "LightClusterGrid_shared.slh"

    struct Stats {
        int numInputLights { 0 };   // the frame's point and spot lights
        int numLights { 0 };        // in the list: in view, within the cap
        int numInputProbes { 0 };   // the scene's reflection probes with a cube map
        int numProbes { 0 };        // in the list: in view, within the cap
        int numTileBits { 0 };      // light and probe bits set over every tile
        int tileSize { 0 };         // pixels
        int tileCountX { 0 };
        int tileCountY { 0 };
        int binCount { 0 };
        int wordsPerEntry { 0 };    // 32-bit words a tile or bin holds
    };

    LightClusters();

    void updateFrustum(const ViewFrustum& frustum) { _frustum = frustum; }
    void updateLightStage(const LightStagePointer& lightStage) { _lightStage = lightStage; }
    // the frame's point and spot lights (each kind as the lighting model allows it): the candidates
    void updateLightFrame(const LightStage::FramePointer& lightFrame, bool points = true, bool spots = true);
    // the scene's reflection probes (those with a layer in the stage's cube map array): the candidates
    void updateReflectionProbes(const ReflectionProbeStagePointer& probeStage) { _probeStage = probeStage; }
    // builds the list, the bins and the tiles for the view of args (its viewport, and its two eyes in stereo)
    Stats updateClusters(const RenderArgs* args);

    int getNumLights() const { return _numLights; }
    int getNumProbes() const { return _numProbes; }
    // the probes' cube map array for the shaders (one black layer when the scene has none)
    const gpu::TexturePointer& getReflectionProbeArray() const;
    // the lights a frame's list can hold: 256 with 64 KB uniform blocks, URP's own 32 on OpenGL below that
    static int getMaxLights();

    ViewFrustum _frustum;
    LightStagePointer _lightStage;
    LightStage::LightIndices _visibleLightIndices;
    ReflectionProbeStagePointer _probeStage;

    // what the shaders bind: the parameters and the list, the tiles' words, the bins' words
    gpu::StructBuffer<LightClusterParams> _paramsBuffer;
    gpu::BufferView _tilesBuffer;
    gpu::BufferView _binsBuffer;

private:
    struct Candidate {
        LightID id { LightStage::INVALID_INDEX };
        // a reflection probe's: the probe and its layer in the array (null and -1 for a light)
        graphics::ReflectionProbePointer probe;
        int layer { -1 };
        float minZ { 0.0f };          // eye depth range
        float maxZ { 0.0f };
        glm::vec3 center;             // world: the sphere the tiles are scanned with
        float radius { 0.0f };
        glm::vec3 eyeCenter[2];       // the sphere's centre in each eye's space
        glm::vec4 ndcBounds[2];       // its screen bounds in each eye { x min, x max, y min, y max }
        bool visible[2] { false, false };
    };

    int _numLights { 0 };
    int _numProbes { 0 };
    std::vector<Candidate> _candidates;
    std::vector<Candidate> _probeCandidates;
    std::vector<uint32_t> _tiles;
    std::vector<uint32_t> _bins;
    std::vector<glm::vec4> _columnPlanes;
    std::vector<glm::vec4> _rowPlanes;
};

using LightClustersPointer = std::shared_ptr<LightClusters>;



class LightClusteringPassConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(bool freeze MEMBER freeze NOTIFY dirty)

    Q_PROPERTY(int numInputLights MEMBER numInputLights NOTIFY dirty)
    Q_PROPERTY(int numClusteredLights MEMBER numClusteredLights NOTIFY dirty)
    Q_PROPERTY(int numClusteredLightReferences MEMBER numClusteredLightReferences NOTIFY dirty)
    Q_PROPERTY(int numInputProbes MEMBER numInputProbes NOTIFY dirty)
    Q_PROPERTY(int numClusteredProbes MEMBER numClusteredProbes NOTIFY dirty)
    Q_PROPERTY(int maxLights MEMBER maxLights NOTIFY dirty)
    Q_PROPERTY(int tileSize MEMBER tileSize NOTIFY dirty)
    Q_PROPERTY(int tileCountX MEMBER tileCountX NOTIFY dirty)
    Q_PROPERTY(int tileCountY MEMBER tileCountY NOTIFY dirty)
    Q_PROPERTY(int binCount MEMBER binCount NOTIFY dirty)
    Q_PROPERTY(int wordsPerEntry MEMBER wordsPerEntry NOTIFY dirty)

    Q_PROPERTY(int numSceneLights MEMBER numSceneLights NOTIFY dirty)
    Q_PROPERTY(int numFreeSceneLights MEMBER numFreeSceneLights NOTIFY dirty)
    Q_PROPERTY(int numAllocatedSceneLights MEMBER numAllocatedSceneLights NOTIFY dirty)

public:
    LightClusteringPassConfig() : render::Job::Config(true){}

    // keeps the last build (its lights, tiles and bins) while the view moves on
    bool freeze{ false };

    // the frame's point and spot lights; those in the list; the light bits set over every tile
    int numInputLights { 0 };
    int numClusteredLights { 0 };
    int numClusteredLightReferences { 0 };
    // the scene's reflection probes with a cube map; those in the list
    int numInputProbes { 0 };
    int numClusteredProbes { 0 };
    // the layout
    int maxLights { 0 };
    int tileSize { 0 };
    int tileCountX { 0 };
    int tileCountY { 0 };
    int binCount { 0 };
    int wordsPerEntry { 0 };

    int numSceneLights { 0 };
    int numFreeSceneLights { 0 };
    int numAllocatedSceneLights { 0 };
signals:
    void dirty();
    
protected:
};

class LightClusteringPass {
public:
    using Input = render::VaryingSet4<DeferredFrameTransformPointer, LightingModelPointer, LightStage::FramePointer, LinearDepthFramebufferPointer>;
    using Output = LightClustersPointer;
    using Config = LightClusteringPassConfig;
    using JobModel = render::Job::ModelIO<LightClusteringPass, Input, Output, Config>;
    
    LightClusteringPass();
    
    void configure(const Config& config);
    
    void run(const render::RenderContextPointer& renderContext, const Input& input, Output& output);
    
protected:
    LightClustersPointer _lightClusters;
    bool _freeze { false };
};



// Two views of the culling over a deferred frame, like Unity's Lighting Complexity view: the lights each fragment's
// tile and bin hand it (with the tiles' edges), and the lights whose volume holds it after that
class DebugLightClustersConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(bool doDrawClusterFromDepth MEMBER doDrawClusterFromDepth NOTIFY dirty)
    Q_PROPERTY(bool doDrawContent MEMBER doDrawContent NOTIFY dirty)
public:
    DebugLightClustersConfig() : render::Job::Config(false){}

    bool doDrawClusterFromDepth { false };
    bool doDrawContent { false };

signals:
    void dirty();

protected:
};


#include "DeferredFramebuffer.h"

class DebugLightClusters {
public:
    using Inputs = render::VaryingSet4 < DeferredFrameTransformPointer, LightingModelPointer, LinearDepthFramebufferPointer, LightClustersPointer>;
    using Config = DebugLightClustersConfig;
    using JobModel = render::Job::ModelI<DebugLightClusters, Inputs, Config>;

    DebugLightClusters();

    void configure(const Config& config);

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    gpu::PipelinePointer _drawClusterFromDepth;
    gpu::PipelinePointer _drawClusterContent;
    const gpu::PipelinePointer getDrawClusterFromDepthPipeline();
    const gpu::PipelinePointer getDrawClusterContentPipeline();
    bool doDrawClusterFromDepth { false };
    bool doDrawContent { false };
};

#endif
