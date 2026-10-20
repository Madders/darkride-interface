//
//  TerrainFile.h
//  libraries/terrain/src/terrain
//
//  The terrain file: every chunk of a voxel grid, and the paint map over it, in one file, the asset a Terrain
//  entity's terrainURL points at. Little-endian throughout:
//
//    char[4] "DRKT"   uint32 version (1 to 5)   int32 voxelCount x, y, z   int32 chunkSize (32)   uint32 chunkCount
//    then chunkCount times:
//      int32 chunk x, y, z   [version 3 on: uint8 level]   uint8 kind: 0 uniform, 1 dense
//      uniform:  int8 distance   uint8 material
//      dense:    uint32 length   bytes[length]: a zlib stream of the chunk's CHUNK_VOXEL_COUNT distance bytes
//                followed by its CHUNK_VOXEL_COUNT material bytes, in Chunk::indexOf order
//    versions 2 on go on with the paint map (PaintMap.h):
//      uint32 width, height (texels)   uint32 texelsPerVoxel   uint32 length   bytes[length]: a zlib stream of
//      PAINT_LAYERS planes of width * height bytes, layer 0's weights at every texel (x fastest), then layer 1's…
//      (width and height 0 when there is no map to write)
//    version 4 goes on with the layer stack (Layers.h), bottom to top:
//      uint8 layerCount, then for each: uint8 slot, blend, visible, flags   float32 tiling, offset x, offset z,
//      opacity, normal strength, colour r, g, b, tint r, g, b   uint16 length + bytes (UTF-8) name   uint16 length +
//      bytes material URL
//      [version 5 on, after the material URL: the normal, roughness, occlusion and height map URLs the same way;
//      flags bit 0 is "use the normal map" (0 in a version 4 file, which reads as on)]
//
//  Version 3 gives each chunk its level (VoxelGrid.h), its coordinates then being at that level; versions 1 and 2
//  hold level-0 chunks only. Version 4 adds the layers, and with them the map's bytes become each layer's own
//  coverage (drawn in the stack's order) where before they were shares of the texel summing to 255 at most; a map
//  from an older file is converted as it is read (PaintMap::sharesToCoverage). Version 5 adds each layer's maps,
//  its normal map toggle and the Height blend (Layer::hasMaps). A terrain is written in the oldest version that
//  holds it: version 1 while its map was never painted, 2 with paint, 3 once any chunk is split, 4 with a layer
//  stack, 5 once a layer has a map, so a file reads wherever it can. A stack read with no map on any layer (a version
//  4 file, or a version 5 saved before any layer had a map) gets the defaults' maps on the layers that use the
//  default materials' colour textures (LayerStack::fillDefaultMaps). Chunks keep their own coordinates so a later version can
//  leave them in files of their own and fetch them one at a time (streaming, TERRAIN.md section 10). Chunks missing
//  from a file take the grid's fill value.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_TerrainFile_h
#define hifi_terrain_TerrainFile_h

#include <cstdint>
#include <string>
#include <vector>

#include "Layers.h"
#include "PaintMap.h"
#include "VoxelGrid.h"

namespace terrain {

using Bytes = std::vector<uint8_t>;

// The paint map, if given and painted, must cover the grid's footprint; otherwise the file holds the grid alone.
// The layers, if given, well formed and not empty, go in too (version 4; 5 when any has maps).
Bytes saveTerrain(const VoxelGrid& grid, const PaintMap* paint = nullptr, const LayerStack* layers = nullptr);

// Replaces the grid, and the paint map and layers if given, with the file's contents; a file without a paint map
// leaves the map empty, for the caller to make a fresh one, and one without layers leaves the stack empty, for the
// caller to fill with the defaults. On failure all are left empty and error, if given, says why.
bool loadTerrain(const uint8_t* data, size_t size, VoxelGrid& grid, PaintMap* paint, LayerStack* layers = nullptr,
                 std::string* error = nullptr);
inline bool loadTerrain(const Bytes& data, VoxelGrid& grid, PaintMap* paint, LayerStack* layers = nullptr,
                        std::string* error = nullptr) {
    return loadTerrain(data.data(), data.size(), grid, paint, layers, error);
}

} // namespace terrain

#endif // hifi_terrain_TerrainFile_h
