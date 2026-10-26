// <!
//  Created by Bradley Austin Davis on 2018/05/25
//  Copyright 2013-2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
// !>

// <@if not ENTITIES_SHADER_CONSTANTS_H@>
// <@def ENTITIES_SHADER_CONSTANTS_H@>

// Hack comment to absorb the extra '//' scribe prepends

#ifndef ENTITIES_SHADER_CONSTANTS_H
#define ENTITIES_SHADER_CONSTANTS_H


// Mirror glass (mirror_screen.slf, a pipeline of its own): the picture, and the region of the screen it covers at
// the material buffer slot, which the glass's draw has no material for (the next item binds its own there)
#define ENTITIES_TEXTURE_MIRROR_PICTURE 0
#define ENTITIES_BUFFER_MIRROR 1

// Terrain: the layers' colour textures in one array (a slice per layer, as their normal and mask maps are kept), the
// chunk's paint map tiles (layers 0 to 3 and 4 to 7 of the weights), the column's top surface heights and the two
// map arrays take the first six texture units; the forward variant's lighting has the rest (the skybox at 11, the
// sun's shadow at 12, the local shadow atlas at 13, the ambient LUT at 14, the cookies at 15 and the shadow table as
// resource buffer 1). The terrain's parameters take the material buffer slot, which its own pipeline leaves free (an
// item with a material binds its own there before it draws, so a terrain chunk's leftover never reaches one), and
// the chunk's own (where its tile is read) the last of the backend's fourteen slots (0 to 13), past the lighting's
// (the shadow parameters at 2, the lighting model at 3, the lights at 4 to 7, the clusters at 10 to 12). The slot
// after the parameters is for the draw that fills a slice of the arrays (terrain_layer_maps.slf, a pipeline of its
// own, which binds nothing else).
#define ENTITIES_TEXTURE_TERRAIN_LAYERS 0     // the colour array: a slice per slot, sRGB
#define ENTITIES_TEXTURE_TERRAIN_PAINT0 1
#define ENTITIES_TEXTURE_TERRAIN_PAINT1 2
#define ENTITIES_TEXTURE_TERRAIN_TOP 3        // the chunk column's heights of the ground the sky sees (TopSurface.h)
#define ENTITIES_TEXTURE_TERRAIN_NORMALS 4
#define ENTITIES_TEXTURE_TERRAIN_MASKS 5
#define ENTITIES_BUFFER_TERRAIN_LAYERS 1
#define ENTITIES_BUFFER_TERRAIN_CHUNK 13
#define ENTITIES_BUFFER_TERRAIN_MAPS 3

// the impostor card (ImpostorCard, IMPOSTORS.md 2.3): its atlases (the albedo with the coverage, the normal with the
// depth, and since step 4 the roughness and metallic) and its parameters
#define ENTITIES_TEXTURE_IMPOSTOR_ALBEDO 0
#define ENTITIES_TEXTURE_IMPOSTOR_NORMAL 1
#define ENTITIES_TEXTURE_IMPOSTOR_MATERIAL 2
#define ENTITIES_BUFFER_IMPOSTOR 1



// <!

namespace entities_renderer { namespace slot {

namespace texture {
enum Texture {
    MirrorPicture = ENTITIES_TEXTURE_MIRROR_PICTURE,
    TerrainLayers = ENTITIES_TEXTURE_TERRAIN_LAYERS,
    TerrainPaint0 = ENTITIES_TEXTURE_TERRAIN_PAINT0,
    TerrainTop = ENTITIES_TEXTURE_TERRAIN_TOP,
    TerrainNormals = ENTITIES_TEXTURE_TERRAIN_NORMALS,
    TerrainMasks = ENTITIES_TEXTURE_TERRAIN_MASKS,
    ImpostorAlbedo = ENTITIES_TEXTURE_IMPOSTOR_ALBEDO,
    ImpostorNormal = ENTITIES_TEXTURE_IMPOSTOR_NORMAL,
    ImpostorMaterial = ENTITIES_TEXTURE_IMPOSTOR_MATERIAL,
};
} // namespace texture

namespace buffer {
enum Buffer {
    Mirror = ENTITIES_BUFFER_MIRROR,
    TerrainLayers = ENTITIES_BUFFER_TERRAIN_LAYERS,
    TerrainChunk = ENTITIES_BUFFER_TERRAIN_CHUNK,
    TerrainMaps = ENTITIES_BUFFER_TERRAIN_MAPS,
    Impostor = ENTITIES_BUFFER_IMPOSTOR,
};
} // namespace buffer

} } // namespace entities::slot

// !>
// Hack Comment

#endif // ENTITIES_SHADER_CONSTANTS_H

// <@if 1@>
// Trigger Scribe include
// <@endif@> <!def that !>

// <@endif@>

// Hack Comment
