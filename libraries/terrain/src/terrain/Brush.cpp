//
//  Brush.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Brush.h"

#include <glm/gtx/norm.hpp>

#include <cmath>
#include <vector>

using namespace terrain;

namespace {

// How many of a chunk's voxels a brush's radius should span at least: a chunk whose voxels are coarser than that
// under the brush is split first, so a small brush carves finely and a big one stays coarse.
const float BRUSH_VOXELS_WANTED = 4.0f;

// Raise and Lower mend the distances under the brush from this many voxels off the surface outward (the mesher reads
// nearer ones), where a voxel falls short of its nearest neighbour's distance plus one by more than this slack.
const float MEND_BEYOND = 2.0f;
const float MEND_SLACK = 0.7f;

// Smooth blurs the field with a box reaching this much of the brush's radius each way, at least a voxel, so a wide
// brush levels hills rather than polishing voxel-sized dents.
const float SMOOTH_REACH_FRACTION = 0.25f;

// The PCG hash (Jarzynski and Olano, "Hash Functions for GPU Rendering", 2020).
uint32_t pcgHash(uint32_t value) {
    const uint32_t state = value * 747796405u + 2891336453u;
    const uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// A number in [0, 1) for a voxel of a level and a seed, the hash nested over them as the paper does for more than
// one dimension.
float voxelChance(const glm::ivec3& point, int level, uint32_t seed) {
    uint32_t hash = pcgHash(seed);
    hash = pcgHash((uint32_t)level + hash);
    hash = pcgHash((uint32_t)point.z + hash);
    hash = pcgHash((uint32_t)point.y + hash);
    hash = pcgHash((uint32_t)point.x + hash);
    return (float)(hash >> 8) * (1.0f / 16777216.0f);
}

// Replaces every value of the block with the mean of the values within reach of it along each axis (a box, cut off
// at the block's edges): a running sum along each axis in turn.
void boxBlur(std::vector<float>& values, const glm::ivec3& size, int reach) {
    const glm::ivec3 stride(1, size.x, size.x * size.y);
    std::vector<float> sums;
    for (int axis = 0; axis < 3; ++axis) {
        const int n = size[axis];
        const int step = stride[axis];
        const int a = (axis + 1) % 3;
        const int b = (axis + 2) % 3;
        sums.resize((size_t)n + 1);
        for (int j = 0; j < size[b]; ++j) {
            for (int i = 0; i < size[a]; ++i) {
                float* line = &values[(size_t)i * stride[a] + (size_t)j * stride[b]];
                sums[0] = 0.0f;
                for (int t = 0; t < n; ++t) {
                    sums[t + 1] = sums[t] + line[(size_t)t * step];
                }
                for (int t = 0; t < n; ++t) {
                    const int low = std::max(0, t - reach);
                    const int high = std::min(n - 1, t + reach);
                    line[(size_t)t * step] = (sums[high + 1] - sums[low]) / (float)(high - low + 1);
                }
            }
        }
    }
}

} // namespace

namespace {

// 1 up to the flat part of the radius, falling smoothly to 0 at the radius.
float falloff(float distance, float radius, float hardness) {
    if (distance >= radius) {
        return 0.0f;
    }
    const float flat = radius * glm::clamp(hardness, 0.0f, 1.0f);
    if (distance <= flat) {
        return 1.0f;
    }
    const float t = (distance - flat) / (radius - flat);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

} // namespace

float Brush::weightAt(const glm::vec3& point) const {
    if (radius <= 0.0f) {
        return 0.0f;
    }
    const glm::vec3 offset = point - center;
    if (mask && !mask->isEmpty()) {
        const float across = mask->weightAt(glm::vec2(offset.x, offset.z) / radius, angle, hardness);
        return across <= 0.0f ? 0.0f : across * falloff(std::abs(offset.y), radius, hardness);
    }
    return falloff(glm::length(offset), radius, hardness);
}

glm::vec3 Brush::reach() const {
    if (mask && !mask->isEmpty()) {
        const float across = radius * BrushMask::reach(angle);
        return glm::vec3(across, radius, across);
    }
    return glm::vec3(radius);
}

int terrain::brushLevel(const VoxelGrid& grid, const Brush& brush) {
    int level = 0;
    while (level < grid.getMaxLevel() && brush.radius * (float)(1 << level) < BRUSH_VOXELS_WANTED) {
        ++level;
    }
    return level;
}

bool terrain::brushBounds(const VoxelGrid& grid, const Brush& brush, glm::ivec3& low, glm::ivec3& high) {
    if (grid.isEmpty() || brush.radius <= 0.0f) {
        return false;
    }
    // the grid voxels the brush can touch, a voxel to spare: Dig and Fill reach a voxel past the radius, and finer
    // chunks have voxels between the grid's
    const glm::vec3 reach = brush.reach();
    low = glm::max(glm::ivec3(glm::floor(brush.center - reach)) - 1, glm::ivec3(0));
    high = glm::min(glm::ivec3(glm::ceil(brush.center + reach)) + 1, grid.getVoxelCount() - 1);
    return glm::all(glm::lessThanEqual(low, high));
}

int terrain::sculpt(VoxelGrid& grid, const Brush& brush, SculptMode mode) {
    glm::ivec3 low, high;
    if (mode == SculptMode::Paint || !brushBounds(grid, brush, low, high)) {
        return 0;
    }
    // Dig and Fill set the field to the distance to the sphere's wall, which the ground just outside the sphere
    // needs too, so they reach one voxel further and use no falloff.
    const bool shaping = mode == SculptMode::Dig || mode == SculptMode::Fill;
    const int reach = shaping ? 1 : 0;
    const float strength = glm::clamp(brush.strength, 0.0f, 1.0f);

    // The chunks under the brush, split first where their voxels are too coarse for it; each is worked in its own
    // voxels, the brush scaled to them.
    grid.refine(low, high, brushLevel(grid, brush));
    const std::vector<ChunkKey> keys = grid.chunksCovering(low, high);

    struct Region {
        ChunkKey key;
        Chunk* chunk { nullptr };
        glm::ivec3 low, high;          // the chunk's voxels under the brush, in the level's points
        glm::ivec3 size;
        glm::ivec3 beforeLow, beforeSize;
        std::vector<float> before;     // Smooth: the distances around, before any voxel changes, blurred
        std::vector<uint8_t> saturated;   // Raise and Lower: which voxels were at the byte's limit before the pass
        glm::ivec3 changedLow { CHUNK_SIZE }, changedHigh { -1 };

        size_t indexOf(const glm::ivec3& point) const {
            const glm::ivec3 i = point - low;
            return (size_t)(i.z * size.y + i.y) * size.x + i.x;
        }
        float beforeAt(const glm::ivec3& point) const {
            const glm::ivec3 i = point - beforeLow;
            return before[(i.z * beforeSize.y + i.y) * beforeSize.x + i.x];
        }
        // the voxel at a point of the chunk's level: the chunk's own, or the grid's reading beyond it
        Voxel voxelAt(const VoxelGrid& grid, const glm::ivec3& point) const {
            const glm::ivec3 local = point - key.coords * CHUNK_SIZE;
            if (glm::all(glm::greaterThanEqual(local, glm::ivec3(0))) && glm::all(glm::lessThan(local, glm::ivec3(CHUNK_SIZE)))) {
                return chunk->get(local.x, local.y, local.z);
            }
            return grid.sampleAt(key.level, point);
        }
        float distanceAt(const VoxelGrid& grid, const glm::ivec3& point) const {
            return voxelAt(grid, point).getDistance();
        }
        // The material of the ground a voxel grows out of: its neighbour's deepest in the ground, or the given one
        // when none of them is ground.
        uint8_t groundMaterialNear(const VoxelGrid& grid, const glm::ivec3& point, uint8_t fallback) const {
            uint8_t material = fallback;
            float deepest = 0.0f;
            for (int axis = 0; axis < 3; ++axis) {
                glm::ivec3 step(0);
                step[axis] = 1;
                for (const glm::ivec3& neighbour : { point - step, point + step }) {
                    const Voxel voxel = voxelAt(grid, neighbour);
                    if (voxel.isGround() && voxel.getDistance() < deepest) {
                        deepest = voxel.getDistance();
                        material = voxel.material;
                    }
                }
            }
            return material;
        }
    };
    std::vector<Region> regions;
    for (const ChunkKey& key : keys) {
        const float scale = (float)key.getScale();
        const glm::vec3 center = brush.center * scale;
        const float radius = brush.radius * scale;
        const glm::vec3 extent = brush.reach() * scale;
        const glm::ivec3 origin = key.coords * CHUNK_SIZE;
        Region region;
        region.key = key;
        region.chunk = grid.findChunk(key);
        region.low = glm::max(glm::ivec3(glm::floor(center - extent)) - reach, glm::max(origin, glm::ivec3(0)));
        region.high = glm::min(glm::ivec3(glm::ceil(center + extent)) + reach,
                               glm::min(origin + CHUNK_SIZE - 1, (grid.getVoxelCount() << key.level) - 1));
        if (!region.chunk || glm::any(glm::greaterThan(region.low, region.high))) {
            continue;
        }
        region.size = region.high - region.low + 1;
        if (mode == SculptMode::Smooth) {
            // the field around the region before any voxel changes, blurred as wide as the brush asks
            const int blurReach = std::max(1, (int)std::lround(SMOOTH_REACH_FRACTION * radius));
            region.beforeLow = region.low - blurReach;
            region.beforeSize = region.size + 2 * blurReach;
            region.before.resize((size_t)region.beforeSize.x * region.beforeSize.y * region.beforeSize.z);
            for (int z = 0; z < region.beforeSize.z; ++z) {
                for (int y = 0; y < region.beforeSize.y; ++y) {
                    for (int x = 0; x < region.beforeSize.x; ++x) {
                        region.before[(z * region.beforeSize.y + y) * region.beforeSize.x + x] =
                            grid.sampleAt(key.level, region.beforeLow + glm::ivec3(x, y, z)).getDistance();
                    }
                }
            }
            boxBlur(region.before, region.beforeSize, blurReach);
        }
        regions.push_back(std::move(region));
    }

    // Raise and Lower shift the field, which a byte can follow only within its band of a few voxels: a voxel at the
    // byte's limit says only "far on this side", so shifting it would put it a few voxels short of the limit, a
    // distance it never had. So they shift by at most a voxel of the finest chunk's at a time, leave the voxels at
    // the limit as they are and, after each pass, give those a distance read off their neighbours, every chunk
    // shifted before any is read. (Shifting them and reading them back meanwhile is what a chunk at a finer level
    // next to a coarser one did: the coarser chunk read the finer one's shifted limit in its own voxels, a short
    // distance, and its air above the ground was given that distance all the way up; the next passes then moved
    // the ground up through it at once, a wall as tall as the brush along the chunks' join.)
    const bool shifting = mode == SculptMode::Raise || mode == SculptMode::Lower;
    int finestScale = 1;
    for (const Region& region : regions) {
        finestScale = std::max(finestScale, region.key.getScale());
    }
    const int passes = shifting ? std::max(1, (int)std::ceil(strength * (float)finestScale - 1.0e-4f)) : 1;
    int changed = 0;
    auto record = [&](Region& region, const glm::ivec3& local) {
        ++changed;
        region.changedLow = glm::min(region.changedLow, local);
        region.changedHigh = glm::max(region.changedHigh, local);
    };
    for (int pass = 0; pass < passes; ++pass) {
        for (Region& region : regions) {
            Chunk* chunk = region.chunk;
            const float scale = (float)region.key.getScale();
            const glm::vec3 center = brush.center * scale;
            const float radius = brush.radius * scale;
            const glm::ivec3 origin = region.key.coords * CHUNK_SIZE;
            const float shift = shifting ? strength * scale / (float)passes : 0.0f;   // grid voxels of strength, in the chunk's
            Brush scaled = brush;   // the falloff in the chunk's voxels
            scaled.center = center;
            scaled.radius = radius;
            if (shifting) {
                region.saturated.assign((size_t)region.size.x * region.size.y * region.size.z, 0);
            }
            for (int z = region.low.z; z <= region.high.z; ++z) {
                for (int y = region.low.y; y <= region.high.y; ++y) {
                    for (int x = region.low.x; x <= region.high.x; ++x) {
                        const glm::ivec3 point(x, y, z);
                        const glm::vec3 p(point);
                        const float weight = shaping ? 1.0f : scaled.weightAt(p);
                        if (weight <= 0.0f) {
                            continue;
                        }
                        const glm::ivec3 local = point - origin;
                        const Voxel old = chunk->get(local.x, local.y, local.z);
                        if (shifting && std::abs((int)old.distance) == DISTANCE_MAX) {
                            region.saturated[region.indexOf(point)] = 1;   // the sweeps below give it its distance
                            continue;
                        }
                        const float distance = old.getDistance();
                        Voxel next = old;
                        switch (mode) {
                            case SculptMode::Raise:
                                next.setDistance(distance - shift * weight);
                                break;
                            case SculptMode::Lower:
                                next.setDistance(distance + shift * weight);
                                break;
                            case SculptMode::Dig: {
                                // Carve the sphere out: the field becomes the larger of itself and the sphere's inside.
                                const float sphere = radius - glm::length(p - center);
                                next.setDistance(glm::mix(distance, glm::max(distance, sphere), strength));
                                break;
                            }
                            case SculptMode::Fill: {
                                // Add the sphere: the field becomes the smaller of itself and the sphere's outside.
                                const float sphere = glm::length(p - center) - radius;
                                next.setDistance(glm::mix(distance, glm::min(distance, sphere), strength));
                                break;
                            }
                            case SculptMode::Flatten:
                                next.setDistance(glm::mix(distance, (float)y - brush.flattenHeight * scale, strength * weight));
                                break;
                            case SculptMode::Smooth:
                                // toward the blurred field: the mean of the box around the voxel before the pass
                                next.setDistance(glm::mix(distance, region.beforeAt(point), strength * weight));
                                break;
                            case SculptMode::PaintVoxels:
                                // one material a voxel: a share of the ground voxels, as much as the paint map's coverage
                                // would rise by, each by chance
                                if (old.isGround() && voxelChance(point, region.key.level, brush.seed) < strength * weight) {
                                    next.material = brush.material;
                                }
                                break;
                            case SculptMode::Paint:
                                break;
                        }
                        if (next.isGround() && !old.isGround()) {
                            // New ground. Raise grows the ground that is there, in its own material (the chosen layer's
                            // left voxel-sized patches of it along the rim of a raise, where the soft edge brought
                            // single voxels over: 2026-10-18); Fill adds the chosen material.
                            if (mode == SculptMode::Raise) {
                                next.material = region.groundMaterialNear(grid, point, brush.material);
                            } else if (mode == SculptMode::Fill) {
                                next.material = brush.material;
                            }
                        }
                        if (next != old && chunk->set(local.x, local.y, local.z, next)) {
                            record(region, local);
                        }
                    }
                }
            }
        }
        if (!shifting) {
            break;
        }
        // The voxels at the limit: each is a voxel further from the surface than its nearest neighbour, on its own
        // side. Two sweeps, one each way, carry the distances outward through them; one still at the limit when a
        // neighbour reads it is read as far, whatever the neighbour's voxel size. The sweeps also mend the field
        // under the brush beyond the band the mesher reads: a voxel clearly nearer the surface than its nearest
        // neighbour less a voxel cannot be (a true distance field never falls short of that by more than the
        // sweep's own overestimate on a slope, 1 - 1/sqrt(3)), so it gets the neighbour's distance plus one. Fields
        // sculpted before the limit voxels were left unshifted have such air above the ground along the joins of
        // chunk levels, and shifting it unmended put the ground up through it in a few passes.
        for (int sweep = 0; sweep < 2; ++sweep) {
            const bool forward = sweep == 0;
            for (Region& region : regions) {
                Chunk* chunk = region.chunk;
                const glm::ivec3 origin = region.key.coords * CHUNK_SIZE;
                const float scale = (float)region.key.getScale();
                Brush scaled = brush;   // the brush in the chunk's voxels: what it reaches is mended
                scaled.center = brush.center * scale;
                scaled.radius = brush.radius * scale;
                for (int k = 0; k < region.size.z; ++k) {
                    const int z = forward ? region.low.z + k : region.high.z - k;
                    for (int j = 0; j < region.size.y; ++j) {
                        const int y = forward ? region.low.y + j : region.high.y - j;
                        for (int i = 0; i < region.size.x; ++i) {
                            const int x = forward ? region.low.x + i : region.high.x - i;
                            const glm::ivec3 point(x, y, z);
                            const glm::ivec3 local = point - origin;
                            const Voxel old = chunk->get(local.x, local.y, local.z);
                            const bool atLimit = region.saturated[region.indexOf(point)] != 0;
                            if (!atLimit && (std::abs(old.getDistance()) < MEND_BEYOND || scaled.weightAt(glm::vec3(point)) <= 0.0f)) {
                                continue;
                            }
                            float nearest = SATURATED_DISTANCE;
                            bool surface = false;
                            for (int axis = 0; axis < 3 && !surface; ++axis) {
                                glm::ivec3 step(0);
                                step[axis] = 1;
                                for (const glm::ivec3& neighbour : { point - step, point + step }) {
                                    const float d = region.distanceAt(grid, neighbour);
                                    if ((d < 0.0f) != old.isGround()) {
                                        surface = true;   // the surface runs between them: the voxel's own value stands
                                        break;
                                    }
                                    nearest = std::min(nearest, std::abs(d));
                                }
                            }
                            if (surface || (!atLimit && std::abs(old.getDistance()) + MEND_SLACK >= nearest + 1.0f)) {
                                continue;
                            }
                            Voxel next = old;
                            next.setDistance((old.isGround() ? -1.0f : 1.0f) * std::min(nearest + 1.0f, SATURATED_DISTANCE));
                            if (next != old && chunk->set(local.x, local.y, local.z, next)) {
                                record(region, local);
                            }
                        }
                    }
                }
            }
        }
    }
    for (const Region& region : regions) {
        if (region.changedHigh.x >= 0) {
            grid.voxelsChanged(region.key, region.changedLow, region.changedHigh);
        }
    }
    return changed;
}
