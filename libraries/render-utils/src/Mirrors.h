//
//  Mirrors.h
//  libraries/render-utils/src
//
//  The scene's mirrors: a shape whose userData has "mirror": true shows, on its local +Z face, what the viewer would
//  see reflected in it. RenderMirrorsTask draws that picture before the main view, from a camera behind the face at
//  the viewer's reflection, looking back through the face with a frustum fitted to the face's rectangle (so the
//  face itself is the near plane and nothing behind the mirror gets in). The mirror's material shows the picture
//  as an unlit albedo map, flipped the way a mirror flips. There are a few slots a frame; they go to the mirrors in
//  view that take the most of it, and a mirror keeps its slot while it is drawn. The picture is sized by the
//  mirror's size on screen, scaled by a setting, so a small mirror far away costs little.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Mirrors_h
#define hifi_Mirrors_h

#include <array>
#include <map>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QUuid>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <gpu/Framebuffer.h>
#include <gpu/Texture.h>
#include <ViewFrustum.h>

class Mirrors {
public:
    static const int SLOT_COUNT = 4;    // mirrors drawn in a frame at most

    // how a mirror shows its reflection
    enum class Mode {
        Picture,   // a picture of what the face frames, from a camera behind it, mapped onto the face
        Screen     // the main view reflected in the face's plane, drawn pixel for pixel where the face is on screen
    };

    // a mirror as its entity stands
    struct Surface {
        glm::vec3 position;      // world
        glm::quat orientation;   // world
        glm::vec3 dimensions;    // scaled: x and y the face, z the thickness
        Mode mode { Mode::Picture };
    };

    struct Slot {
        QUuid mirror;                          // the mirror it is drawing; null: idle
        bool active { false };                 // drawn this frame
        ViewFrustum frustum;                   // the camera behind the face, when active
        gpu::FramebufferPointer framebuffer;   // the picture
        // Screen mode: the part of the main view the picture covers, in the view's normalized device coordinates
        // (low corner xy, high corner zw); the glass reads the picture by its own position within it
        glm::vec4 region { -1.0f, -1.0f, 1.0f, 1.0f };
    };

    static Mirrors& instance();

    // from the entity renderers, on their thread
    void setSurface(const QUuid& id, const Surface& surface);
    void removeSurface(const QUuid& id);
    // the picture a mirror's material shows this frame (null: it has no slot), and a count that moves whenever the
    // picture changes (a slot taken or lost, a picture resized), so a renderer knows when to take it again
    gpu::TexturePointer textureFor(const QUuid& id, uint32_t& generation) const;
    uint32_t generation(const QUuid& id) const;
    // a Screen mirror's picture this frame and the part of the main view it covers (Slot::region); false when the
    // mirror has no slot this frame, and the glass shows its own colour
    bool screenFor(const QUuid& id, gpu::TexturePointer& picture, glm::vec4& region) const;

    // the render engine's config of a slot's view: switched off while the slot idles, so nothing is drawn for it
    void setSlotView(int slot, QObject* viewConfig) { _slotViews[slot] = viewConfig; }
    // a stereo display's eyes, as the gpu context holds them: each eye's projection and its view offset from the
    // camera; the two eyes sit side by side in the viewport, each in half its width
    struct Eyes {
        glm::mat4 projection[2];
        glm::mat4 view[2];
    };
    // from the render task, once a frame before the slots are drawn: gives the frame's slots to the mirrors in view
    // (the viewport is the main view's: origin and size; in stereo, the eyes, so a picture is sized to an eye's
    // pixels rather than the culling view's)
    void plan(const ViewFrustum& view, const glm::ivec4& viewport, const Eyes* eyes = nullptr);
    const Slot& slot(int index) const { return _slots[index]; }
    // where an idle slot's view goes: a few texels (the half-resolution passes need a size)
    const gpu::FramebufferPointer& idleFramebuffer();

    bool isEnabled() const { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }
    // the picture's texels per pixel of the mirror on screen: 1 (the default) is crisp, 0.5 costs a quarter
    float getResolutionScale() const { return _resolutionScale; }
    void setResolutionScale(float scale) { _resolutionScale = glm::clamp(scale, 0.125f, 2.0f); }
    int getMaxActive() const { return _maxActive; }
    void setMaxActive(int count) { _maxActive = glm::clamp(count, 0, SLOT_COUNT); }
    int getMaxPixels() const { return _maxPixels; }
    void setMaxPixels(int pixels) { _maxPixels = glm::clamp(pixels, 128, 4096); }

private:
    struct Entry {
        Surface surface;
        int slot { -1 };
        uint32_t generation { 1 };
    };
    // a mirror wanted this frame: its camera and what its picture should be
    struct Want {
        QUuid id;
        Mode mode { Mode::Picture };
        ViewFrustum camera;        // Picture: complete; Screen: the pose, its projection cropped once the slot is known
        glm::ivec2 pixels;         // Picture: the picture's size; Screen: the least the picture must be (the glass's
                                   // rectangle on screen, at the resolution scale)
        float area { 0.0f };       // the glass's area on screen, in pixels
        // Screen mode
        glm::ivec2 rectOrigin;     // the glass's rectangle on screen: its low corner, in pixels from the viewport's
        glm::vec4 plane;           // the glass's plane in the reflected camera's space, the viewer's side positive
    };
    // what a mirror seen from the view wants; false when it is out of view, edge-on, too small, or the viewer is
    // behind it
    bool cameraFor(const Surface& surface, const ViewFrustum& view, const glm::ivec4& viewport, const Eyes* eyes, Want& want) const;
    // Screen mode, once the slot's picture has its size: the picture covers the glass's rectangle and the pixels
    // past it up to the picture's size, at the screen's own pixel density; the camera's projection is cropped to
    // that region, and the region is kept for the glass
    void cropScreenCamera(Want& want, const glm::ivec2& pictureSize, const glm::ivec4& viewport, Slot& slot) const;

    mutable std::mutex _mutex;
    std::map<QUuid, Entry> _entries;
    std::array<Slot, SLOT_COUNT> _slots;
    std::array<QObject*, SLOT_COUNT> _slotViews {};
    gpu::FramebufferPointer _idle;
    bool _enabled { true };
    float _resolutionScale { 1.0f };   // a texel a pixel; 0.5 costs a quarter, for a cheaper mirror
    int _maxActive { SLOT_COUNT };
    int _maxPixels { 2048 };
};

#endif // hifi_Mirrors_h
