//
//  Chunk.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Chunk.h"

#include <algorithm>
#include <cassert>

using namespace terrain;

bool Chunk::set(int x, int y, int z, const Voxel& voxel) {
    if (isUniform()) {
        if (voxel == _uniform) {
            return false;
        }
        expand();
    }
    Voxel& target = _voxels[indexOf(x, y, z)];
    if (target == voxel) {
        return false;
    }
    target = voxel;
    return true;
}

void Chunk::fill(const Voxel& voxel) {
    _uniform = voxel;
    _voxels.clear();
    _voxels.shrink_to_fit();
}

bool Chunk::compact() {
    if (isUniform()) {
        return true;
    }
    const Voxel first = _voxels[0];
    if (std::all_of(_voxels.begin(), _voxels.end(), [&](const Voxel& voxel) { return voxel == first; })) {
        fill(first);
        return true;
    }
    return false;
}

void Chunk::setVoxels(std::vector<Voxel> voxels) {
    assert(voxels.size() == (size_t)CHUNK_VOXEL_COUNT);
    _voxels = std::move(voxels);
    compact();
}

bool Chunk::operator==(const Chunk& other) const {
    if (isUniform() && other.isUniform()) {
        return _uniform == other._uniform;
    }
    if (!isUniform() && !other.isUniform()) {
        return _voxels == other._voxels;
    }
    const Voxel& value = isUniform() ? _uniform : other._uniform;
    const std::vector<Voxel>& voxels = isUniform() ? other._voxels : _voxels;
    return std::all_of(voxels.begin(), voxels.end(), [&](const Voxel& voxel) { return voxel == value; });
}

void Chunk::expand() {
    _voxels.assign(CHUNK_VOXEL_COUNT, _uniform);
}
