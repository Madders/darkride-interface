//
//  MeshPartPayload.h
//  interface/src/renderer
//
//  Created by Sam Gateau on 10/3/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MeshPartPayload_h
#define hifi_MeshPartPayload_h

#include <atomic>

#include <Interpolate.h>

#include <gpu/Batch.h>

#include <render/Scene.h>

#include <graphics/Geometry.h>

#include "Model.h"

class ImpostorSwitch;
class Model;

class MeshPartPayload {
public:
    MeshPartPayload() = default;
    MeshPartPayload(const std::shared_ptr<const graphics::Mesh>& mesh,
                    int partIndex,
                    graphics::MaterialPointer material,
                    const uint64_t& created);
    virtual ~MeshPartPayload() = default;

    typedef render::Payload<MeshPartPayload> Payload;
    typedef Payload::DataPointer Pointer;

    virtual void updateKey(const render::ItemKey& key);

    virtual void updateMeshPart(const std::shared_ptr<const graphics::Mesh>& drawMesh, int partIndex);

    virtual void notifyLocationChanged() {}
    void updateTransform(const Transform& transform);
    void updateTransformAndBound(const Transform& transform);

    // Render Item interface
    virtual render::ItemKey getKey() const;
    virtual render::Item::Bound getBound() const;
    virtual render::ShapeKey getShapeKey() const;
    virtual void render(RenderArgs* args);

    // ModelMeshPartPayload functions to perform render
    void drawCall(gpu::Batch& batch) const;
    virtual void bindMesh(gpu::Batch& batch);
    virtual void bindTransform(gpu::Batch& batch, RenderArgs::RenderMode renderMode) const;

    // Payload resource cached values
    Transform _worldFromLocalTransform;
    int _partIndex = 0;
    bool _hasColorAttrib{ false };

    graphics::Box _localBound;
    mutable graphics::Box _worldBound;
    std::shared_ptr<const graphics::Mesh> _drawMesh;

    graphics::MultiMaterial _drawMaterials;
    graphics::Mesh::Part _drawPart;

    size_t getVerticesCount() const { return _drawMesh ? _drawMesh->getNumVertices() : 0; }
    size_t getMaterialTextureSize() { return _drawMaterials.getTextureSize(); }
    int getMaterialTextureCount() { return _drawMaterials.getTextureCount(); }
    bool hasTextureInfo() const { return _drawMaterials.hasTextureInfo(); }

    void addMaterial(graphics::MaterialLayer material);
    void removeMaterial(graphics::MaterialPointer material);

    void setCullWithParent(bool value) { _cullWithParent = value; }

    static bool DEFAULT_ENABLE_MATERIAL_PROCEDURAL_SHADERS;
    static bool enableMaterialProceduralShaders; // set from menu/settings
    static bool sceneIsReady; // set from entity tree renderer

protected:
    render::ItemKey _itemKey{ render::ItemKey::Builder::opaqueShape().build() };
    bool _cullWithParent{ false };
    uint64_t _created;
};

namespace render {
template <>
const ItemKey payloadGetKey(const MeshPartPayload::Pointer& payload);
template <>
const Item::Bound payloadGetBound(const MeshPartPayload::Pointer& payload);
template <>
const ShapeKey shapeGetShapeKey(const MeshPartPayload::Pointer& payload);
template <>
void payloadRender(const MeshPartPayload::Pointer& payload, RenderArgs* args);
}  // namespace render

class ModelMeshPartPayload : public MeshPartPayload {
public:
    ModelMeshPartPayload(ModelPointer model,
                         int meshIndex,
                         int partIndex,
                         int shapeIndex,
                         const Transform& transform,
                         const Transform& offsetTransform,
                         const uint64_t& created);

    typedef render::Payload<ModelMeshPartPayload> Payload;
    typedef Payload::DataPointer Pointer;

    void notifyLocationChanged() override;

    void updateKey(const render::ItemKey& key) override;

    // matrix palette skinning
    void updateClusterBuffer(const std::vector<glm::mat4>& clusterMatrices);

    // dual quaternion skinning
    void updateClusterBuffer(const std::vector<Model::TransformDualQuaternion>& clusterDualQuaternions);

    // Render Item interface
    render::ShapeKey getShapeKey() const override;
    void render(RenderArgs* args) override;

    void setShapeKey(bool invalidateShapeKey, PrimitiveMode primitiveMode, bool useDualQuaternionSkinning);
    void setCauterized(bool cauterized) { _cauterized = cauterized; }
    // the model's impostor switch (Model::setImpostorSwitch): this part draws only in the views where it says the
    // model shows rather than its card
    void setImpostorSwitch(const std::shared_ptr<ImpostorSwitch>& impostorSwitch) { _impostorSwitch = impostorSwitch; }

    // automatic LODs (AUTOMATIC-LODS.md 3.3): the part's triangles in full and in each level of its mesh, and the level
    // the main view draws now, 0 for the full mesh (read from the main thread by LODManager.describeEntity)
    int getFullTriangles() const { return (int)(_drawPart._numIndices / INDICES_PER_TRIANGLE); }
    std::vector<int> getLodTriangles() const;
    int getShownLodLevel() const { return _shownLodLevel.load(std::memory_order_relaxed); }

    // ModelMeshPartPayload functions to perform render
    void bindMesh(gpu::Batch& batch) override;
    void bindTransform(gpu::Batch& batch, RenderArgs::RenderMode renderMode) const override;

    gpu::BufferPointer _clusterBuffer;

    enum class ClusterBufferType
    {
        Matrices,
        DualQuaternions
    };
    ClusterBufferType _clusterBufferType{ ClusterBufferType::Matrices };

    int _meshIndex;
    int _shapeID;
    uint32_t _deformerIndex;

    bool _isSkinned{ false };
    bool _isBlendShaped{ false };
    bool _hasTangents{ false };

    void setBlendshapeBuffer(const std::unordered_map<int, gpu::BufferPointer>& blendshapeBuffers,
                             const QVector<int>& blendedMeshSizes);

private:
    void initCache(const ModelPointer& model);

    gpu::BufferPointer _meshBlendshapeBuffer;
    int _meshNumVertices;
    render::ShapeKey _shapeKey{ render::ShapeKey::Builder::invalid() };
    bool _prevUseDualQuaternionSkinning{ false };
    bool _cauterized{ false };
    std::shared_ptr<ImpostorSwitch> _impostorSwitch;

    static const uint32_t INDICES_PER_TRIANGLE { 3 };
    // the level a view draws: the coarsest whose error looks small enough from its camera; the shadow pass takes the
    // main view's, which keeps a hysteresis band
    int chooseLodLevel(const RenderArgs* args);
    std::vector<graphics::Mesh::Part> _lodParts;   // the part's range in each level of the mesh's LOD index buffer
    std::vector<float> _lodErrors;                 // each level's error, in the mesh's units
    int _mainViewLodLevel { 0 };
    std::atomic<int> _shownLodLevel { 0 };
};

namespace render {
template <>
const ItemKey payloadGetKey(const ModelMeshPartPayload::Pointer& payload);
template <>
const Item::Bound payloadGetBound(const ModelMeshPartPayload::Pointer& payload);
template <>
const ShapeKey shapeGetShapeKey(const ModelMeshPartPayload::Pointer& payload);
template <>
void payloadRender(const ModelMeshPartPayload::Pointer& payload, RenderArgs* args);
}  // namespace render

#endif  // hifi_MeshPartPayload_h
