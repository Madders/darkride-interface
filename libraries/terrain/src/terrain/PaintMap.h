//
//  PaintMap.h
//  libraries/terrain/src/terrain
//
//  The paint map of a Terrain entity (TERRAIN.md section 5): a picture laid over the ground's footprint, seen from
//  above, that says how much of each of the terrain's eight layers covers every point. It is what the Paint tool
//  paints and what the shader reads wherever the ground faces up; where it faces sideways or down, and wherever the
//  map holds nothing, the voxels' own materials show instead. A texel's eight weights are bytes, each a layer's own
//  coverage, 0 to 255, on its own: the shader draws the layers in the stack's order (Layers.h), each over what is
//  below it by its coverage, so a map that was never painted changes nothing, painting a layer covers without taking
//  anything from the others, and erasing a layer lowers its own coverage and shows what is under it again.
//
//  The map has texelsPerVoxel texels along each voxel of the footprint (x and z of voxel space); texel (i, j) covers
//  voxel x from i / texelsPerVoxel on, and its centre is where a brush is evaluated. The renderer draws each chunk
//  column of the ground with a tile of its own cut from the map, the column's texels and an apron of the
//  neighbours' around them, since a chunk's mesh reaches up to two of its voxels past its low faces
//  (SurfaceNets.h); the map tracks which columns' tiles are out of date, as the grid tracks its dirty chunks. A
//  coarser node of the level of detail (LevelOfDetail.h) gets a tile of the same size over its own wider footprint,
//  each texel the mean of the map's texels under it.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_PaintMap_h
#define hifi_terrain_PaintMap_h

#include <cstdint>
#include <set>
#include <vector>

#include <glm/glm.hpp>

#include "BrushMask.h"
#include "Chunk.h"

namespace terrain {

const int PAINT_LAYERS = 8;        // weights per texel, one for each of the terrain's layers
const int PAINT_TILE_APRON = 2;    // voxels of the neighbours' texels around a column's own in its tile
const int MAX_PAINT_TEXELS_PER_VOXEL = 16;

struct IVec2Less {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) {
            return a.x < b.x;
        }
        return a.y < b.y;
    }
};
using ColumnSet = std::set<glm::ivec2, IVec2Less>;

// A rectangle of the map's texels, copied out to be put back later (an undo step keeps them).
struct PaintBlock {
    glm::ivec2 low { 0 };             // the texel at its low corner
    glm::ivec2 size { 0 };
    std::vector<uint8_t> weights;     // size.x * size.y texels of PAINT_LAYERS bytes, x fastest

    bool isEmpty() const { return weights.empty(); }
};

class PaintMap {
public:
    PaintMap() = default;
    // An unpainted map over a footprint of voxels (x, z), texelsPerVoxel texels along each voxel.
    PaintMap(const glm::ivec2& voxelFootprint, int texelsPerVoxel);

    void reset(const glm::ivec2& voxelFootprint, int texelsPerVoxel);

    bool isEmpty() const { return _weights.empty(); }
    const glm::ivec2& getSize() const { return _size; }     // texels
    int getTexelsPerVoxel() const { return _texelsPerVoxel; }
    glm::ivec2 getVoxelFootprint() const { return _size / glm::max(_texelsPerVoxel, 1); }
    bool contains(const glm::ivec2& texel) const;

    // A texel's PAINT_LAYERS weights. Changing them through the pointer marks nothing dirty; markTexelsDirty() does.
    const uint8_t* at(const glm::ivec2& texel) const { return &_weights[indexOf(texel)]; }
    uint8_t* at(const glm::ivec2& texel) { return &_weights[indexOf(texel)]; }
    // Every texel's weights, x fastest, PAINT_LAYERS bytes each.
    const std::vector<uint8_t>& getWeights() const { return _weights; }
    // Replaces every weight; the vector must hold getSize().x * getSize().y * PAINT_LAYERS bytes. Returns whether it did.
    bool setWeights(std::vector<uint8_t> weights);
    // Whether any texel holds any weight: an unpainted map need not be saved.
    bool isPainted() const;
    // Turns a map whose weights were shares of the texel (summing to at most 255, the rest the voxel material, as
    // terrain files before version 4 hold them) into coverages drawn in the given order of layers, bottom to top:
    // each layer's coverage becomes its share of all that is drawn up to and including it, so the stack drawn
    // in that order shows exactly what the shares did. Layers not in the order lose their weight.
    void sharesToCoverage(const std::vector<uint8_t>& order);

    // The texels from low to high inclusive, clamped to the map; empty when none lie in it.
    PaintBlock copy(const glm::ivec2& low, const glm::ivec2& high) const;
    // Puts a block's texels back where they came from (those within the map), and marks them dirty.
    void paste(const PaintBlock& block);

    // Chunk columns: column (cx, cz) holds the texels of chunks (cx, *, cz). Floor division, as the grid's chunkOf.
    static glm::ivec2 columnOf(const glm::ivec2& voxel);
    glm::ivec2 getColumnCount() const;
    // The texels of a column's own square, clamped to the map.
    void columnTexels(const glm::ivec2& column, glm::ivec2& low, glm::ivec2& high) const;

    // A column's tile for the renderer: getTileSize() texels a side, the column's own texels with PAINT_TILE_APRON
    // voxels of the neighbours' around them (the map's edge texels repeat beyond its edge), as two pictures of four
    // bytes a texel, layers 0 to 3 and 4 to 7, x fastest.
    int getTileSize() const { return (CHUNK_SIZE + 2 * PAINT_TILE_APRON) * _texelsPerVoxel; }
    void fillTile(const glm::ivec2& column, std::vector<uint8_t>& layers0to3, std::vector<uint8_t>& layers4to7) const;
    // The tile of a coarser node's column (a level below 0: 2^-level level-0 columns a side, column in that level's
    // columns), the same size as a level-0 column's and laid out alike in the node's own voxels: each texel the mean
    // of the map's texels it covers, of a sample of them four a side at the coarsest levels. Level 0 is the
    // column's own tile.
    void fillTile(int level, const glm::ivec2& column, std::vector<uint8_t>& layers0to3, std::vector<uint8_t>& layers4to7) const;

    // The columns whose tiles are out of date: a changed texel dirties its column and any neighbour whose apron
    // holds it. The renderer clears the set once it has remade the tiles.
    const ColumnSet& getDirtyColumns() const { return _dirtyColumns; }
    void clearDirtyColumns() { _dirtyColumns.clear(); }
    void markAllColumnsDirty();
    // The texels from low to high inclusive changed.
    void markTexelsDirty(const glm::ivec2& low, const glm::ivec2& high);

private:
    size_t indexOf(const glm::ivec2& texel) const { return ((size_t)texel.y * _size.x + texel.x) * PAINT_LAYERS; }

    glm::ivec2 _size { 0 };
    int _texelsPerVoxel { 1 };
    std::vector<uint8_t> _weights;
    ColumnSet _dirtyColumns;
};

// The Paint tool's brush: a disc on the footprint, in voxel units, with the sculpt brushes' falloff, or a picture
// (BrushMask.h) laid over its square and turned about its centre.
struct PaintBrush {
    glm::vec2 center { 0.0f };      // voxel coordinates (x, z)
    float radius { 4.0f };          // voxels
    float strength { 0.5f };        // 0 to 1: how far one application goes toward the full effect
    float hardness { 0.5f };        // 0 to 1: the part of the radius with the full effect before the falloff
    uint8_t layer { 0 };            // the layer painted
    const BrushMask* mask { nullptr };   // the picture the brush takes its shape from, if any (not owned)
    float angle { 0.0f };           // radians, the picture turned about the centre

    // Round: 1 at the centre, falling smoothly to 0 at the radius. With a picture: its value under the point.
    float weightAt(const glm::vec2& point) const;
    // How far from the centre, in voxels, the brush can reach along x and z.
    glm::vec2 reach() const;
};

// The texels (inclusive bounds) a brush can change; false when none lie in the map.
bool brushTexelBounds(const PaintMap& map, const PaintBrush& brush, glm::ivec2& low, glm::ivec2& high);

// One application of the brush: under it the layer's own coverage grows toward full, the other layers keeping
// theirs, so it covers what is below it in the stack; erasing shrinks it toward none, so what is below shows
// again. Returns how many texels changed.
int paint(PaintMap& map, const PaintBrush& brush, bool erase);

} // namespace terrain

#endif // hifi_terrain_PaintMap_h
