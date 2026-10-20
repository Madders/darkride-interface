//
//  TopSurface.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "TopSurface.h"

#include <algorithm>

namespace terrain {

void TopSurface::rebuild(const VoxelGrid& grid) {
    const glm::ivec3 count = grid.getVoxelCount();
    _footprint = glm::ivec2(count.x, count.z);
    _chunkColumns = glm::ivec2(grid.getChunkCount().x, grid.getChunkCount().z);
    const size_t columns = (size_t)std::max(_footprint.x, 0) * (size_t)std::max(_footprint.y, 0);
    _heights.assign(columns, NO_GROUND_HEIGHT);
    _seconds.assign(columns, NO_GROUND_HEIGHT);
    _floors.assign(columns, NO_GROUND_HEIGHT);
    for (int z = 0; z < _chunkColumns.y; ++z) {
        for (int x = 0; x < _chunkColumns.x; ++x) {
            findChunkColumn(grid, glm::ivec2(x, z));
        }
    }
    for (int z = 0; z < _chunkColumns.y; ++z) {
        for (int x = 0; x < _chunkColumns.x; ++x) {
            settleFloors(glm::ivec2(x, z));
        }
    }
    _dirtyColumns.clear();
    _allDirty = true;
}

void TopSurface::update(VoxelGrid& grid) {
    std::vector<std::pair<glm::ivec3, glm::ivec3>> boxes;
    const bool told = grid.takeChanges(boxes, VoxelGrid::ChangeLog::TopSurface);
    if (grid.isEmpty()) {
        if (!_heights.empty()) {
            _heights.clear();
            _seconds.clear();
            _floors.clear();
            _footprint = glm::ivec2(0);
            _chunkColumns = glm::ivec2(0);
            _dirtyColumns.clear();
            _allDirty = true;
        }
        return;
    }
    const glm::ivec3 count = grid.getVoxelCount();
    if (!told || _heights.empty() || _footprint != glm::ivec2(count.x, count.z)) {
        rebuild(grid);
        return;
    }
    if (boxes.empty()) {
        return;
    }
    // the chunk columns the edits reached, a voxel wider than the boxes: a crossing between a changed voxel and the
    // next one moves too
    ColumnSet reached;
    const glm::ivec2 lastColumn = _chunkColumns - 1;
    for (const auto& box : boxes) {
        const glm::ivec3 low = (box.first >> MAX_CHUNK_LEVEL) - 1;
        const glm::ivec3 high = ((box.second + FINEST_PER_GRID_VOXEL - 1) >> MAX_CHUNK_LEVEL) + 1;
        const glm::ivec2 columnLow = glm::clamp(PaintMap::columnOf(glm::ivec2(low.x, low.z)), glm::ivec2(0), lastColumn);
        const glm::ivec2 columnHigh = glm::clamp(PaintMap::columnOf(glm::ivec2(high.x, high.z)), glm::ivec2(0), lastColumn);
        for (int z = columnLow.y; z <= columnHigh.y; ++z) {
            for (int x = columnLow.x; x <= columnHigh.x; ++x) {
                reached.insert(glm::ivec2(x, z));
            }
        }
    }
    for (const glm::ivec2& column : reached) {
        findChunkColumn(grid, column);
    }
    // the sheltered floors look a few columns around, so the neighbours' change with a chunk column's heights: those
    // again too, and all of their tiles (whose aprons hold its edge besides)
    ColumnSet around;
    for (const glm::ivec2& column : reached) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                const glm::ivec2 neighbour = column + glm::ivec2(dx, dz);
                if (glm::all(glm::greaterThanEqual(neighbour, glm::ivec2(0))) && glm::all(glm::lessThanEqual(neighbour, lastColumn))) {
                    around.insert(neighbour);
                }
            }
        }
    }
    for (const glm::ivec2& column : around) {
        settleFloors(column);
        _dirtyColumns.insert(column);
    }
}

void TopSurface::settleFloors(const glm::ivec2& column) {
    const int x0 = column.x * CHUNK_SIZE;
    const int z0 = column.y * CHUNK_SIZE;
    const int x1 = std::min(x0 + CHUNK_SIZE, _footprint.x);
    const int z1 = std::min(z0 + CHUNK_SIZE, _footprint.y);
    for (int z = z0; z < z1; ++z) {
        for (int x = x0; x < x1; ++x) {
            const size_t i = (size_t)z * _footprint.x + x;
            const float second = _seconds[i];
            float floor = NO_GROUND_HEIGHT;
            if (second > NO_GROUND_HEIGHT) {
                // open ground at the floor's height within reach: the top of a column around is the same ground
                for (int dz = -SHELTER_REACH_COLUMNS; dz <= SHELTER_REACH_COLUMNS && floor == NO_GROUND_HEIGHT; ++dz) {
                    for (int dx = -SHELTER_REACH_COLUMNS; dx <= SHELTER_REACH_COLUMNS; ++dx) {
                        const glm::ivec2 other(x + dx, z + dz);
                        if (glm::any(glm::lessThan(other, glm::ivec2(0))) || glm::any(glm::greaterThanEqual(other, _footprint))) {
                            continue;
                        }
                        if (std::abs(_heights[(size_t)other.y * _footprint.x + other.x] - second) <= SHELTER_TOLERANCE) {
                            floor = second;
                            break;
                        }
                    }
                }
            }
            _floors[i] = floor;
        }
    }
}

void TopSurface::findChunkColumn(const VoxelGrid& grid, const glm::ivec2& column) {
    const glm::ivec3 count = grid.getVoxelCount();
    const int x0 = column.x * CHUNK_SIZE;
    const int z0 = column.y * CHUNK_SIZE;
    const int x1 = std::min(x0 + CHUNK_SIZE, count.x);   // past the last
    const int z1 = std::min(z0 + CHUNK_SIZE, count.z);
    if (x0 >= x1 || z0 >= z1 || count.y <= 0) {
        return;
    }
    const int width = x1 - x0;
    const int depth = z1 - z0;
    // For each column, from the top down: the air above the top's ground (0), the top's ground (1), the air under
    // it (2), the next ground (3, done); and the distance of the air voxel last read, for the crossing.
    std::vector<float> above((size_t)width * depth, SATURATED_DISTANCE);
    std::vector<uint8_t> state((size_t)width * depth, 0);
    int remaining = width * depth;
    // the crossing between the column's ground voxel at y and the air voxel above it
    auto crossing = [&](int i, int y, float distance) {
        const float air = above[i];
        const float t = air > distance ? -distance / (air - distance) : 0.0f;
        return (float)y + glm::clamp(t, 0.0f, 1.0f);
    };
    auto read = [&](int i, int x, int z, int y, float distance) {
        const size_t at = (size_t)z * _footprint.x + x;
        const bool ground = distance < 0.0f;
        switch (state[i]) {
            case 0:
                if (ground) {
                    _heights[at] = crossing(i, y, distance);
                    state[i] = 1;
                } else {
                    above[i] = distance;
                }
                break;
            case 1:
                if (!ground) {
                    above[i] = distance;
                    state[i] = 2;
                }
                break;
            case 2:
                if (ground) {
                    _seconds[at] = crossing(i, y, distance);
                    state[i] = 3;
                    --remaining;
                } else {
                    above[i] = distance;
                }
                break;
            default:
                break;
        }
    };
    for (int cy = (count.y - 1) >> CHUNK_SHIFT; cy >= 0 && remaining > 0; --cy) {
        const int y0 = cy * CHUNK_SIZE;
        const int y1 = std::min(y0 + CHUNK_SIZE, count.y) - 1;   // the highest voxel of the chunk in the grid
        // the level-0 chunk's own voxels; a place split into finer chunks is read through the field at the grid's points
        const Chunk* chunk = grid.findChunk(ChunkKey(0, glm::ivec3(column.x, cy, column.y)));
        if (chunk && chunk->isUniform()) {
            // one value all the way through: it is met at the chunk's top, and nothing changes below
            const float distance = chunk->getUniformValue().getDistance();
            for (int z = z0; z < z1; ++z) {
                for (int x = x0; x < x1; ++x) {
                    read((z - z0) * width + (x - x0), x, z, y1, distance);
                }
            }
            continue;
        }
        for (int z = z0; z < z1; ++z) {
            for (int x = x0; x < x1; ++x) {
                const int i = (z - z0) * width + (x - x0);
                for (int y = y1; y >= y0 && state[i] < 3; --y) {
                    const float distance = chunk ? chunk->get(x - x0, y - y0, z - z0).getDistance()
                                                 : grid.sampleAt(0, glm::ivec3(x, y, z)).getDistance();
                    read(i, x, z, y, distance);
                }
            }
        }
    }
    for (int z = z0; z < z1; ++z) {
        for (int x = x0; x < x1; ++x) {
            const int i = (z - z0) * width + (x - x0);
            const size_t at = (size_t)z * _footprint.x + x;
            if (state[i] == 0) {
                _heights[at] = NO_GROUND_HEIGHT;   // air all the way down
            }
            if (state[i] < 3) {
                _seconds[at] = NO_GROUND_HEIGHT;   // no second ground
            }
        }
    }
}

float TopSurface::floorAt(const glm::ivec2& column) const {
    if (_floors.empty()) {
        return NO_GROUND_HEIGHT;
    }
    const glm::ivec2 clamped = glm::clamp(column, glm::ivec2(0), _footprint - 1);
    return _floors[(size_t)clamped.y * _footprint.x + clamped.x];
}

float TopSurface::heightAt(const glm::ivec2& column) const {
    if (_heights.empty()) {
        return NO_GROUND_HEIGHT;
    }
    const glm::ivec2 clamped = glm::clamp(column, glm::ivec2(0), _footprint - 1);
    return _heights[(size_t)clamped.y * _footprint.x + clamped.x];
}

float TopSurface::heightAt(const glm::vec2& point) const {
    const glm::vec2 base = glm::floor(point);
    const glm::vec2 f = point - base;
    const glm::ivec2 column(base);
    const float low = glm::mix(heightAt(column), heightAt(column + glm::ivec2(1, 0)), f.x);
    const float high = glm::mix(heightAt(column + glm::ivec2(0, 1)), heightAt(column + glm::ivec2(1, 1)), f.x);
    return glm::mix(low, high, f.y);
}

bool TopSurface::isOnTop(const glm::vec3& point, float tolerance) const {
    if (_heights.empty()) {
        return true;
    }
    // the lowest of the four columns around the point, as the shader reads the tiles: the ground at the foot of a
    // wall is on top by its own column, not under the wall's
    const glm::ivec2 low(glm::floor(glm::vec2(point.x, point.z)));
    float top = heightAt(low);
    top = std::min(top, heightAt(low + glm::ivec2(1, 0)));
    top = std::min(top, heightAt(low + glm::ivec2(0, 1)));
    top = std::min(top, heightAt(low + glm::ivec2(1, 1)));
    if (point.y >= top - tolerance) {
        return true;
    }
    // or on a sheltered floor of those columns (the highest, as the shader takes it): at it, within the tolerance
    float floor = floorAt(low);
    floor = std::max(floor, floorAt(low + glm::ivec2(1, 0)));
    floor = std::max(floor, floorAt(low + glm::ivec2(0, 1)));
    floor = std::max(floor, floorAt(low + glm::ivec2(1, 1)));
    return std::abs(point.y - floor) <= tolerance;
}

void TopSurface::fillTile(const glm::ivec2& column, std::vector<float>& heights) const {
    const int size = getTileSize();
    heights.resize((size_t)size * size * 2);
    const glm::ivec2 origin = column * CHUNK_SIZE - PAINT_TILE_APRON;
    for (int j = 0; j < size; ++j) {
        for (int i = 0; i < size; ++i) {
            heights[((size_t)j * size + i) * 2] = heightAt(origin + glm::ivec2(i, j));
            heights[((size_t)j * size + i) * 2 + 1] = floorAt(origin + glm::ivec2(i, j));
        }
    }
}

} // namespace terrain
