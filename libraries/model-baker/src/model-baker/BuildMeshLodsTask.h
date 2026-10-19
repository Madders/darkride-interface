//
//  BuildMeshLodsTask.h
//  model-baker/src/model-baker
//
//  Automatic LODs (AUTOMATIC-LODS.md 3.1): simplified versions of each mesh, made with meshoptimizer when the model
//  loads and added to the graphics meshes BuildGraphicsMeshTask built, as index ranges over the same vertices in an
//  index buffer of their own (graphics::Mesh::setLods). Off unless the loader turns it on: the client's model cache
//  does, while the offline baker's output has no place for them.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_BuildMeshLodsTask_h
#define hifi_BuildMeshLodsTask_h

#include <hfm/HFM.h>
#include <shared/HifiTypes.h>

#include "Engine.h"
#include "BakerTypes.h"

class BuildMeshLodsConfig : public baker::JobConfig {
    Q_OBJECT
public:
    BuildMeshLodsConfig() : baker::JobConfig(false) {}
};

class BuildMeshLodsTask {
public:
    using Config = BuildMeshLodsConfig;
    using Input = baker::VaryingSet4<std::vector<hfm::Mesh>, std::vector<graphics::MeshPointer>, hifi::URL, baker::NormalsPerMesh>;
    using JobModel = baker::Job::ModelI<BuildMeshLodsTask, Input, Config>;

    void configure(const Config& config) {}
    // the graphics meshes are changed in place: they are shared pointers the later jobs read
    void run(const baker::BakeContextPointer& context, const Input& input);
};

#endif // hifi_BuildMeshLodsTask_h
