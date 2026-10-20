//
//  SurfaceNets.h
//  libraries/terrain/src/terrain
//
//  Meshes one chunk's samples by surface nets (Gibson 1998, in the form of Lysenko's "naive surface nets"): every
//  cell between eight voxels that the surface crosses gets one vertex, at the mean of the surface's crossings of the
//  cell's edges, and every crossed edge gets a quad joining the vertices of the four cells around it. The normal is
//  the gradient of the distance field at the vertex, so it is smooth across cells; the material is the ground
//  voxel's nearest the surface. The cells one step outside the chunk are meshed too (from the neighbours' samples),
//  so a chunk's quads along its low faces have vertices to join and the neighbour computes the same ones: meshes
//  meet exactly without talking to each other. Positions are in the chunk's own voxels, whatever its level.
//
//  Where chunks of different levels meet, this is dual contouring over the octree of cells (Ju, Losasso, Schaefer
//  and Warren, "Dual Contouring of Hermite Data", 2002), of which surface nets is the uniform case: a polygon for
//  every edge of the finest cells around it, joining the one vertex of each cell that shares the edge, a triangle
//  where a coarser cell stands in two of the four places. An edge belongs to the chunk whose box holds its low
//  end (low faces in, high faces out), so a finer neighbour's cells across a low face halve the face's own edges
//  and add edges of their own in the face's plane, and a coarser neighbour's cells across a low face stand in for
//  the layer of cells outside it. A chunk builds its neighbours' cells along the join from the neighbours' own
//  samples at their level (ChunkSamples' blocks), exactly as they do, so the meshes meet without a crack.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_SurfaceNets_h
#define hifi_terrain_SurfaceNets_h

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "VoxelGrid.h"

namespace terrain {

struct ChunkMesh {
    // The chunk's voxel units, relative to the chunk's origin (its voxel 0, 0, 0): within the chunk, and up to
    // one voxel outside its low faces, two where the neighbour there is coarser.
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;     // unit length, pointing out of the ground
    std::vector<uint8_t> materials;     // the ground's material at each vertex
    std::vector<uint32_t> indices;      // triangles, counter-clockwise seen from the air

    bool isEmpty() const { return indices.empty(); }
    size_t getVertexCount() const { return positions.size(); }
    size_t getTriangleCount() const { return indices.size() / 3; }
};

ChunkMesh meshChunk(const ChunkSamples& samples);
ChunkMesh meshChunk(const VoxelGrid& grid, const ChunkKey& chunk);

} // namespace terrain

#endif // hifi_terrain_SurfaceNets_h
