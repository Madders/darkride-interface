#!/usr/bin/env python3
"""Writes the rover test's terrain file (PHYSICS.md round 6): rolling hills with a flat pad to start on, in the terrain
library's DRKT format (libraries/terrain/src/terrain/TerrainFile.h), so the physics server and the clients build the same
ground from the same file. The hills are the same function landrover.js uses to place things on the ground.

    make-terrain.py <out.drkt> [size_m=128] [height_m=32] [voxel_m=0.5]

The Terrain entity that points at it must have dimensions size x height x size and that voxel size.
"""
import math
import struct
import sys
import zlib

CHUNK = 32
DISTANCE_SCALE = 32.0
DISTANCE_MAX = 127


def ground_height(x, z, size):
    """Height of the ground above the box's bottom, in metres, for a point (x, z) in metres from the box's low corner.
    Keep in step with LandRover.groundHeight in landrover.js."""
    cx = x - size * 0.5
    cz = z - size * 0.5
    base = 8.0
    hills = 2.5 * math.sin(cx / 9.0) * math.cos(cz / 11.0) + 1.5 * math.sin((cx + cz) / 7.0)
    # a flat pad around the start, blended in over 6 m
    r = math.hypot(cx, cz)
    pad = 14.0
    blend = 6.0
    if r < pad:
        return base
    if r < pad + blend:
        t = (r - pad) / blend
        t = t * t * (3 - 2 * t)
        return base + t * hills
    return base + hills


def encode(d_voxels):
    s = round(d_voxels * DISTANCE_SCALE)
    return max(-DISTANCE_MAX, min(DISTANCE_MAX, int(s)))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    out = sys.argv[1]
    size = float(sys.argv[2]) if len(sys.argv) > 2 else 128.0
    height = float(sys.argv[3]) if len(sys.argv) > 3 else 32.0
    voxel = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
    nx = int(round(size / voxel))
    ny = int(round(height / voxel))
    nz = int(round(size / voxel))
    # the surface height at every column, in voxel units
    column = [[ground_height((x + 0.5) * voxel, (z + 0.5) * voxel, size) / voxel for z in range(nz)] for x in range(nx)]
    cx = (nx + CHUNK - 1) // CHUNK
    cy = (ny + CHUNK - 1) // CHUNK
    cz = (nz + CHUNK - 1) // CHUNK
    chunks = []
    for kz in range(cz):
        for ky in range(cy):
            for kx in range(cx):
                distances = bytearray(CHUNK * CHUNK * CHUNK)
                lo = DISTANCE_MAX
                hi = -DISTANCE_MAX
                for z in range(CHUNK):
                    vz = kz * CHUNK + z
                    for y in range(CHUNK):
                        vy = ky * CHUNK + y
                        for x in range(CHUNK):
                            vx = kx * CHUNK + x
                            if vx < nx and vz < nz:
                                h = column[vx][vz]
                            else:
                                h = column[min(vx, nx - 1)][min(vz, nz - 1)]
                            d = encode(vy - h)   # positive in the air
                            if d < lo:
                                lo = d
                            if d > hi:
                                hi = d
                            distances[(z * CHUNK + y) * CHUNK + x] = d & 0xFF
                if lo == hi:
                    chunks.append(struct.pack("<iiiB", kx, ky, kz, 0) + struct.pack("<bB", lo, 0))
                else:
                    payload = zlib.compress(bytes(distances) + bytes(CHUNK * CHUNK * CHUNK))
                    chunks.append(struct.pack("<iiiB", kx, ky, kz, 1) + struct.pack("<I", len(payload)) + payload)
    with open(out, "wb") as f:
        f.write(b"DRKT")
        f.write(struct.pack("<I", 1))
        f.write(struct.pack("<iii", nx, ny, nz))
        f.write(struct.pack("<i", CHUNK))
        f.write(struct.pack("<I", len(chunks)))
        for chunk in chunks:
            f.write(chunk)
    dense = sum(1 for c in chunks if c[12] == 1)
    print("%s: %d x %d x %d voxels at %.2f m, %d chunks (%d dense)" % (out, nx, ny, nz, voxel, len(chunks), dense))


if __name__ == "__main__":
    main()
