//
//  ServerTrackEntityItem.h
//  assignment-client/src/physics
//
//  A Track entity on the physics server: its guideway (TrackGuideway, the same mesh the client draws) as a static
//  mesh in the server's world, so what the server simulates (vehicles, dropped things) meets the beam where the
//  clients see it. Rebuilt from the server's step whenever the curve or the guideway's settings change; none while
//  the track draws nothing or is collisionless.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ServerTrackEntityItem_h
#define hifi_ServerTrackEntityItem_h

#include <TrackEntityItem.h>

class ServerTrackEntityItem : public TrackEntityItem {
public:
    static EntityItemPointer factory(const EntityItemID& entityID, const EntityItemProperties& properties);
    ServerTrackEntityItem(const EntityItemID& entityItemID);

    ShapeType getShapeType() const override;
    bool shouldBePhysical() const override;
    bool isReadyToComputeShape() const override;
    void computeShapeInfo(ShapeInfo& info) override;

    // Builds the shape when the curve, the guideway's settings or the box have changed; nothing when it is current.
    // Called from the server's step.
    void update();

    bool hasShape() const { return _shapeReady; }
    size_t getTriangleCount() const { return _triangles; }

private:
    glm::vec3 registrationOffset() const;
    void build();

    uint32_t _builtTrackVersion { 0 };
    uint32_t _builtGuidewayVersion { 0 };
    glm::vec3 _builtOffset { 0.0f };
    bool _built { false };

    ShapeInfo _shapeInfo;
    bool _shapeReady { false };
    uint64_t _shapeSerial { 0 };
    size_t _triangles { 0 };
};

#endif // hifi_ServerTrackEntityItem_h
