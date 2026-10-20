//
//  Resample.h
//  libraries/terrain/src/terrain
//
//  Resampling a terrain to another voxel count over the same box. A Terrain entity's box is fixed by its
//  dimensions; its grid may hold any number of voxels along each side, and the voxel's size follows (the box over
//  the count). Resampling makes a new grid with another count whose surface lies where the old one's did: each new
//  voxel reads the old distance field between the old voxels (trilinear interpolation, at whatever level the chunk
//  there keeps), scaled into the new voxel units, and takes the nearest old voxel's material. The paint map resamples the same way over the footprint
//  (bilinear). It is what the editor's Resample action runs when a terrain has been scaled, or its voxel size
//  changed, so that the voxels are the entity's voxel size again, and it is the split and merge of a chunk between
//  levels of detail: a chunk sampled at twice or half its count is the same ground, finer or coarser.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_Resample_h
#define hifi_terrain_Resample_h

#include <glm/glm.hpp>

#include "PaintMap.h"
#include "VoxelGrid.h"

namespace terrain {

// The source grid's ground at targetCount voxels along each side of the same box, every chunk at level 0. Chunks
// of the source that hold no surface stay one value in the result. The work is spread over the machine's cores; threads 0 means all of
// them, 1 means this thread alone. An empty source gives an empty grid.
VoxelGrid resampleGrid(const VoxelGrid& source, const glm::ivec3& targetCount, int threads = 0);

// The source map's paint over a footprint of voxelFootprint voxels at texelsPerVoxel texels each, the same picture
// at the new texel count: every weight is read between the old texels, and a texel's weights still sum to at most
// 255. An unpainted or empty source gives a fresh, unpainted map.
PaintMap resamplePaint(const PaintMap& source, const glm::ivec2& voxelFootprint, int texelsPerVoxel);

} // namespace terrain

#endif // hifi_terrain_Resample_h
