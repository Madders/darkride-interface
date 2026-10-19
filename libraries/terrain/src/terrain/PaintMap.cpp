//
//  PaintMap.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PaintMap.h"

#include <algorithm>
#include <cmath>

using namespace terrain;

namespace {

int floorDiv(int value, int divisor) {
    return (value >= 0) ? value / divisor : -((-value + divisor - 1) / divisor);
}

const int WEIGHT_MAX = 255;

} // namespace

PaintMap::PaintMap(const glm::ivec2& voxelFootprint, int texelsPerVoxel) {
    reset(voxelFootprint, texelsPerVoxel);
}

void PaintMap::reset(const glm::ivec2& voxelFootprint, int texelsPerVoxel) {
    _texelsPerVoxel = glm::clamp(texelsPerVoxel, 1, MAX_PAINT_TEXELS_PER_VOXEL);
    _size = glm::max(voxelFootprint, glm::ivec2(0)) * _texelsPerVoxel;
    _weights.assign((size_t)_size.x * _size.y * PAINT_LAYERS, 0);
    markAllColumnsDirty();
}

bool PaintMap::contains(const glm::ivec2& texel) const {
    return texel.x >= 0 && texel.y >= 0 && texel.x < _size.x && texel.y < _size.y;
}

bool PaintMap::setWeights(std::vector<uint8_t> weights) {
    if (weights.size() != _weights.size()) {
        return false;
    }
    _weights = std::move(weights);
    markAllColumnsDirty();
    return true;
}

bool PaintMap::isPainted() const {
    return std::any_of(_weights.begin(), _weights.end(), [](uint8_t weight) { return weight != 0; });
}

void PaintMap::sharesToCoverage(const std::vector<uint8_t>& order) {
    if (isEmpty()) {
        return;
    }
    const size_t texelCount = _weights.size() / PAINT_LAYERS;
    uint8_t coverage[PAINT_LAYERS];
    for (size_t texel = 0; texel < texelCount; ++texel) {
        uint8_t* shares = &_weights[texel * PAINT_LAYERS];
        int sum = 0;
        for (int k = 0; k < PAINT_LAYERS; ++k) {
            sum += shares[k];
            coverage[k] = 0;
        }
        // drawn bottom to top, each layer covers its share of all that is drawn up to and including it, starting
        // from the voxel material's share
        int drawn = std::max(0, WEIGHT_MAX - sum);
        for (uint8_t slot : order) {
            if (slot >= PAINT_LAYERS) {
                continue;
            }
            const int share = shares[slot];
            drawn += share;
            coverage[slot] = drawn > 0 ? (uint8_t)std::lround(share * (float)WEIGHT_MAX / (float)drawn) : 0;
        }
        for (int k = 0; k < PAINT_LAYERS; ++k) {
            shares[k] = coverage[k];
        }
    }
    markAllColumnsDirty();
}

PaintBlock PaintMap::copy(const glm::ivec2& low, const glm::ivec2& high) const {
    PaintBlock block;
    if (isEmpty()) {
        return block;
    }
    const glm::ivec2 from = glm::max(low, glm::ivec2(0));
    const glm::ivec2 to = glm::min(high, _size - 1);
    if (glm::any(glm::greaterThan(from, to))) {
        return block;
    }
    block.low = from;
    block.size = to - from + 1;
    block.weights.resize((size_t)block.size.x * block.size.y * PAINT_LAYERS);
    const size_t rowBytes = (size_t)block.size.x * PAINT_LAYERS;
    for (int y = 0; y < block.size.y; ++y) {
        std::copy_n(at(glm::ivec2(from.x, from.y + y)), rowBytes, block.weights.begin() + (size_t)y * rowBytes);
    }
    return block;
}

void PaintMap::paste(const PaintBlock& block) {
    if (isEmpty() || block.isEmpty()) {
        return;
    }
    const glm::ivec2 from = glm::max(block.low, glm::ivec2(0));
    const glm::ivec2 to = glm::min(block.low + block.size - 1, _size - 1);
    if (glm::any(glm::greaterThan(from, to))) {
        return;
    }
    const size_t rowBytes = (size_t)(to.x - from.x + 1) * PAINT_LAYERS;
    for (int y = from.y; y <= to.y; ++y) {
        const size_t source = (((size_t)(y - block.low.y) * block.size.x) + (from.x - block.low.x)) * PAINT_LAYERS;
        std::copy_n(block.weights.begin() + source, rowBytes, at(glm::ivec2(from.x, y)));
    }
    markTexelsDirty(from, to);
}

glm::ivec2 PaintMap::columnOf(const glm::ivec2& voxel) {
    return glm::ivec2(floorDiv(voxel.x, CHUNK_SIZE), floorDiv(voxel.y, CHUNK_SIZE));
}

glm::ivec2 PaintMap::getColumnCount() const {
    const glm::ivec2 footprint = getVoxelFootprint();
    return (footprint + CHUNK_SIZE - 1) / CHUNK_SIZE;
}

void PaintMap::columnTexels(const glm::ivec2& column, glm::ivec2& low, glm::ivec2& high) const {
    const int texelsPerColumn = CHUNK_SIZE * _texelsPerVoxel;
    low = glm::max(column * texelsPerColumn, glm::ivec2(0));
    high = glm::min((column + 1) * texelsPerColumn - 1, _size - 1);
}

void PaintMap::fillTile(const glm::ivec2& column, std::vector<uint8_t>& layers0to3, std::vector<uint8_t>& layers4to7) const {
    const int tileSize = getTileSize();
    const size_t texelCount = (size_t)tileSize * tileSize;
    layers0to3.assign(texelCount * 4, 0);
    layers4to7.assign(texelCount * 4, 0);
    if (isEmpty()) {
        return;
    }
    const glm::ivec2 origin = (column * CHUNK_SIZE - PAINT_TILE_APRON) * _texelsPerVoxel;   // the tile's texel (0, 0) in the map
    for (int j = 0; j < tileSize; ++j) {
        const int z = glm::clamp(origin.y + j, 0, _size.y - 1);
        for (int i = 0; i < tileSize; ++i) {
            const int x = glm::clamp(origin.x + i, 0, _size.x - 1);
            const uint8_t* weights = at(glm::ivec2(x, z));
            const size_t out = ((size_t)j * tileSize + i) * 4;
            std::copy_n(weights, 4, layers0to3.begin() + out);
            std::copy_n(weights + 4, 4, layers4to7.begin() + out);
        }
    }
}

void PaintMap::fillTile(int level, const glm::ivec2& column, std::vector<uint8_t>& layers0to3, std::vector<uint8_t>& layers4to7) const {
    if (level >= 0) {
        fillTile(column, layers0to3, layers4to7);
        return;
    }
    const int tileSize = getTileSize();
    const size_t texelCount = (size_t)tileSize * tileSize;
    layers0to3.assign(texelCount * 4, 0);
    layers4to7.assign(texelCount * 4, 0);
    if (isEmpty()) {
        return;
    }
    const int factor = 1 << -level;               // the map's texels a side under one of the tile's
    const int step = std::max(1, factor / 4);     // at most four a side of them are read
    const glm::ivec2 origin = (column * CHUNK_SIZE - PAINT_TILE_APRON) * _texelsPerVoxel * factor;
    uint32_t sums[PAINT_LAYERS];
    for (int j = 0; j < tileSize; ++j) {
        for (int i = 0; i < tileSize; ++i) {
            std::fill(std::begin(sums), std::end(sums), 0u);
            uint32_t count = 0;
            for (int dz = 0; dz < factor; dz += step) {
                const int z = glm::clamp(origin.y + j * factor + dz, 0, _size.y - 1);
                for (int dx = 0; dx < factor; dx += step) {
                    const uint8_t* weights = at(glm::ivec2(glm::clamp(origin.x + i * factor + dx, 0, _size.x - 1), z));
                    for (int k = 0; k < PAINT_LAYERS; ++k) {
                        sums[k] += weights[k];
                    }
                    ++count;
                }
            }
            const size_t out = ((size_t)j * tileSize + i) * 4;
            for (int k = 0; k < 4; ++k) {
                layers0to3[out + k] = (uint8_t)((sums[k] + count / 2) / count);
                layers4to7[out + k] = (uint8_t)((sums[k + 4] + count / 2) / count);
            }
        }
    }
}

void PaintMap::markAllColumnsDirty() {
    _dirtyColumns.clear();
    const glm::ivec2 columns = getColumnCount();
    for (int z = 0; z < columns.y; ++z) {
        for (int x = 0; x < columns.x; ++x) {
            _dirtyColumns.insert(glm::ivec2(x, z));
        }
    }
}

void PaintMap::markTexelsDirty(const glm::ivec2& low, const glm::ivec2& high) {
    if (isEmpty()) {
        return;
    }
    const glm::ivec2 from = glm::max(low, glm::ivec2(0));
    const glm::ivec2 to = glm::min(high, _size - 1);
    if (glm::any(glm::greaterThan(from, to))) {
        return;
    }
    // a column's tile holds its own voxels and an apron of its neighbours', so a texel within the apron of a
    // neighbouring column dirties that column too
    const glm::ivec2 columnLow = glm::max(columnOf(from / _texelsPerVoxel - PAINT_TILE_APRON), glm::ivec2(0));
    const glm::ivec2 columnHigh = glm::min(columnOf(to / _texelsPerVoxel + PAINT_TILE_APRON), getColumnCount() - 1);
    for (int z = columnLow.y; z <= columnHigh.y; ++z) {
        for (int x = columnLow.x; x <= columnHigh.x; ++x) {
            _dirtyColumns.insert(glm::ivec2(x, z));
        }
    }
}

// The brush

float PaintBrush::weightAt(const glm::vec2& point) const {
    if (radius <= 0.0f) {
        return 0.0f;
    }
    if (mask && !mask->isEmpty()) {
        return mask->weightAt((point - center) / radius, angle, hardness);
    }
    const float distance = glm::length(point - center);
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

glm::vec2 PaintBrush::reach() const {
    return glm::vec2(radius * (mask && !mask->isEmpty() ? BrushMask::reach(angle) : 1.0f));
}

bool terrain::brushTexelBounds(const PaintMap& map, const PaintBrush& brush, glm::ivec2& low, glm::ivec2& high) {
    if (map.isEmpty() || brush.radius <= 0.0f) {
        return false;
    }
    const float texelsPerVoxel = (float)map.getTexelsPerVoxel();
    const glm::vec2 reach = brush.reach();
    low = glm::max(glm::ivec2(glm::floor((brush.center - reach) * texelsPerVoxel)), glm::ivec2(0));
    high = glm::min(glm::ivec2(glm::ceil((brush.center + reach) * texelsPerVoxel)), map.getSize() - 1);
    return !glm::any(glm::greaterThan(low, high));
}

int terrain::paint(PaintMap& map, const PaintBrush& brush, bool erase) {
    glm::ivec2 low, high;
    if (!brushTexelBounds(map, brush, low, high)) {
        return 0;
    }
    const float strength = glm::clamp(brush.strength, 0.0f, 1.0f);
    const int layer = glm::clamp((int)brush.layer, 0, PAINT_LAYERS - 1);
    const float texelSize = 1.0f / (float)map.getTexelsPerVoxel();
    int changed = 0;
    glm::ivec2 changedLow(map.getSize());
    glm::ivec2 changedHigh(-1);
    for (int z = low.y; z <= high.y; ++z) {
        for (int x = low.x; x <= high.x; ++x) {
            const glm::vec2 centre = (glm::vec2(x, z) + 0.5f) * texelSize;
            const float amount = strength * brush.weightAt(centre);
            if (amount <= 0.0f) {
                continue;
            }
            // the layer's own coverage alone: toward full, or (erasing) toward none; the other layers keep theirs
            uint8_t* weights = map.at(glm::ivec2(x, z));
            const float target = erase ? weights[layer] * (1.0f - amount) : weights[layer] + amount * (WEIGHT_MAX - weights[layer]);
            const uint8_t next = (uint8_t)glm::clamp((int)std::lround(target), 0, WEIGHT_MAX);
            if (next != weights[layer]) {
                weights[layer] = next;
                ++changed;
                changedLow = glm::min(changedLow, glm::ivec2(x, z));
                changedHigh = glm::max(changedHigh, glm::ivec2(x, z));
            }
        }
    }
    if (changed > 0) {
        map.markTexelsDirty(changedLow, changedHigh);
    }
    return changed;
}
