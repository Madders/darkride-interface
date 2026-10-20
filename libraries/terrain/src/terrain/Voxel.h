//
//  Voxel.h
//  libraries/terrain/src/terrain
//
//  The voxel of a Terrain entity (TERRAIN.md). Each voxel keeps a signed distance to the ground's surface, in voxel
//  units scaled into a byte (negative inside the ground, positive in the air), and the index of the material the
//  ground shows where it is dug open or faces sideways. Voxel (x, y, z) sits at the integer point (x, y, z) of the
//  grid's voxel space; the surface runs between voxels of opposite sign.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_Voxel_h
#define hifi_terrain_Voxel_h

#include <cstdint>
#include <cmath>

namespace terrain {

// Encoded distance units per voxel: a byte spans about four voxels either side of the surface, which is all the
// mesher and the brushes need; further away only the sign matters.
const float DISTANCE_SCALE = 32.0f;
const int8_t DISTANCE_MAX = 127;

inline int8_t encodeDistance(float voxels) {
    float scaled = std::round(voxels * DISTANCE_SCALE);
    if (scaled > DISTANCE_MAX) {
        return DISTANCE_MAX;
    }
    if (scaled < -DISTANCE_MAX) {
        return -DISTANCE_MAX;
    }
    return (int8_t)scaled;
}

inline float decodeDistance(int8_t encoded) {
    return encoded / DISTANCE_SCALE;
}

// A distance at the byte's limit says only "far on this side": it stays at the limit whatever the voxel size.
const float SATURATED_DISTANCE = DISTANCE_MAX / DISTANCE_SCALE;
const float SATURATION_EPSILON = 1.0e-4f;

// A distance read in one voxel size, encoded in another: `scale` old voxels to a new one (2 when the voxels are
// halved, 0.5 when they are doubled). What was at the limit stays there.
inline int8_t rescaleDistance(float voxels, float scale) {
    if (voxels >= SATURATED_DISTANCE - SATURATION_EPSILON) {
        return DISTANCE_MAX;
    }
    if (voxels <= -SATURATED_DISTANCE + SATURATION_EPSILON) {
        return -DISTANCE_MAX;
    }
    return encodeDistance(voxels * scale);
}

struct Voxel {
    int8_t distance { DISTANCE_MAX };   // air, far from any surface
    uint8_t material { 0 };

    Voxel() = default;
    Voxel(int8_t distance, uint8_t material) : distance(distance), material(material) {}

    static Voxel air() { return Voxel(DISTANCE_MAX, 0); }
    static Voxel ground(uint8_t material = 0) { return Voxel(-DISTANCE_MAX, material); }

    float getDistance() const { return decodeDistance(distance); }
    void setDistance(float voxels) { distance = encodeDistance(voxels); }
    bool isGround() const { return distance < 0; }

    bool operator==(const Voxel& other) const { return distance == other.distance && material == other.material; }
    bool operator!=(const Voxel& other) const { return !(*this == other); }
};

} // namespace terrain

#endif // hifi_terrain_Voxel_h
