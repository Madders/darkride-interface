//
//  Layers.cpp
//  libraries/terrain/src/terrain
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Layers.h"

using namespace terrain;

bool Layer::operator==(const Layer& other) const {
    return slot == other.slot && name == other.name && materialURL == other.materialURL && color == other.color
        && tint == other.tint && tiling == other.tiling && offset == other.offset && opacity == other.opacity && blend == other.blend
        && visible == other.visible && normalStrength == other.normalStrength && normalURL == other.normalURL
        && roughnessURL == other.roughnessURL && occlusionURL == other.occlusionURL && heightURL == other.heightURL
        && useNormal == other.useNormal;
}

bool Layer::hasMaps() const {
    return !normalURL.empty() || !roughnessURL.empty() || !occlusionURL.empty() || !heightURL.empty() || !useNormal
        || blend == LayerBlend::Height;
}

bool LayerStack::isValid() const {
    if (layers.size() > (size_t)MAX_LAYERS) {
        return false;
    }
    bool taken[MAX_LAYERS] = { false };
    for (const Layer& layer : layers) {
        if (layer.slot >= MAX_LAYERS || taken[layer.slot]) {
            return false;
        }
        taken[layer.slot] = true;
    }
    return true;
}

const Layer* LayerStack::find(uint8_t slot) const {
    const int index = indexOf(slot);
    return index >= 0 ? &layers[index] : nullptr;
}

Layer* LayerStack::find(uint8_t slot) {
    const int index = indexOf(slot);
    return index >= 0 ? &layers[index] : nullptr;
}

int LayerStack::indexOf(uint8_t slot) const {
    for (size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].slot == slot) {
            return (int)i;
        }
    }
    return -1;
}

int LayerStack::freeSlot() const {
    for (int slot = 0; slot < MAX_LAYERS; ++slot) {
        if (indexOf((uint8_t)slot) < 0) {
            return slot;
        }
    }
    return -1;
}

std::vector<uint8_t> LayerStack::order() const {
    std::vector<uint8_t> slots;
    slots.reserve(layers.size());
    for (const Layer& layer : layers) {
        slots.push_back(layer.slot);
    }
    return slots;
}

LayerStack LayerStack::defaults() {
    struct Default {
        const char* name;
        const char* file;     // <file>.jpg is the colour, <file>-normal.jpg, -roughness.jpg, -occlusion.jpg, -height.jpg the maps
        glm::vec3 color;
        float tiling;
        bool occlusion;       // whether the set has an occlusion map
    };
    // the textures live in the client's resources (interface/resources/terrain/materials)
    static const Default DEFAULTS[MAX_LAYERS] = {
        { "Grass", "grass", glm::vec3(0.33f, 0.52f, 0.22f), 4.0f, true },
        { "Dirt", "dirt", glm::vec3(0.42f, 0.30f, 0.18f), 3.0f, true },
        { "Rock", "rock", glm::vec3(0.50f, 0.50f, 0.50f), 6.0f, true },
        { "Sand", "sand", glm::vec3(0.78f, 0.70f, 0.50f), 3.0f, true },
        { "Gravel", "gravel", glm::vec3(0.55f, 0.52f, 0.48f), 2.0f, true },
        { "Mud", "mud", glm::vec3(0.36f, 0.28f, 0.20f), 3.0f, true },
        { "Moss", "moss", glm::vec3(0.24f, 0.40f, 0.20f), 2.0f, true },
        { "Snow", "snow", glm::vec3(0.92f, 0.93f, 0.95f), 4.0f, false }
    };
    const std::string base = "qrc:///terrain/materials/";
    LayerStack stack;
    for (int slot = 0; slot < MAX_LAYERS; ++slot) {
        const Default& d = DEFAULTS[slot];
        Layer layer;
        layer.slot = (uint8_t)slot;
        layer.name = d.name;
        layer.materialURL = base + d.file + ".jpg";
        layer.normalURL = base + d.file + "-normal.jpg";
        layer.roughnessURL = base + d.file + "-roughness.jpg";
        layer.occlusionURL = d.occlusion ? base + d.file + "-occlusion.jpg" : std::string();
        layer.heightURL = base + d.file + "-height.jpg";
        layer.color = d.color;
        layer.tiling = d.tiling;
        stack.layers.push_back(layer);
    }
    return stack;
}

bool LayerStack::fillDefaultMaps() {
    static const LayerStack DEFAULTS = defaults();
    bool changed = false;
    for (Layer& layer : layers) {
        if (layer.materialURL.empty() || !layer.normalURL.empty() || !layer.roughnessURL.empty() || !layer.occlusionURL.empty()
            || !layer.heightURL.empty()) {
            continue;
        }
        for (const Layer& d : DEFAULTS.layers) {
            if (d.materialURL == layer.materialURL) {
                layer.normalURL = d.normalURL;
                layer.roughnessURL = d.roughnessURL;
                layer.occlusionURL = d.occlusionURL;
                layer.heightURL = d.heightURL;
                changed = true;
                break;
            }
        }
    }
    return changed;
}
