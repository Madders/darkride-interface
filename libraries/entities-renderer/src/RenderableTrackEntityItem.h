//
//  RenderableTrackEntityItem.h
//  libraries/entities-renderer/src
//
//  A Track entity on the client: its guideway (the monorail's beam and pylons, TrackGuideway) drawn through the
//  engine's material pipeline in the entity's colour, with Material entities able to skin it, and the same mesh as
//  its collision, a static mesh like the terrain's, so a visitor can stand on a beam. The mesh is rebuilt on the main
//  thread whenever the curve or the guideway's settings change; the render item then takes the new buffers.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderableTrackEntityItem_h
#define hifi_RenderableTrackEntityItem_h

#include <TrackEntityItem.h>
#include <TrackGuideway.h>
#include <graphics/Material.h>

#include "RenderableEntityItem.h"

namespace render { namespace entities {
class TrackEntityRenderer;
} }

class RenderableTrackEntityItem : public TrackEntityItem {
    friend class render::entities::TrackEntityRenderer;
public:
    static EntityItemPointer factory(const EntityItemID& entityID, const EntityItemProperties& properties);
    RenderableTrackEntityItem(const EntityItemID& entityItemID);

    // the physics: the guideway as a static mesh, none while the track draws nothing
    ShapeType getShapeType() const override;
    bool shouldBePhysical() const override;
    bool isReadyToComputeShape() const override;
    void computeShapeInfo(ShapeInfo& info) override;
    void setUnscaledDimensions(const glm::vec3& value) override;
    void setRegistrationPoint(const glm::vec3& value) override;

    // picking: the guideway's own triangles, not the entity's box (which spans the whole loop)
    bool supportsDetailedIntersection() const override { return true; }
    bool findDetailedRayIntersection(const glm::vec3& origin, const glm::vec3& direction, OctreeElementPointer& element,
                                     float& distance, BoxFace& face, glm::vec3& surfaceNormal, QVariantMap& extraInfo,
                                     bool precisionPicking) const override;

    // The mesh is out of date with the curve, the guideway's settings or the entity's box (the collision shape is in
    // box-centred space, so the box's registration offset is part of it).
    bool guidewayIsStale() const;
    // Rebuilds the mesh and the collision shape from the entity's properties; main thread.
    void rebuildGuideway();
    const TrackGuidewayMesh& getGuidewayMesh() const { return _mesh; }
    uint64_t getGuidewayMeshVersion() const { return _meshVersion; }
    TrackGuidewaySpec guidewaySpec() const;

private:
    glm::vec3 registrationOffset() const;   // from the entity's origin (its registration point) to its box's centre

    TrackGuidewayMesh _mesh;                // main thread's; the renderer copies it into GPU buffers
    // the mesh's triangles in runs of PICK_CHUNK_TRIANGLES, a box each, so a ray tests the boxes and only the runs it
    // meets (a mouse pick can run every frame; the mesh is up to some forty thousand triangles)
    std::vector<AABox> _pickChunks;
    uint64_t _meshVersion { 0 };
    uint32_t _builtTrackVersion { 0 };
    uint32_t _builtGuidewayVersion { 0 };
    glm::vec3 _builtOffset { 0.0f };
    bool _built { false };

    ShapeInfo _shapeInfo;
    bool _shapeReady { false };
    uint64_t _shapeSerial { 0 };
};

namespace render { namespace entities {

class TrackEntityRenderer : public TypedEntityRenderer<RenderableTrackEntityItem> {
    using Parent = TypedEntityRenderer<RenderableTrackEntityItem>;
    friend class EntityRenderer;
public:
    TrackEntityRenderer(const EntityItemPointer& entity);
    ~TrackEntityRenderer();

protected:
    ShapeKey getShapeKey() override;
    Item::Bound getBound() override;
    bool isTransparent() const override;

private:
    bool needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const override;
    void doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) override;
    void doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) override;
    void doRender(RenderArgs* args) override;

    static const gpu::Stream::FormatPointer& vertexFormat();

    // the entity's colour as the base material; Material entities layer over it through _materials["0"]
    std::shared_ptr<graphics::Material> _material { std::make_shared<graphics::Material>() };
    glm::vec3 _color { NAN };

    uint64_t _meshVersion { 0 };
    gpu::BufferPointer _vertexBuffer;
    gpu::BufferPointer _indexBuffer;
    uint32_t _indexCount { 0 };
    AABox _localBound;            // the mesh's, in the track's own space
    Transform _trackTransform;    // the track's own space in the world: its position and rotation, no scale
    Item::Bound _trackBound;      // _localBound placed by _trackTransform
};

} }

#endif // hifi_RenderableTrackEntityItem_h
