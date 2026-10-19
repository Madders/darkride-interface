//
//  SurfaceNets.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SurfaceNets.h"

#include <algorithm>
#include <unordered_map>

using namespace terrain;

namespace {

// Cells run from -1 to CHUNK_SIZE - 1 on each axis: the chunk's own and one layer outside on the low side, whose
// vertices the quads along the low faces need.
const int CELL_COUNT = CHUNK_SIZE + 1;
// The chunk's side in a finer neighbour's voxels.
const int FINE_SIZE = CHUNK_SIZE * 2;

// The corner k of a cell is at the cell's origin plus (k & 1, k >> 1 & 1, k >> 2 & 1).
const glm::vec3 CORNER_OFFSETS[8] = {
    glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f)
};

// The twelve edges of a cell as pairs of corners.
const int EDGES[12][2] = {
    { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },   // along x
    { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },   // along y
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }    // along z
};

const glm::ivec3 AXES[3] = { glm::ivec3(1, 0, 0), glm::ivec3(0, 1, 0), glm::ivec3(0, 0, 1) };

const uint32_t NO_VERTEX = 0xffffffffu;

// A cell: of the chunk's own lattice (levelDelta 0, origin from -1 to CHUNK_SIZE - 1 on each axis), or of a
// neighbour's coarser (-1) or finer (+1) one, the origin in that lattice relative to the chunk's origin.
struct CellRef {
    int levelDelta;
    glm::ivec3 origin;

    bool operator==(const CellRef& other) const { return levelDelta == other.levelDelta && origin == other.origin; }
};

int floorHalf(int value) {
    return value >= 0 ? value / 2 : -((1 - value) / 2);
}

glm::ivec3 floorHalf(const glm::ivec3& value) {
    return glm::ivec3(floorHalf(value.x), floorHalf(value.y), floorHalf(value.z));
}

// Which of the chunk's low neighbours a cell outside it belongs to, as ChunkSamples names them: bit 0 for below
// x = 0, 1 for y, 2 for z. 0 for the chunk's own cells.
int regionOf(const glm::ivec3& cellOrigin) {
    return (cellOrigin.x < 0 ? 1 : 0) | (cellOrigin.y < 0 ? 2 : 0) | (cellOrigin.z < 0 ? 4 : 0);
}

class Mesher {
public:
    explicit Mesher(const ChunkSamples& samples) : _samples(samples) {}

    ChunkMesh run() {
        if (_samples.isOneSided()) {
            return std::move(_mesh);
        }
        _cellVertices.assign((size_t)CELL_COUNT * CELL_COUNT * CELL_COUNT, -1);

        // Every edge whose low voxel is the chunk's own is this chunk's to mesh, up to the grid's end.
        const glm::ivec3& limit = _samples.getLimit();
        for (int z = 0; z < limit.z; ++z) {
            for (int y = 0; y < limit.y; ++y) {
                for (int x = 0; x < limit.x; ++x) {
                    const glm::ivec3 voxel(x, y, z);
                    const bool lowInside = _samples.at(x, y, z).isGround();
                    for (int axis = 0; axis < 3; ++axis) {
                        const glm::ivec3 high = voxel + AXES[axis];
                        if (_samples.at(high.x, high.y, high.z).isGround() != lowInside) {
                            meshOwnEdge(voxel, axis, lowInside);
                        }
                    }
                }
            }
        }
        // and so are the finer edges a finer neighbour brings into the plane of a low face
        for (int axis = 0; axis < 3; ++axis) {
            if (_samples.getNeighbourLevel(1 << axis) > 0) {
                meshFinerFace(axis);
            }
        }
        // and, where the chunk lies against a closed low face of the grid's box, the crossings into it across that
        // face, whose low voxel lies outside the grid and so is no chunk's own (VoxelGrid::applyCaps closed the
        // samples there): the cap along the box's face
        for (int axis = 0; axis < 3; ++axis) {
            if (!_samples.capsLow(axis)) {
                continue;
            }
            const int b = (axis + 1) % 3;
            const int c = (axis + 2) % 3;
            for (int j = 0; j < limit[c]; ++j) {
                for (int i = 0; i < limit[b]; ++i) {
                    glm::ivec3 voxel(0);
                    voxel[axis] = -1;
                    voxel[b] = i;
                    voxel[c] = j;
                    const bool lowInside = _samples.at(voxel.x, voxel.y, voxel.z).isGround();
                    const glm::ivec3 high = voxel + AXES[axis];
                    if (_samples.at(high.x, high.y, high.z).isGround() != lowInside) {
                        meshOwnEdge(voxel, axis, lowInside);
                    }
                }
            }
        }
        return std::move(_mesh);
    }

private:
    // The edge from the chunk's own voxel along an axis, which the surface crosses. The edge is shared by the
    // four cells with origins v, v - b, v - c and v - b - c (b and c the other two axes), seen from +axis in
    // counter-clockwise order (0,0) (1,0) (1,1) (0,1) in the (b, c) plane, b, c following axis cyclically.
    void meshOwnEdge(const glm::ivec3& voxel, int axis, bool lowInside) {
        const glm::ivec3& b = AXES[(axis + 1) % 3];
        const glm::ivec3& c = AXES[(axis + 2) % 3];
        const glm::ivec3 cells[4] = { voxel - b - c, voxel - c, voxel, voxel - b };
        int levels[4];
        int finest = 0;
        for (int i = 0; i < 4; ++i) {
            levels[i] = _samples.getNeighbourLevel(regionOf(cells[i]));
            finest = std::max(finest, levels[i]);
        }
        if (finest <= 0) {
            // The edge is one of the finest cells' around it: one polygon, of a coarser neighbour's cells where
            // the chunk's would lie outside it.
            CellRef places[4];
            for (int i = 0; i < 4; ++i) {
                places[i] = levels[i] < 0 ? CellRef{ -1, floorHalf(cells[i]) } : CellRef{ 0, cells[i] };
            }
            addPolygon(places, lowInside);
            return;
        }
        // A finer neighbour's cells halve the edge: a polygon for each half, of the finer cells where the
        // neighbours are finer and the chunk's own where not. The halves meet at the finer lattice's point
        // between the chunk's own two, which lies on the chunk's low face and so is read between them: the
        // crossing is in one half or the other.
        const glm::ivec3& e = AXES[axis];
        const glm::ivec3 fineVoxel = voxel * 2;
        const Voxel* middle = _samples.findAt(1, fineVoxel + e);
        if (!middle) {
            return;
        }
        const bool middleInside = middle->isGround();
        for (int half = 0; half < 2; ++half) {
            const bool halfLowInside = half == 0 ? lowInside : middleInside;
            const bool halfHighInside = half == 0 ? middleInside : !lowInside;
            if (halfLowInside == halfHighInside) {
                continue;
            }
            const glm::ivec3 fineLow = fineVoxel + e * half;
            const glm::ivec3 fineCells[4] = { fineLow - b - c, fineLow - c, fineLow, fineLow - b };
            CellRef places[4];
            for (int i = 0; i < 4; ++i) {
                places[i] = levels[i] > 0 ? CellRef{ 1, fineCells[i] } : CellRef{ 0, cells[i] };
            }
            addPolygon(places, halfLowInside);
        }
    }

    // The edges of a finer neighbour's lattice in the plane of the chunk's low face along `axis` that are not
    // halves of the chunk's own (those meshOwnEdge takes): the ones along one in-plane axis at the odd points of
    // the other. Each lies between two of the neighbour's cells and one of the chunk's: a triangle.
    void meshFinerFace(int axis) {
        const glm::ivec3& limit = _samples.getFinerLimit();   // the grid's end, in the finer lattice
        if (limit[axis] <= 0) {
            return;
        }
        const int inPlane[2] = { (axis + 1) % 3, (axis + 2) % 3 };
        for (int which = 0; which < 2; ++which) {
            const int along = inPlane[which];
            const int across = inPlane[1 - which];
            const glm::ivec3& b = AXES[(along + 1) % 3];
            const glm::ivec3& c = AXES[(along + 2) % 3];
            const int alongEnd = std::min(FINE_SIZE, limit[along]);
            const int acrossEnd = std::min(FINE_SIZE, limit[across]);
            // at a closed low face of the box along either in-plane axis, the finer edges start a point further out
            // too: the cap's crossings into the chunk across that face
            const int alongStart = _samples.capsLow(along) ? -1 : 0;
            const int acrossStart = _samples.capsLow(across) ? -1 : 1;
            for (int i = alongStart; i < alongEnd; ++i) {
                for (int j = acrossStart; j < acrossEnd; j += 2) {
                    glm::ivec3 low(0);
                    low[along] = i;
                    low[across] = j;
                    const Voxel* lowVoxel = _samples.findAt(1, low);
                    const Voxel* highVoxel = _samples.findAt(1, low + AXES[along]);
                    if (!lowVoxel || !highVoxel || lowVoxel->isGround() == highVoxel->isGround()) {
                        continue;
                    }
                    const glm::ivec3 fineCells[4] = { low - b - c, low - c, low, low - b };
                    CellRef places[4];
                    for (int k = 0; k < 4; ++k) {
                        places[k] = fineCells[k][axis] < 0 ? CellRef{ 1, fineCells[k] } : CellRef{ 0, floorHalf(fineCells[k]) };
                    }
                    addPolygon(places, lowVoxel->isGround());
                }
            }
        }
    }

    // The polygon around an edge: the vertices of the cells in the four places around it, counter-clockwise
    // seen from the edge's +axis when the low voxel is the ground (the surface faces +axis then), clockwise
    // otherwise. A coarser cell can stand in two neighbouring places; the polygon is a triangle then.
    void addPolygon(const CellRef places[4], bool lowInside) {
        uint32_t ids[4];
        int count = 0;
        for (int i = 0; i < 4; ++i) {
            if (places[i] == places[(i + 3) % 4]) {
                continue;
            }
            const uint32_t id = vertexOf(places[i]);
            if (id == NO_VERTEX) {
                return;
            }
            ids[count++] = id;
        }
        if (count < 3) {
            return;
        }
        if (!lowInside) {
            std::reverse(ids + 1, ids + count);
        }
        if (count == 4) {
            _mesh.indices.insert(_mesh.indices.end(), { ids[0], ids[1], ids[2], ids[0], ids[2], ids[3] });
        } else {
            _mesh.indices.insert(_mesh.indices.end(), { ids[0], ids[1], ids[2] });
        }
    }

    static int cellIndex(const glm::ivec3& cell) {
        return ((cell.z + 1) * CELL_COUNT + (cell.y + 1)) * CELL_COUNT + (cell.x + 1);
    }

    uint32_t vertexOf(const CellRef& cell) {
        if (cell.levelDelta == 0) {
            int32_t& slot = _cellVertices[cellIndex(cell.origin)];
            if (slot < 0) {
                slot = (int32_t)makeVertex(cell);   // the chunk's own cells always have their corners
            }
            return (uint32_t)slot;
        }
        // a neighbour's cell: its origin runs from -1 to FINE_SIZE on each axis
        const uint32_t key = (cell.levelDelta > 0 ? 1u : 0u) | ((uint32_t)(cell.origin.x + 2) << 1) |
                             ((uint32_t)(cell.origin.y + 2) << 9) | ((uint32_t)(cell.origin.z + 2) << 17);
        auto found = _neighbourVertices.find(key);
        if (found != _neighbourVertices.end()) {
            return found->second;
        }
        const uint32_t id = makeVertex(cell);
        _neighbourVertices.emplace(key, id);
        return id;
    }

    uint32_t makeVertex(const CellRef& cell) {
        float distances[8];
        uint8_t materials[8];
        for (int k = 0; k < 8; ++k) {
            const glm::ivec3 corner = cell.origin + glm::ivec3(k & 1, (k >> 1) & 1, (k >> 2) & 1);
            const Voxel* voxel = cell.levelDelta == 0 ? &_samples.at(corner.x, corner.y, corner.z) : _samples.findAt(cell.levelDelta, corner);
            if (!voxel) {
                return NO_VERTEX;
            }
            distances[k] = voxel->getDistance();
            materials[k] = voxel->material;
        }

        // The vertex: the mean of the surface's crossings of the cell's edges. A coarser neighbour's cell can
        // have none and still be wanted, when the surface crosses the finer edges in the face it shares with
        // this chunk but none of its own (a bump smaller than its voxels); it gets the cell's centre then, which
        // every chunk that wants it agrees on.
        glm::vec3 sum(0.0f);
        int crossings = 0;
        for (const auto& edge : EDGES) {
            const float d0 = distances[edge[0]];
            const float d1 = distances[edge[1]];
            if ((d0 < 0.0f) == (d1 < 0.0f)) {
                continue;
            }
            const float t = d0 / (d0 - d1);
            sum += glm::mix(CORNER_OFFSETS[edge[0]], CORNER_OFFSETS[edge[1]], t);
            ++crossings;
        }
        const glm::vec3 p = crossings > 0 ? sum / (float)crossings : glm::vec3(0.5f);

        // The normal: the gradient of the trilinear distance field at the vertex, which points out of the ground.
        const float* d = distances;
        const float u = p.x, v = p.y, w = p.z;
        glm::vec3 gradient(
            glm::mix(glm::mix(d[1] - d[0], d[3] - d[2], v), glm::mix(d[5] - d[4], d[7] - d[6], v), w),
            glm::mix(glm::mix(d[2] - d[0], d[3] - d[1], u), glm::mix(d[6] - d[4], d[7] - d[5], u), w),
            glm::mix(glm::mix(d[4] - d[0], d[5] - d[1], u), glm::mix(d[6] - d[2], d[7] - d[3], u), v));
        const float length = glm::length(gradient);
        const glm::vec3 normal = length > 1.0e-6f ? gradient / length : glm::vec3(0.0f, 1.0f, 0.0f);

        // The material: the ground corner's nearest the surface.
        int nearest = -1;
        for (int k = 0; k < 8; ++k) {
            if (distances[k] < 0.0f && (nearest < 0 || distances[k] > distances[nearest])) {
                nearest = k;
            }
        }
        const uint8_t material = materials[nearest >= 0 ? nearest : 0];

        // in the chunk's own voxels: a coarser cell's are two of them, a finer cell's half of one
        const float scale = cell.levelDelta < 0 ? 2.0f : cell.levelDelta > 0 ? 0.5f : 1.0f;
        const uint32_t id = (uint32_t)_mesh.positions.size();
        _mesh.positions.push_back((glm::vec3(cell.origin) + p) * scale);
        _mesh.normals.push_back(normal);
        _mesh.materials.push_back(material);
        return id;
    }

    const ChunkSamples& _samples;
    ChunkMesh _mesh;
    std::vector<int32_t> _cellVertices;
    std::unordered_map<uint32_t, uint32_t> _neighbourVertices;
};

} // namespace

ChunkMesh terrain::meshChunk(const ChunkSamples& samples) {
    return Mesher(samples).run();
}

ChunkMesh terrain::meshChunk(const VoxelGrid& grid, const ChunkKey& chunk) {
    ChunkSamples samples;
    grid.sampleChunk(chunk, samples);
    return meshChunk(samples);
}
