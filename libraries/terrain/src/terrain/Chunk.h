//
//  Chunk.h
//  libraries/terrain/src/terrain
//
//  A cube of CHUNK_SIZE voxels a side, the unit a terrain is stored, meshed, drawn and collided in. A chunk whose
//  voxels are all the same (all air above the ground, all ground deep below it) is kept as that one value until a
//  voxel is set differently; compact() folds it back once it is uniform again.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_Chunk_h
#define hifi_terrain_Chunk_h

#include <vector>

#include "Voxel.h"

namespace terrain {

const int CHUNK_SIZE = 32;
const int CHUNK_VOXEL_COUNT = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

class Chunk {
public:
    explicit Chunk(const Voxel& fill = Voxel()) : _uniform(fill) {}

    bool isUniform() const { return _voxels.empty(); }
    const Voxel& getUniformValue() const { return _uniform; }

    // Local coordinates run from 0 to CHUNK_SIZE - 1; x varies fastest in the dense array.
    static int indexOf(int x, int y, int z) { return (z * CHUNK_SIZE + y) * CHUNK_SIZE + x; }

    const Voxel& get(int x, int y, int z) const { return isUniform() ? _uniform : _voxels[indexOf(x, y, z)]; }
    // Some voxel of the chunk: the one value while it is uniform, the first otherwise.
    const Voxel& getFirstVoxel() const { return isUniform() ? _uniform : _voxels[0]; }
    // Returns true when the voxel changed.
    bool set(int x, int y, int z, const Voxel& voxel);

    void fill(const Voxel& voxel);
    // Folds a chunk whose voxels have all become equal back into one value; returns whether the chunk is uniform now.
    bool compact();

    // The voxels in indexOf() order; empty while the chunk is uniform.
    const std::vector<Voxel>& getVoxels() const { return _voxels; }
    // Replaces every voxel; the vector must hold CHUNK_VOXEL_COUNT entries. Compacts when they are all equal.
    void setVoxels(std::vector<Voxel> voxels);

    // The same voxels, whether kept as one value or spelled out.
    bool operator==(const Chunk& other) const;
    bool operator!=(const Chunk& other) const { return !(*this == other); }

private:
    void expand();

    Voxel _uniform;
    std::vector<Voxel> _voxels;
};

} // namespace terrain

#endif // hifi_terrain_Chunk_h
