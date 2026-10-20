//
//  ImpostorBake.h
//  libraries/render-utils/src
//
//  The in-engine impostor bake (IMPOSTORS.md 2.2): a subject (the render items carrying the bake tag) drawn alone from
//  an orthographic view through the deferred opaque pass into a small G-buffer, resolved into three 8-bit pictures
//  (the albedo with its coverage; the object-space normal with the depth through the bounding sphere; the roughness
//  and metallic) and read back. One
//  frame is drawn per rendered frame; the editor's baker queues six of them for a model and composes the atlases.
//
//  Created by the editor session on 2026-10-18.
//

#ifndef hifi_ImpostorBake_h
#define hifi_ImpostorBake_h

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <QtGui/QImage>

#include <glm/glm.hpp>

#include <gpu/Framebuffer.h>
#include <render/Engine.h>
#include <render/CullTask.h>
#include <render/Item.h>
#include <ViewFrustum.h>

class ImpostorBake {
public:
    // the tag bit the subject's render items carry while they bake (the main view is bit 0, the secondary views bit 1)
    static const uint8_t TAG = render::ItemKey::TAG_BITS_7;

    // one orthographic view of the subject
    struct Frame {
        enum State { PENDING = 0, DRAWN = 1, DONE = 2 };

        ViewFrustum frustum;              // the camera: position, orientation and the plain orthographic projection
        glm::mat4 projection;             // the same projection, kept for the resolve (the backend reverses the depth)
        float nearClip { 0.0f };
        float farClip { 1.0f };
        glm::mat4 worldToObject;          // the rotation that takes a world normal into the subject's frame
        int size { 256 };
        gpu::FramebufferPointer albedoFramebuffer;
        gpu::FramebufferPointer normalFramebuffer;
        gpu::FramebufferPointer materialFramebuffer;
        QImage albedo;                    // read back: Format_ARGB32, the bottom row first as glReadPixels gives it
        QImage normal;
        QImage material;                  // r: roughness, g: metallic (step 4)
        std::atomic<int> state { PENDING };
    };
    using FramePointer = std::shared_ptr<Frame>;

    static ImpostorBake& instance();

    // queues frames; drawing starts after delayFrames rendered frames (the subject's new item keys take a frame or two
    // to reach the scene)
    void enqueue(const std::vector<FramePointer>& frames, int delayFrames);
    // the frame to draw this rendered frame, or null (called once per frame by the bake task)
    FramePointer begin();
    // a one-texel target for the frames with nothing to draw
    const gpu::FramebufferPointer& idleFramebuffer();
    bool isIdle();

private:
    std::mutex _mutex;
    std::vector<FramePointer> _queue;
    int _delay { 0 };
    gpu::FramebufferPointer _idle;
};

class RenderImpostorBakeTaskConfig : public render::Task::Config {
    Q_OBJECT
public:
    RenderImpostorBakeTaskConfig() : render::Task::Config(true) {}
};

class RenderImpostorBakeTask {
public:
    using Config = RenderImpostorBakeTaskConfig;
    using JobModel = render::Task::Model<RenderImpostorBakeTask, Config>;

    RenderImpostorBakeTask() {}
    void configure(const Config& config) {}
    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor);
};

#endif // hifi_ImpostorBake_h
