//
//  RenderMirrorsTask.cpp
//  libraries/render-utils/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderMirrorsTask.h"

#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <gpu/Context.h>

#include "Mirrors.h"
#include "RenderViewTask.h"

using RenderArgsPointer = std::shared_ptr<RenderArgs>;

namespace {
    // Gives the frame's slots to the mirrors in view.
    class MirrorsSetup {
    public:
        using JobModel = render::Job::Model<MirrorsSetup>;

        void run(const render::RenderContextPointer& renderContext) {
            auto args = renderContext->args;
            if (args->isStereo() && args->_context) {
                // a headset (or a stereo monitor): the eyes, so a picture is sized to an eye's pixels
                Mirrors::Eyes eyes;
                args->_context->getStereoProjections(eyes.projection);
                args->_context->getStereoViews(eyes.view);
                Mirrors::instance().plan(args->getViewFrustum(), args->_viewport, &eyes);
            } else {
                Mirrors::instance().plan(args->getViewFrustum(), args->_viewport);
            }
        }
    };

    // A slot's turn begins: the frame draws into the slot's picture from the mirror's camera, the frame's own
    // settings kept for the end. An idle slot gets a view that takes in nothing, into one texel.
    class MirrorSlotBegin {
    public:
        using JobModel = render::Job::ModelO<MirrorSlotBegin, RenderArgsPointer>;

        MirrorSlotBegin(int slot) : _slot(slot), _cachedArgsPointer(std::make_shared<RenderArgs>()) {}

        void run(const render::RenderContextPointer& renderContext, RenderArgsPointer& cachedArgs) {
            auto args = renderContext->args;
            auto& mirrors = Mirrors::instance();
            const Mirrors::Slot& slot = mirrors.slot(_slot);

            _cachedArgsPointer->_blitFramebuffer = args->_blitFramebuffer;
            _cachedArgsPointer->_viewport = args->_viewport;
            _cachedArgsPointer->_displayMode = args->_displayMode;
            _cachedArgsPointer->_renderMode = args->_renderMode;
            _cachedArgsPointer->_stencilMaskMode = args->_stencilMaskMode;

            gpu::FramebufferPointer target = slot.active && slot.framebuffer ? slot.framebuffer : mirrors.idleFramebuffer();
            if (slot.active && slot.framebuffer) {
                // for Statistics: the mirrors drawn this frame and the pixels their views add up to
                args->_details._mirrorsDrawn++;
                args->_details._mirrorPixels += (int)target->getWidth() * (int)target->getHeight();
            }
            args->_blitFramebuffer = target;
            args->_viewport = glm::ivec4(0, 0, target->getWidth(), target->getHeight());
            args->_displayMode = RenderArgs::MONO;
            args->_renderMode = RenderArgs::RenderMode::SECONDARY_CAMERA_RENDER_MODE;
            args->_stencilMaskMode = StencilMaskMode::NONE;

            gpu::doInBatch("MirrorSlotBegin::run", args->_context, [&](gpu::Batch& batch) {
                batch.disableContextStereo();
                batch.disableContextViewCorrection();
            });

            ViewFrustum frustum;
            if (slot.active) {
                frustum = slot.frustum;
            } else {
                // far below everything, seeing a hand's width
                frustum.setPosition(glm::vec3(0.0f, -1.0e6f, 0.0f));
                frustum.setProjection(glm::perspective(glm::radians(1.0f), 1.0f, 0.1f, 0.2f));
                frustum.calculate();
            }
            args->pushViewFrustum(frustum);
            cachedArgs = _cachedArgsPointer;
        }

    private:
        int _slot;
        RenderArgsPointer _cachedArgsPointer;
    };

    // A slot's turn ends: the frame's own settings come back.
    class MirrorSlotEnd {
    public:
        using JobModel = render::Job::ModelI<MirrorSlotEnd, RenderArgsPointer>;


        void run(const render::RenderContextPointer& renderContext, const RenderArgsPointer& cachedArgs) {
            auto args = renderContext->args;
            if (cachedArgs) {
                args->_blitFramebuffer = cachedArgs->_blitFramebuffer;
                args->_viewport = cachedArgs->_viewport;
                args->_displayMode = cachedArgs->_displayMode;
                args->_renderMode = cachedArgs->_renderMode;
                args->_stencilMaskMode = cachedArgs->_stencilMaskMode;
            }
            args->popViewFrustum();

            gpu::doInBatch("MirrorSlotEnd::run", args->_context, [&](gpu::Batch& batch) {
                batch.restoreContextStereo();
                batch.restoreContextViewCorrection();
            });
        }
    };
}

void RenderMirrorsTask::build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor) {
    task.addJob<MirrorsSetup>("MirrorsSetup");
    for (int slot = 0; slot < Mirrors::SLOT_COUNT; ++slot) {
        std::string number = std::to_string(slot);
        const auto cachedArgs = task.addJob<MirrorSlotBegin>("MirrorSlot" + number, slot);
        // the secondary view's tag: entities visible to the secondary camera, not the editor's gizmos and grid
        task.addJob<RenderViewTask>("MirrorView" + number, cullFunctor, render::ItemKey::TAG_BITS_1, render::ItemKey::TAG_BITS_1);
        task.addJob<MirrorSlotEnd>("MirrorSlotEnd" + number, cachedArgs);
    }
}
