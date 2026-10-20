//
//  ReflectionProbe.h
//  libraries/graphics/src/graphics
//
//  A reflection probe (LIGHTMAPS.md §7): a cube map captured at a point, with the box it lights. Inside the box a
//  surface's ambient specular (its reflection of the surroundings) comes from the probe's cube map instead of the
//  zone's ambient map, fading to the zone's over the blend distance from the box's faces; with box projection the
//  reflection ray is intersected with the box and the cube map looked up toward the hit point, so a floor reflects the
//  ceiling from the right place. Several probes blend by importance (the most important first). The box is
//  axis-aligned in the probe's own frame (its orientation), offset from the probe's position by boxOffset.
//  ReflectionProbeStage (render-utils) holds the scene's probes and their cube map array.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_model_ReflectionProbe_h
#define hifi_model_ReflectionProbe_h

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <gpu/Texture.h>

namespace graphics {

class ReflectionProbe {
public:
    glm::vec3 position { 0.0f };
    glm::quat orientation { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::vec3 boxSize { 10.0f };            // the box's extent, metres, in the probe's frame
    glm::vec3 boxOffset { 0.0f };           // the box's centre from the probe's position, in the probe's frame
    float intensity { 1.0f };               // the cube map's scale
    int importance { 0 };                   // blending order: the most important probe takes a fragment first
    float blendDistance { 1.0f };           // metres inside the box over which the probe fades in
    bool boxProjection { true };

    // the probe's cube map, a prefiltered HDR cube texture (an ambient map's kind), once it has loaded
    gpu::TexturePointer cubemap;
    // bumped by whoever changes the probe, so the stage rebuilds what depends on it
    uint32_t stamp { 0 };

    void touch() { ++stamp; }
    // the sphere the box fits in, for the culling: its centre in the world, and its radius
    glm::vec3 getBoxCenter() const { return position + orientation * boxOffset; }
    float getBoxRadius() const { return 0.5f * glm::length(boxSize); }
    float getBoxVolume() const { return boxSize.x * boxSize.y * boxSize.z; }
};
using ReflectionProbePointer = std::shared_ptr<ReflectionProbe>;

}

#endif // hifi_model_ReflectionProbe_h
