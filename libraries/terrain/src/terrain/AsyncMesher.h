//
//  AsyncMesher.h
//  libraries/terrain/src/terrain
//
//  Meshes a grid's dirty chunks on a thread pool. The editing thread samples each dirty chunk (ChunkSamples: its
//  voxels and one layer of its neighbours') and hands the samples to the pool, where meshChunk() runs on them and
//  nothing else; the finished meshes wait in a queue until the editing thread collects them. One job per chunk is
//  out at a time: a chunk edited while its job runs is dirty again and goes out once more after its mesh has come
//  back, and that mesh is still worth showing meanwhile. Nothing here touches the grid off the editing thread.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_AsyncMesher_h
#define hifi_terrain_AsyncMesher_h

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "SurfaceNets.h"
#include "VoxelGrid.h"

class QThreadPool;

namespace terrain {

class AsyncMesher {
public:
    struct Result {
        ChunkKey key;
        ChunkMesh mesh;      // empty when no surface crosses the chunk
        uint64_t tag { 0 };  // submitSamples()' tag; 0 for the grid's chunks
    };

    // The pool to run on; by default one of this library's own, so meshing neither waits behind nor holds up the
    // application's other background work.
    explicit AsyncMesher(QThreadPool* pool = nullptr);
    // Jobs still running finish on their own and their meshes are dropped.
    ~AsyncMesher();

    AsyncMesher(const AsyncMesher&) = delete;
    AsyncMesher& operator=(const AsyncMesher&) = delete;

    // Samples every dirty chunk that has no job out and starts one for it, taking the chunk off the grid's dirty
    // set; returns how many went out. A chunk whose samples hold no surface is answered at once with an empty
    // mesh. Chunks with a job out stay dirty until collect() has taken their mesh. A dirty key with no chunk at it
    // any more (split, or merged) is only taken off the set.
    int submit(VoxelGrid& grid);
    // Meshes samples made elsewhere (a node of the level of detail's, LevelOfDetail.h) on the pool, ahead of the
    // grid's chunks when the priority is higher; the mesh comes back from collect() with the samples' key and this
    // tag. Nothing is counted as in flight for it: the caller keeps track, with a mesher of its own.
    void submitSamples(std::shared_ptr<const ChunkSamples> samples, uint64_t tag, int priority = 0);
    // The meshes finished since the last call, in no particular order; their chunks have no job out afterwards.
    std::vector<Result> collect();
    // Forgets every job out: their meshes are dropped when they finish. For a grid that was replaced.
    void reset();

    size_t getInFlightCount() const { return _inFlight.size(); }
    bool isInFlight(const ChunkKey& key) const { return _inFlight.count(key) > 0; }
    bool hasResults() const;
    // No job out and no mesh waiting.
    bool isIdle() const { return _inFlight.empty() && !hasResults(); }

    static QThreadPool* defaultPool();

private:
    struct Shared;   // the queue the jobs write to; it outlives the mesher while a job still holds it
    std::shared_ptr<Shared> _shared;
    ChunkKeySet _inFlight;
    uint32_t _generation { 0 };
    QThreadPool* _pool;
};

} // namespace terrain

#endif // hifi_terrain_AsyncMesher_h
