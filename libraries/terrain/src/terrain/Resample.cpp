//
//  Resample.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Resample.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

using namespace terrain;

namespace {

// The box of source voxels one target chunk reads: the source points of the chunk's voxels, and the voxel after
// each for the interpolation, clamped into the source.
struct SourceBox {
    glm::ivec3 low { 0 };
    glm::ivec3 size { 0 };
    std::vector<float> distances;
    std::vector<uint8_t> materials;

    size_t indexOf(const glm::ivec3& voxel) const {
        const glm::ivec3 i = voxel - low;
        return ((size_t)i.z * size.y + i.y) * size.x + i.x;
    }
};

// Makes one target chunk: target voxel p reads the source at p * ratio, the same place in the box.
Chunk resampleChunk(const VoxelGrid& source, const glm::ivec3& chunk, const glm::vec3& ratio, float distanceScale) {
    const glm::ivec3 sourceCount = source.getVoxelCount();
    const glm::ivec3 sourceLast = sourceCount - 1;
    const glm::ivec3 origin = chunk * CHUNK_SIZE;
    SourceBox box;
    box.low = glm::clamp(glm::ivec3(glm::floor(glm::vec3(origin) * ratio)), glm::ivec3(0), sourceLast);
    const glm::ivec3 high = glm::clamp(glm::ivec3(glm::floor(glm::vec3(origin + CHUNK_SIZE - 1) * ratio)) + 1, glm::ivec3(0), sourceLast);
    box.size = high - box.low + 1;

    // Source chunks that all hold one value make a target chunk of that value, without a sample taken.
    const std::vector<ChunkKey> covering = source.chunksCovering(box.low, high);
    bool uniform = true;
    bool coarsest = true;   // every source chunk at level 0: the grid's own voxels are the whole field
    Voxel value;
    bool first = true;
    for (const ChunkKey& key : covering) {
        const Chunk& sourceChunk = source.getChunk(key);
        if (key.level > 0) {
            coarsest = false;
        }
        if (!sourceChunk.isUniform()) {
            uniform = false;
        } else if (first) {
            value = sourceChunk.getUniformValue();
            first = false;
        } else if (sourceChunk.getUniformValue() != value) {
            uniform = false;
        }
    }
    if (uniform) {
        return Chunk(Voxel(rescaleDistance(value.getDistance(), distanceScale), value.material));
    }

    std::vector<Voxel> voxels(CHUNK_VOXEL_COUNT);
    const glm::vec3 sourceLimit(sourceLast);
    if (!coarsest) {
        // finer chunks among the source: read the field as the grid interpolates it, at whatever level is there
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const glm::vec3 point = glm::clamp(glm::vec3(origin + glm::ivec3(x, y, z)) * ratio, glm::vec3(0.0f), sourceLimit);
                    const VoxelGrid::FieldSample sample = source.fieldAt(point);
                    voxels[Chunk::indexOf(x, y, z)] = Voxel(rescaleDistance(sample.distance, distanceScale), sample.material);
                }
            }
        }
        Chunk result;
        result.setVoxels(std::move(voxels));
        return result;
    }

    box.distances.resize((size_t)box.size.x * box.size.y * box.size.z);
    box.materials.resize(box.distances.size());
    size_t index = 0;
    for (int z = box.low.z; z <= high.z; ++z) {
        for (int y = box.low.y; y <= high.y; ++y) {
            for (int x = box.low.x; x <= high.x; ++x) {
                const Voxel& voxel = source.get(glm::ivec3(x, y, z));
                box.distances[index] = voxel.getDistance();
                box.materials[index] = voxel.material;
                ++index;
            }
        }
    }

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const glm::vec3 point = glm::clamp(glm::vec3(origin + glm::ivec3(x, y, z)) * ratio, glm::vec3(0.0f), sourceLimit);
                const glm::ivec3 corner = glm::clamp(glm::ivec3(glm::floor(point)), box.low, high);
                const glm::ivec3 next = glm::min(corner + 1, high);
                const glm::vec3 f = glm::clamp(point - glm::vec3(corner), glm::vec3(0.0f), glm::vec3(1.0f));
                const float d000 = box.distances[box.indexOf(glm::ivec3(corner.x, corner.y, corner.z))];
                const float d100 = box.distances[box.indexOf(glm::ivec3(next.x, corner.y, corner.z))];
                const float d010 = box.distances[box.indexOf(glm::ivec3(corner.x, next.y, corner.z))];
                const float d110 = box.distances[box.indexOf(glm::ivec3(next.x, next.y, corner.z))];
                const float d001 = box.distances[box.indexOf(glm::ivec3(corner.x, corner.y, next.z))];
                const float d101 = box.distances[box.indexOf(glm::ivec3(next.x, corner.y, next.z))];
                const float d011 = box.distances[box.indexOf(glm::ivec3(corner.x, next.y, next.z))];
                const float d111 = box.distances[box.indexOf(glm::ivec3(next.x, next.y, next.z))];
                const float d00 = glm::mix(d000, d100, f.x);
                const float d10 = glm::mix(d010, d110, f.x);
                const float d01 = glm::mix(d001, d101, f.x);
                const float d11 = glm::mix(d011, d111, f.x);
                const float distance = glm::mix(glm::mix(d00, d10, f.y), glm::mix(d01, d11, f.y), f.z);
                const glm::ivec3 nearest = glm::clamp(glm::ivec3(glm::round(point)), box.low, high);
                voxels[Chunk::indexOf(x, y, z)] = Voxel(rescaleDistance(distance, distanceScale), box.materials[box.indexOf(nearest)]);
            }
        }
    }
    Chunk result;
    result.setVoxels(std::move(voxels));
    return result;
}

} // namespace

VoxelGrid terrain::resampleGrid(const VoxelGrid& source, const glm::ivec3& targetCount, int threads) {
    if (source.isEmpty() || glm::any(glm::lessThanEqual(targetCount, glm::ivec3(0)))) {
        return VoxelGrid();
    }
    const glm::ivec3 sourceCount = source.getVoxelCount();
    if (targetCount == sourceCount) {
        return source;
    }
    VoxelGrid target(targetCount);
    target.setCaps(source.getCaps());   // the box's closed faces are the terrain's, whatever its voxel count
    // Target voxel p lies where source voxel p * ratio does; a distance of one source voxel is one over the ratio
    // in target voxels, taken as the mean over the axes (a box scaled unevenly has no single ratio).
    const glm::vec3 ratio = glm::vec3(sourceCount) / glm::vec3(targetCount);
    const float distanceScale = (1.0f / ratio.x + 1.0f / ratio.y + 1.0f / ratio.z) / 3.0f;

    const glm::ivec3 chunkCount = target.getChunkCount();
    const int total = chunkCount.x * chunkCount.y * chunkCount.z;
    auto chunkAt = [&](int index) {
        return glm::ivec3(index % chunkCount.x, (index / chunkCount.x) % chunkCount.y, index / (chunkCount.x * chunkCount.y));
    };
    std::vector<Chunk> results(total);
    std::atomic<int> next { 0 };
    auto work = [&] {
        for (int index = next++; index < total; index = next++) {
            results[index] = resampleChunk(source, chunkAt(index), ratio, distanceScale);
        }
    };
    int threadCount = threads > 0 ? threads : std::max(1, (int)std::thread::hardware_concurrency());
    threadCount = std::min(threadCount, total);
    std::vector<std::thread> helpers;
    for (int i = 1; i < threadCount; ++i) {
        helpers.emplace_back(work);
    }
    work();
    for (std::thread& helper : helpers) {
        helper.join();
    }
    for (int index = 0; index < total; ++index) {
        target.setChunk(chunkAt(index), std::move(results[index]));
    }
    return target;
}

PaintMap terrain::resamplePaint(const PaintMap& source, const glm::ivec2& voxelFootprint, int texelsPerVoxel) {
    PaintMap target(voxelFootprint, texelsPerVoxel);
    if (source.isEmpty() || target.isEmpty() || !source.isPainted()) {
        return target;
    }
    const glm::ivec2 sourceSize = source.getSize();
    const glm::ivec2 targetSize = target.getSize();
    const glm::ivec2 sourceLast = sourceSize - 1;
    std::vector<uint8_t> weights((size_t)targetSize.x * targetSize.y * PAINT_LAYERS);
    size_t out = 0;
    for (int j = 0; j < targetSize.y; ++j) {
        // the texel's centre, as a fraction of the box, lands between source texel centres
        const float sy = glm::clamp(((float)j + 0.5f) * sourceSize.y / targetSize.y - 0.5f, 0.0f, (float)sourceLast.y);
        const int y0 = (int)std::floor(sy);
        const int y1 = std::min(y0 + 1, sourceLast.y);
        const float fy = sy - y0;
        for (int i = 0; i < targetSize.x; ++i) {
            const float sx = glm::clamp(((float)i + 0.5f) * sourceSize.x / targetSize.x - 0.5f, 0.0f, (float)sourceLast.x);
            const int x0 = (int)std::floor(sx);
            const int x1 = std::min(x0 + 1, sourceLast.x);
            const float fx = sx - x0;
            const uint8_t* w00 = source.at(glm::ivec2(x0, y0));
            const uint8_t* w10 = source.at(glm::ivec2(x1, y0));
            const uint8_t* w01 = source.at(glm::ivec2(x0, y1));
            const uint8_t* w11 = source.at(glm::ivec2(x1, y1));
            int sum = 0;
            uint8_t* texel = &weights[out];
            for (int layer = 0; layer < PAINT_LAYERS; ++layer) {
                const float weight = glm::mix(glm::mix((float)w00[layer], (float)w10[layer], fx),
                                              glm::mix((float)w01[layer], (float)w11[layer], fx), fy);
                texel[layer] = (uint8_t)glm::clamp((int)std::lround(weight), 0, 255);
                sum += texel[layer];
            }
            // rounding each weight up can push the sum past 255: the largest give the excess back
            while (sum > 255) {
                uint8_t* largest = std::max_element(texel, texel + PAINT_LAYERS);
                --*largest;
                --sum;
            }
            out += PAINT_LAYERS;
        }
    }
    target.setWeights(std::move(weights));
    target.markAllColumnsDirty();
    return target;
}
