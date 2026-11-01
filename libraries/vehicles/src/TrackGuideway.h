//
//  TrackGuideway.h
//  libraries/vehicles/src
//
//  The guideway a Track entity draws and collides as (PHYSICS.md 4.8, the monorail): a beam hung under the ride line
//  (the curve the cars follow) with pylons at a spacing down from it to a floor level. One mesh builder, plain maths
//  on glm, so the client's renderer, its collision shape and the physics server's collision shape are all the same
//  surface. The mesh is in the track's own space (the curve's), in metres: position, normal, texture coordinate and
//  tangent per vertex, in the layout the engine's shape pipelines read, and triangles by index.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TrackGuideway_h
#define hifi_TrackGuideway_h

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class TrackCurve;

struct TrackGuidewaySpec {
    enum class Profile : uint8_t {
        None = 0,   // nothing drawn: the builder dresses the track with models of their own
        Beam = 1    // a monorail's box beam under the ride line
    };

    Profile profile { Profile::Beam };
    float width { 0.8f };           // the beam's, across the track, m
    float height { 1.0f };          // the beam's, m
    float clearance { 0.3f };       // from the ride line down to the beam's top, m
    float pylonSpacing { 10.0f };   // between pylons along the track, m; 0 for no pylons
    float pylonFloor { -8.0f };     // the height of the pylons' feet in the track's own space, m: every foot lands
                                    // on this level, so pylons grow where the track climbs; none where the beam
                                    // is already down at it
    float ringSpacing { 0.5f };     // between the rings of the beam along the track, m, about

    static Profile profileFromName(const std::string& name);   // "none", "beam"; anything else is Beam
    static const char* profileName(Profile profile);

    bool operator==(const TrackGuidewaySpec& other) const {
        return profile == other.profile && width == other.width && height == other.height && clearance == other.clearance
            && pylonSpacing == other.pylonSpacing && pylonFloor == other.pylonFloor && ringSpacing == other.ringSpacing;
    }
    bool operator!=(const TrackGuidewaySpec& other) const { return !(*this == other); }
};

struct TrackGuidewayMesh {
    // the layout of GeometryCache::ShapeVertex (position, normal, uv, tangent), so the buffer goes to the GPU as it is
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;        // u along the track in metres, v across the face in metres: a 1 m texture tiles true
        glm::vec3 tangent;   // along the track
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;   // triangles, anticlockwise seen from outside
    glm::vec3 boundMin { 0.0f };
    glm::vec3 boundMax { 0.0f };

    bool empty() const { return indices.empty(); }
    size_t triangleCount() const { return indices.size() / 3; }
    void clear();
};

namespace TrackGuideway {
    // The guideway of a curve in the track's own space; an invalid curve or Profile::None makes an empty mesh.
    void build(const TrackCurve& curve, const TrackGuidewaySpec& spec, TrackGuidewayMesh& mesh);
}

#endif // hifi_TrackGuideway_h
