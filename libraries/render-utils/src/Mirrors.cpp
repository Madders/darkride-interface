//
//  Mirrors.cpp
//  libraries/render-utils/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Mirrors.h"

#include "RenderUtilsLogging.h"

#include <algorithm>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace {
    // the projection with its near plane moved onto a plane given in camera space (Lengyel's oblique near plane):
    // the camera lies on the plane's negative side, and only the positive side is drawn
    glm::mat4 obliqueProjection(const glm::mat4& projection, const glm::vec4& plane) {
        glm::vec4 q = glm::inverse(projection) * glm::vec4(glm::sign(plane.x), glm::sign(plane.y), 1.0f, 1.0f);
        glm::vec4 c = plane * (2.0f / glm::dot(plane, q));
        glm::mat4 result = projection;
        result[0][2] = c.x;
        result[1][2] = c.y;
        result[2][2] = c.z + 1.0f;
        result[3][2] = c.w;
        return result;
    }
}

Mirrors& Mirrors::instance() {
    static Mirrors mirrors;
    return mirrors;
}

void Mirrors::setSurface(const QUuid& id, const Surface& surface) {
    std::lock_guard<std::mutex> lock(_mutex);
    _entries[id].surface = surface;
}

void Mirrors::removeSurface(const QUuid& id) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _entries.find(id);
    if (found == _entries.end()) {
        return;
    }
    if (found->second.slot >= 0 && _slots[found->second.slot].mirror == id) {
        _slots[found->second.slot].mirror = QUuid();
        _slots[found->second.slot].active = false;
    }
    _entries.erase(found);
}

gpu::TexturePointer Mirrors::textureFor(const QUuid& id, uint32_t& generation) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _entries.find(id);
    if (found == _entries.end()) {
        generation = 0;
        return nullptr;
    }
    generation = found->second.generation;
    int slot = found->second.slot;
    if (slot < 0 || _slots[slot].mirror != id || !_slots[slot].framebuffer) {
        return nullptr;
    }
    return _slots[slot].framebuffer->getRenderBuffer(0);
}

uint32_t Mirrors::generation(const QUuid& id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _entries.find(id);
    return found == _entries.end() ? 0 : found->second.generation;
}

bool Mirrors::screenFor(const QUuid& id, gpu::TexturePointer& picture, glm::vec4& region) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _entries.find(id);
    if (found == _entries.end() || found->second.surface.mode != Mode::Screen) {
        return false;
    }
    int slot = found->second.slot;
    if (slot < 0 || _slots[slot].mirror != id || !_slots[slot].active || !_slots[slot].framebuffer) {
        return false;
    }
    picture = _slots[slot].framebuffer->getRenderBuffer(0);
    region = _slots[slot].region;
    return true;
}

const gpu::FramebufferPointer& Mirrors::idleFramebuffer() {
    if (!_idle) {
        _idle = gpu::FramebufferPointer(gpu::Framebuffer::create("mirrorIdle", gpu::Element::COLOR_SRGBA_32, 8, 8));
    }
    return _idle;
}

namespace {
    // a mirror smaller than this on screen (in pixels of area) shows its colour: a view's fixed cost (the sun's
    // shadow maps, the lighting passes) is not worth a thumbnail
    const float MIN_AREA = 32.0f * 32.0f;

    // the face's rectangle in a view, in whole pixels of the view's viewport from its low corner, clipped to the
    // view; a corner behind the eye makes it the whole view. Returns the rectangle's size; pixelLow its low corner.
    glm::ivec2 rectInView(const glm::mat4& clipFromMirror, const glm::vec3 corners[4], const glm::ivec2& viewportSize,
                          glm::ivec2& pixelLow) {
        glm::vec2 low(1.0f);
        glm::vec2 high(-1.0f);
        for (int i = 0; i < 4; ++i) {
            glm::vec4 clip = clipFromMirror * glm::vec4(corners[i], 1.0f);
            if (clip.w <= 0.001f) {
                low = glm::vec2(-1.0f);
                high = glm::vec2(1.0f);
                break;
            }
            glm::vec2 ndc = glm::clamp(glm::vec2(clip) / clip.w, glm::vec2(-1.0f), glm::vec2(1.0f));
            low = glm::min(low, ndc);
            high = glm::max(high, ndc);
        }
        pixelLow = glm::clamp(glm::ivec2(glm::floor((low + 1.0f) * 0.5f * glm::vec2(viewportSize))), glm::ivec2(0), viewportSize);
        glm::ivec2 pixelHigh = glm::clamp(glm::ivec2(glm::ceil((high + 1.0f) * 0.5f * glm::vec2(viewportSize))), pixelLow, viewportSize);
        return pixelHigh - pixelLow;
    }
}

bool Mirrors::cameraFor(const Surface& surface, const ViewFrustum& view, const glm::ivec4& viewport, const Eyes* eyes, Want& want) const {
    glm::vec2 half(surface.dimensions.x * 0.5f, surface.dimensions.y * 0.5f);
    float front = surface.dimensions.z * 0.5f;   // the face's distance from the entity's centre, along +Z
    if (half.x <= 0.0f || half.y <= 0.0f) {
        return false;
    }
    glm::mat4 worldFromMirror = glm::translate(surface.position) * glm::mat4_cast(surface.orientation);
    glm::mat4 mirrorFromWorld = glm::inverse(worldFromMirror);
    glm::vec3 viewer = glm::vec3(mirrorFromWorld * glm::vec4(view.getPosition(), 1.0f));
    float distance = viewer.z - front;   // the viewer's distance in front of the face
    if (distance < 0.01f) {
        return false;   // behind the mirror, or in the glass
    }

    // in view at all?
    glm::vec3 center = glm::vec3(worldFromMirror * glm::vec4(0.0f, 0.0f, front, 1.0f));
    if (!view.sphereIntersectsFrustum(center, glm::length(half))) {
        return false;
    }

    // its rectangle on screen: the face's corners projected, clipped to the view, in whole pixels from the
    // viewport's low corner
    glm::mat4 worldFromCamera = glm::translate(view.getPosition()) * glm::mat4_cast(view.getOrientation());
    glm::vec3 corners[4] = { { -half.x, -half.y, front }, { half.x, -half.y, front }, { half.x, half.y, front }, { -half.x, half.y, front } };
    glm::ivec2 pixelLow;
    glm::ivec2 rectPixels = rectInView(view.getProjection() * glm::inverse(worldFromCamera) * worldFromMirror, corners,
                                       glm::ivec2(viewport.z, viewport.w), pixelLow);
    if (eyes && surface.mode != Mode::Screen) {
        // a stereo display: the view is the culling one, wider than an eye and drawn across both eyes' halves of the
        // viewport, so its pixels are not the eyes'. The picture is sized to the glass's rectangle in the eye that
        // shows it largest (each eye: its own projection, its offset from the camera, half the viewport's width), a
        // texel an eye's pixel. Sized in the culling view, a headset's picture was about half as wide again as the
        // glass in an eye and a twelfth short in height (Quest 3, 2026-10-29): rows soft, columns shimmering, and a
        // third more pixels drawn than shown.
        glm::ivec2 eyeSize(viewport.z / 2, viewport.w);
        rectPixels = glm::ivec2(0);
        for (int eye = 0; eye < 2; ++eye) {
            // the eye sits at its offset from the camera, in the camera's own axes (the backend's getEyeCamera)
            glm::mat4 worldFromEye = worldFromCamera * glm::translate(-glm::vec3(eyes->view[eye][3]));
            glm::ivec2 eyeLow;
            rectPixels = glm::max(rectPixels, rectInView(eyes->projection[eye] * glm::inverse(worldFromEye) * worldFromMirror,
                                                         corners, eyeSize, eyeLow));
        }
    }
    want.area = (float)rectPixels.x * (float)rectPixels.y;
    if (want.area < MIN_AREA) {
        return false;
    }
    want.mode = surface.mode;

    if (surface.mode == Mode::Screen) {
        // the main view reflected in the face's plane, a pixel a pixel: the picture covers the glass's rectangle
        // on screen (cropScreenCamera() fits the projection once the picture has its size). The reflected camera's
        // axes are reflected too, which makes a left-handed camera; its right is turned round to keep a proper
        // one, and the glass flips the picture back across when it shows it. The near plane is moved onto the
        // glass, so nothing behind the mirror is drawn.
        glm::vec3 normal = surface.orientation * glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 onFace = surface.position + normal * front;
        auto reflectPoint = [&](const glm::vec3& p) { return p - 2.0f * glm::dot(p - onFace, normal) * normal; };
        auto reflectDirection = [&](const glm::vec3& d) { return d - 2.0f * glm::dot(d, normal) * normal; };
        glm::vec3 forward = reflectDirection(view.getDirection());
        glm::vec3 up = reflectDirection(view.getUp());
        glm::vec3 right = glm::cross(forward, up);
        ViewFrustum& camera = want.camera;
        camera.setPosition(reflectPoint(view.getPosition()));
        camera.setOrientation(glm::quat_cast(glm::mat3(right, up, -forward)));
        glm::mat4 reflectedFromWorld = glm::inverse(glm::translate(camera.getPosition()) * glm::mat4_cast(camera.getOrientation()));
        glm::vec3 normalInCamera = glm::normalize(glm::mat3(reflectedFromWorld) * normal);
        glm::vec3 onFaceInCamera = glm::vec3(reflectedFromWorld * glm::vec4(onFace, 1.0f));
        want.plane = glm::vec4(normalInCamera, -glm::dot(normalInCamera, onFaceInCamera));   // the viewer's side is positive
        camera.setProjection(view.getProjection());   // the view's own, to crop
        want.rectOrigin = pixelLow;
        want.pixels = glm::max(glm::ivec2(glm::ceil(glm::vec2(rectPixels) * _resolutionScale)), glm::ivec2(1));
        return true;
    }

    // the picture: the mirror's size on screen (scaled), a texel a pixel, within bounds
    glm::vec2 wanted = glm::clamp(glm::vec2(rectPixels) * _resolutionScale, glm::vec2(32.0f), glm::vec2((float)_maxPixels));
    want.pixels = glm::ivec2(glm::round(wanted));

    // the camera: the viewer reflected in the face, looking back through it, with the face's rectangle as its near
    // plane; it is turned half round so that its right is the face's -X, which keeps the image a proper one (no
    // mirrored winding to draw with); the material flips it across to show it as a mirror does
    ViewFrustum& camera = want.camera;
    glm::vec3 eye(viewer.x, viewer.y, 2.0f * front - viewer.z);
    camera.setPosition(glm::vec3(worldFromMirror * glm::vec4(eye, 1.0f)));
    camera.setOrientation(surface.orientation * glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));
    // the near plane a hair past the glass, so the face itself (which lies exactly at the distance) is clipped
    // away rather than drawn over the whole picture; the rectangle scales with it
    float near = distance * 1.002f + 0.002f;
    float scale = near / distance;
    camera.setProjection(glm::frustum((eye.x - half.x) * scale, (eye.x + half.x) * scale, (-half.y - eye.y) * scale,
                                      (half.y - eye.y) * scale, near, view.getFarClip()));
    camera.calculate();
    return true;
}

void Mirrors::cropScreenCamera(Want& want, const glm::ivec2& pictureSize, const glm::ivec4& viewport, Slot& slot) const {
    // the region of the screen the picture covers: from the glass's low corner, the picture's size at the screen's
    // own pixel density (so it stays a pixel a pixel, and may reach past the glass and past the screen's edge)
    glm::vec2 viewportSize(viewport.z, viewport.w);
    glm::vec2 regionPixels = glm::vec2(pictureSize) / _resolutionScale;
    glm::vec2 low = glm::vec2(want.rectOrigin) / viewportSize * 2.0f - 1.0f;
    glm::vec2 high = (glm::vec2(want.rectOrigin) + regionPixels) / viewportSize * 2.0f - 1.0f;
    slot.region = glm::vec4(low, high);

    // the crop maps the region onto the whole clip square, in x and y only (the w row is untouched, so the oblique
    // near plane applies as before). A point on the glass lands in the reflected camera at the same y and the
    // opposite x (its right was turned round), so the region it sees is the mirror image: [-high.x, -low.x].
    glm::vec2 scale = 2.0f / glm::max(high - low, glm::vec2(1.0e-4f));
    glm::vec2 center = (low + high) * 0.5f;
    glm::mat4 crop(1.0f);
    crop[0][0] = scale.x;
    crop[1][1] = scale.y;
    crop[3][0] = center.x * scale.x;    // x' = (x + center.x) * scale.x
    crop[3][1] = -center.y * scale.y;   // y' = (y - center.y) * scale.y
    glm::mat4 projection = crop * want.camera.getProjection();
    // the planes, near and far from the cropped projection (a sub-frustum, so culling is tighter too; the oblique
    // one's far corners land behind the eye); the oblique projection only to draw with
    want.camera.setProjection(projection);
    want.camera.calculate();
    want.camera.setObliqueProjection(obliqueProjection(projection, want.plane));
}

void Mirrors::plan(const ViewFrustum& view, const glm::ivec4& viewport, const Eyes* eyes) {
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<Want> wants;
    if (_enabled && _maxActive > 0 && viewport.z > 0 && viewport.w > 0) {
        for (const auto& entry : _entries) {
            Want want;
            if (cameraFor(entry.second.surface, view, viewport, eyes, want)) {
                want.id = entry.first;
                wants.push_back(want);
            }
        }
    }
    std::sort(wants.begin(), wants.end(), [](const Want& a, const Want& b) { return a.area > b.area; });
    if ((int)wants.size() > _maxActive) {
        wants.resize(_maxActive);
    }

    // a mirror keeps the slot it had; the slots of mirrors no longer wanted free up
    std::array<bool, SLOT_COUNT> taken {};
    for (const auto& want : wants) {
        int slot = _entries[want.id].slot;
        if (slot >= 0 && _slots[slot].mirror == want.id) {
            taken[slot] = true;
        }
    }
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (taken[i]) {
            continue;
        }
        if (!_slots[i].mirror.isNull()) {
            auto found = _entries.find(_slots[i].mirror);
            if (found != _entries.end() && found->second.slot == i) {
                found->second.slot = -1;
                ++found->second.generation;
            }
            _slots[i].mirror = QUuid();
        }
        _slots[i].active = false;
    }
    for (auto& want : wants) {
        Entry& entry = _entries[want.id];
        if (entry.slot < 0 || _slots[entry.slot].mirror != want.id) {
            int free = (int)(std::find(taken.begin(), taken.end(), false) - taken.begin());
            if (free >= SLOT_COUNT) {
                continue;
            }
            taken[free] = true;
            entry.slot = free;
            ++entry.generation;
            _slots[free].mirror = want.id;
        }
        Slot& slot = _slots[entry.slot];
        slot.active = true;

        // the picture's size. Picture mode: made again only when its size on screen has moved by more than a tenth
        // either way, so a slow approach doesn't make it every frame; in between, the texel stays near a pixel.
        // Screen mode: it must hold the glass's rectangle, so it is made again when the rectangle outgrows it, or
        // shrinks below seven tenths of its area, each time with a tenth to spare
        glm::ivec2 size = want.pixels;
        bool resize = !slot.framebuffer;
        if (slot.framebuffer) {
            glm::ivec2 have(slot.framebuffer->getWidth(), slot.framebuffer->getHeight());
            if (want.mode == Mode::Screen) {
                resize = have.x < size.x || have.y < size.y || (float)have.x * (float)have.y * 0.7f > (float)size.x * (float)size.y;
            } else {
                resize = glm::abs(size.x - have.x) > 0.1f * have.x || glm::abs(size.y - have.y) > 0.1f * have.y;
            }
        }
        if (resize) {
            if (want.mode == Mode::Screen) {
                glm::ivec2 most = glm::max(glm::ivec2(glm::ceil(glm::vec2(viewport.z, viewport.w) * _resolutionScale)), glm::ivec2(1));
                size = glm::min(glm::ivec2(glm::ceil(glm::vec2(size) * 1.1f)), most);
            }
            // sampled linearly: a Picture is often smaller than the mirror on screen, and point sampling would
            // show it blocky; a Screen picture at the screen's density reads texel for pixel either way
            auto picture = gpu::TexturePointer(gpu::Texture::createRenderBuffer(gpu::Element::COLOR_SRGBA_32, size.x, size.y,
                gpu::Texture::SINGLE_MIP, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_LINEAR, gpu::Sampler::WRAP_CLAMP)));
            picture->setSource("mirror" + std::to_string(entry.slot));
            slot.framebuffer = gpu::FramebufferPointer(gpu::Framebuffer::create("mirror" + std::to_string(entry.slot)));
            slot.framebuffer->setRenderBuffer(0, picture);
            ++entry.generation;
            qCDebug(renderutils) << "Mirror" << want.id << "in slot" << entry.slot << (want.mode == Mode::Screen ? "(screen)" : "(picture)")
                                 << "draws" << size.x << "x" << size.y << "for" << (int)want.area << "pixels on screen; camera at"
                                 << want.camera.getPosition();
        }
        if (want.mode == Mode::Screen) {
            cropScreenCamera(want, glm::ivec2(slot.framebuffer->getWidth(), slot.framebuffer->getHeight()), viewport, slot);
        }
        slot.frustum = want.camera;
    }
    // only the slots in use draw their view
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (_slotViews[i] && _slotViews[i]->property("enabled").toBool() != _slots[i].active) {
            _slotViews[i]->setProperty("enabled", _slots[i].active);
        }
    }
}
