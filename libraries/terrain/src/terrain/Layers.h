//
//  Layers.h
//  libraries/terrain/src/terrain
//
//  A terrain's material layers (TERRAIN.md section 5): what its eight material indices look like, and how the paint
//  map's coverage of each is drawn. A layer stands for one slot, the voxel material index and the paint map plane of
//  that number, for as long as it exists; the stack lists the layers bottom to top in drawing order, and reordering
//  or renaming them changes nothing in the voxels or the map. Wherever the ground faces up the shader draws the
//  stack over the voxel material as a painter would: each layer's painted coverage, times its opacity, lays its
//  colour over what is below by its blend mode, so painting a layer covers without taking anything from the layers
//  under it, and a layer turned down or off shows them again. Besides its colour texture a layer may have a normal
//  map, a roughness map, an occlusion map and a height map (one channel each but the normal), tiled and projected
//  as the colour is; the Height blend lets the higher of two layers show through the lower where they meet. The
//  stack is kept in the terrain file with the chunks and the map (TerrainFile.h, version 4, version 5 with the
//  maps); a terrain without one draws the default stack.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_terrain_Layers_h
#define hifi_terrain_Layers_h

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace terrain {

const int MAX_LAYERS = 8;   // the slots: a voxel material index, a paint map plane

// How a layer's colour meets what is drawn below it where it is painted.
enum class LayerBlend : uint8_t {
    Normal = 0,     // covers
    Multiply = 1,   // darkens: the product
    Screen = 2,     // lightens: one less the product of the complements
    Overlay = 3,    // contrast: multiply where the ground is dark, screen where it is light
    Height = 4      // by height: where the layer's height map plus its coverage rises above what is below it, it shows,
                    // with a soft edge (a height lerp, as Unity's and Unreal's terrain shaders blend)
};
const int LAYER_BLEND_COUNT = 5;

struct Layer {
    uint8_t slot { 0 };                     // which material index and paint plane this is, 0 to 7
    std::string name;
    std::string materialURL;                // the colour texture; empty draws the plain colour
    glm::vec3 color { 0.5f, 0.5f, 0.5f };   // the plain colour, drawn without a texture or until it is in
    glm::vec3 tint { 1.0f, 1.0f, 1.0f };    // multiplies the texture
    float tiling { 4.0f };                  // metres per repeat of the texture
    glm::vec2 offset { 0.0f, 0.0f };        // metres the texture is shifted along the terrain's x and z
    float opacity { 1.0f };                 // 0 to 1: how much of its painted coverage shows
    LayerBlend blend { LayerBlend::Normal };
    bool visible { true };                  // off: its paint is not drawn (the voxel material still is)
    std::string normalURL;                  // the normal map, tangent space with green up (OpenGL's way); empty draws none
    std::string roughnessURL;               // one channel, 0 smooth to 1 rough; empty is the terrain's plain roughness
    std::string occlusionURL;               // one channel, 0 shaded to 1 open; empty is none
    std::string heightURL;                  // one channel, 0 low to 1 high, read by the Height blend; empty is half height
    bool useNormal { true };                // off: the normal map is not drawn, the ground's own normal is
    float normalStrength { 1.0f };          // scales the normal map's tilt: 0 is flat, 1 as drawn, more exaggerates

    // Whether the layer uses anything the terrain file's version 4 cannot hold: a map, the normal map turned off, the
    // Height blend.
    bool hasMaps() const;

    bool operator==(const Layer& other) const;
    bool operator!=(const Layer& other) const { return !(*this == other); }
};

// The layers bottom to top in drawing order, each slot at most once.
struct LayerStack {
    std::vector<Layer> layers;

    bool isEmpty() const { return layers.empty(); }
    // Whether it is well formed: at most MAX_LAYERS layers, slots below MAX_LAYERS and no slot twice.
    bool isValid() const;
    // The layer of a slot, or null; its index in the stack, or -1.
    const Layer* find(uint8_t slot) const;
    Layer* find(uint8_t slot);
    int indexOf(uint8_t slot) const;
    // The lowest slot no layer has, or -1 with all eight taken.
    int freeSlot() const;
    // The slots bottom to top.
    std::vector<uint8_t> order() const;

    bool operator==(const LayerStack& other) const { return layers == other.layers; }
    bool operator!=(const LayerStack& other) const { return !(*this == other); }

    // The eight layers every terrain starts with, in slot order: grass, dirt, rock, sand, gravel, mud, moss and
    // snow, each with a colour texture, normal, roughness, occlusion (but snow) and height maps the client carries
    // (CC0, from ambientCG).
    static LayerStack defaults();
    // Gives every layer whose colour texture is one of the defaults', and which has no maps, that default's maps: for a
    // stack from before the maps (a version 4 file, or the entity's terrainLayers text), so it looks as a new terrain
    // does. Returns whether anything changed.
    bool fillDefaultMaps();
};

} // namespace terrain

#endif // hifi_terrain_Layers_h
