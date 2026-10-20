//
//  ImpostorBake.cpp
//  libraries/render-utils/src
//
//  Created by the editor session on 2026-10-18. See ImpostorBake.h and IMPOSTORS.md 2.2.
//

#include "ImpostorBake.h"

#include <DependencyManager.h>
#include <gpu/Context.h>
#include <render/RenderFetchCullSortTask.h>
#include <shaders/Shaders.h>

#include "render-utils/ShaderConstants.h"
#include "DeferredFramebuffer.h"
#include "DeferredLightingEffect.h"
#include "FadeEffect.h"
#include "LightingModel.h"
#include "RenderCommonTask.h"
#include "RenderDeferredTask.h"
#include "StencilMaskPass.h"

#include <FrameDiag.h>

extern void initDeferredPipelines(render::ShapePlumber& plumber, const render::ShapePipeline::BatchSetter& batchSetter, const render::ShapePipeline::ItemSetter& itemSetter);

using namespace render;
using RenderArgsPointer = std::shared_ptr<RenderArgs>;

namespace ru {
    using render_utils::slot::texture::Texture;
    using render_utils::slot::buffer::Buffer;
}

const uint8_t ImpostorBake::TAG;

ImpostorBake& ImpostorBake::instance() {
    static ImpostorBake bake;
    return bake;
}

void ImpostorBake::enqueue(const std::vector<FramePointer>& frames, int delayFrames) {
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.insert(_queue.end(), frames.begin(), frames.end());
    _delay = std::max(_delay, delayFrames);
}

ImpostorBake::FramePointer ImpostorBake::begin() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_queue.empty()) {
        return nullptr;
    }
    if (_delay > 0) {
        _delay--;
        return nullptr;
    }
    FramePointer frame = _queue.front();
    _queue.erase(_queue.begin());
    frame->state = Frame::DRAWN;
    return frame;
}

bool ImpostorBake::isIdle() {
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.empty();
}

const gpu::FramebufferPointer& ImpostorBake::idleFramebuffer() {
    if (!_idle) {
        _idle = gpu::FramebufferPointer(gpu::Framebuffer::create("impostorBakeIdle", gpu::Element::COLOR_SRGBA_32, 1, 1));
    }
    return _idle;
}

namespace {
    // The frame's turn begins: the frame draws into the bake's target from the frame's camera, the frame's own
    // settings kept for the end. With nothing queued the view takes in nothing, into one texel.
    class BakeBegin {
    public:
        using Outputs = render::VaryingSet2<RenderArgsPointer, ImpostorBake::FramePointer>;
        using JobModel = render::Job::ModelO<BakeBegin, Outputs>;

        BakeBegin() : _cachedArgsPointer(std::make_shared<RenderArgs>()) {}

        void run(const render::RenderContextPointer& renderContext, Outputs& outputs) {
            auto args = renderContext->args;
            auto frame = ImpostorBake::instance().begin();
            FrameDiag::bake() = frame ? 1 : 0;

            _cachedArgsPointer->_blitFramebuffer = args->_blitFramebuffer;
            _cachedArgsPointer->_viewport = args->_viewport;
            _cachedArgsPointer->_displayMode = args->_displayMode;
            _cachedArgsPointer->_renderMode = args->_renderMode;
            _cachedArgsPointer->_stencilMaskMode = args->_stencilMaskMode;
            _cachedArgsPointer->_impostorAngleHalfTanSq = args->_impostorAngleHalfTanSq;
            _cachedArgsPointer->_meshLodErrorPixels = args->_meshLodErrorPixels;
            _cachedArgsPointer->_meshLodForceLevel = args->_meshLodForceLevel;

            gpu::FramebufferPointer target = frame ? frame->albedoFramebuffer : ImpostorBake::instance().idleFramebuffer();
            args->_blitFramebuffer = target;
            args->_viewport = glm::ivec4(0, 0, target->getWidth(), target->getHeight());
            args->_displayMode = RenderArgs::MONO;
            args->_renderMode = RenderArgs::RenderMode::SECONDARY_CAMERA_RENDER_MODE;
            args->_stencilMaskMode = StencilMaskMode::NONE;
            // the subject's models, never their card (ImpostorSwitch), whatever the frame's camera subtends
            args->_impostorAngleHalfTanSq = 0.0f;
            // and their full meshes, never their automatic LODs
            args->_meshLodErrorPixels = 0.0f;
            args->_meshLodForceLevel = -1;

            gpu::doInBatch("BakeBegin::run", args->_context, [&](gpu::Batch& batch) {
                batch.disableContextStereo();
                batch.disableContextViewCorrection();
            });

            ViewFrustum frustum;
            if (frame) {
                frustum = frame->frustum;
            } else {
                // far below everything, seeing a hand's width
                frustum.setPosition(glm::vec3(0.0f, -1.0e6f, 0.0f));
                frustum.setProjection(glm::perspective(glm::radians(1.0f), 1.0f, 0.1f, 0.2f));
                frustum.calculate();
            }
            args->pushViewFrustum(frustum);
            outputs.edit0() = _cachedArgsPointer;
            outputs.edit1() = frame;
        }

    private:
        RenderArgsPointer _cachedArgsPointer;
    };

    // The subject's G-buffer, unpacked into the frame's three pictures and read back at the end of the batch (the
    // read runs on the backend's thread, right after the draws that filled the pictures).
    class BakeResolve {
    public:
        using Inputs = render::VaryingSet2<DeferredFramebufferPointer, ImpostorBake::FramePointer>;
        using JobModel = render::Job::ModelI<BakeResolve, Inputs>;

        struct Params {
            glm::mat4 invProjection;        // the inverse of the reversed-depth projection the backend drew with
            glm::mat4 worldToObject;
            glm::vec4 nearFarZeroToOneMode; // near, far, 1 when the clip depth runs 0..1, the layer (0 albedo, 1 normal, 2 material)
        };

        void run(const render::RenderContextPointer& renderContext, const Inputs& inputs) {
            const auto& deferredFramebuffer = inputs.get0();
            const auto& frame = inputs.get1();
            if (!frame || !deferredFramebuffer) {
                return;
            }
            auto args = renderContext->args;
            if (!_pipeline) {
                auto program = gpu::Shader::createProgram(shader::render_utils::program::impostor_bake_resolve);
                auto state = std::make_shared<gpu::State>();
                state->setDepthTest(false, false, gpu::LESS_EQUAL);
                state->setColorWriteMask(true, true, true, true);
                _pipeline = gpu::Pipeline::create(program, state);
                _params = std::make_shared<gpu::Buffer>(sizeof(Params), nullptr);
                _params2 = std::make_shared<gpu::Buffer>(sizeof(Params), nullptr);
                _params3 = std::make_shared<gpu::Buffer>(sizeof(Params), nullptr);
            }
            Params params;
            params.invProjection = glm::inverse(gpu::Backend::reversedZProjection(frame->projection));
            params.worldToObject = frame->worldToObject;
            params.nearFarZeroToOneMode = glm::vec4(frame->nearClip, frame->farClip, gpu::Backend::depthClipZeroToOne ? 1.0f : 0.0f, 0.0f);
            _params->setSubData(0, params);
            params.nearFarZeroToOneMode.w = 1.0f;
            _params2->setSubData(0, params);
            params.nearFarZeroToOneMode.w = 2.0f;
            _params3->setSubData(0, params);

            const int size = frame->size;
            const glm::ivec4 viewport(0, 0, size, size);
            gpu::doInBatch("BakeResolve::run", args->_context, [&](gpu::Batch& batch) {
                batch.enableStereo(false);
                batch.setViewportTransform(viewport);
                batch.setStateScissorRect(viewport);
                batch.setProjectionTransform(glm::mat4());
                batch.resetViewTransform();
                batch.setModelTransform(Transform());
                batch.setPipeline(_pipeline);
                batch.setResourceTexture(ru::Texture::DeferredColor, deferredFramebuffer->getDeferredColorTexture());
                batch.setResourceTexture(ru::Texture::DeferredNormal, deferredFramebuffer->getDeferredNormalTexture());
                batch.setResourceTexture(ru::Texture::DeferredSpecular, deferredFramebuffer->getDeferredSpecularTexture());
                batch.setResourceTexture(ru::Texture::DeferredDepth, deferredFramebuffer->getPrimaryDepthTexture());

                batch.setFramebuffer(frame->albedoFramebuffer);
                batch.setUniformBuffer(ru::Buffer::DebugDeferredParams, _params);
                batch.draw(gpu::TRIANGLE_STRIP, 4);
                batch.setFramebuffer(frame->normalFramebuffer);
                batch.setUniformBuffer(ru::Buffer::DebugDeferredParams, _params2);
                batch.draw(gpu::TRIANGLE_STRIP, 4);
                batch.setFramebuffer(frame->materialFramebuffer);
                batch.setUniformBuffer(ru::Buffer::DebugDeferredParams, _params3);
                batch.draw(gpu::TRIANGLE_STRIP, 4);

                batch.setResourceTexture(ru::Texture::DeferredColor, nullptr);
                batch.setResourceTexture(ru::Texture::DeferredNormal, nullptr);
                batch.setResourceTexture(ru::Texture::DeferredSpecular, nullptr);
                batch.setResourceTexture(ru::Texture::DeferredDepth, nullptr);

                auto context = args->_context;
                batch.runLambda([frame, context, size] {
                    gpu::Vec4i region(0, 0, size, size);
                    context->downloadFramebuffer(frame->albedoFramebuffer, region, frame->albedo);
                    context->downloadFramebuffer(frame->normalFramebuffer, region, frame->normal);
                    context->downloadFramebuffer(frame->materialFramebuffer, region, frame->material);
                    frame->state = ImpostorBake::Frame::DONE;
                });
            });
        }

    private:
        gpu::PipelinePointer _pipeline;
        gpu::BufferPointer _params;
        gpu::BufferPointer _params2;
        gpu::BufferPointer _params3;
    };

    // The frame's turn ends: the frame's own settings come back.
    class BakeEnd {
    public:
        using JobModel = render::Job::ModelI<BakeEnd, RenderArgsPointer>;

        void run(const render::RenderContextPointer& renderContext, const RenderArgsPointer& cachedArgs) {
            auto args = renderContext->args;
            if (cachedArgs) {
                args->_blitFramebuffer = cachedArgs->_blitFramebuffer;
                args->_viewport = cachedArgs->_viewport;
                args->_displayMode = cachedArgs->_displayMode;
                args->_renderMode = cachedArgs->_renderMode;
                args->_stencilMaskMode = cachedArgs->_stencilMaskMode;
                args->_impostorAngleHalfTanSq = cachedArgs->_impostorAngleHalfTanSq;
                args->_meshLodErrorPixels = cachedArgs->_meshLodErrorPixels;
                args->_meshLodForceLevel = cachedArgs->_meshLodForceLevel;
            }
            args->popViewFrustum();

            gpu::doInBatch("BakeEnd::run", args->_context, [&](gpu::Batch& batch) {
                batch.restoreContextStereo();
                batch.restoreContextViewCorrection();
            });
        }
    };
}

void RenderImpostorBakeTask::build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor) {
    const auto beginOutputs = task.addJob<BakeBegin>("ImpostorBakeBegin");
    const auto cachedArgs = beginOutputs.getN<BakeBegin::Outputs>(0);
    const auto frame = beginOutputs.getN<BakeBegin::Outputs>(1);

    // the deferred opaque pass alone, on the items carrying the bake tag: the G-buffer is all the bake wants
    task.addJob<SetRenderMethod>("ImpostorBakeMethod", render::Args::DEFERRED);
    const auto fetched = task.addJob<RenderFetchCullSortTask>("ImpostorBakeFetch", cullFunctor, ImpostorBake::TAG, ImpostorBake::TAG);
    const auto& fetchedItems = fetched.get<RenderFetchCullSortTask::Output>();
    const auto& items = fetchedItems.get0();
    const auto& opaques = items[RenderFetchCullSortTask::OPAQUE_SHAPE];

    const auto lightingModel = task.addJob<MakeLightingModel>("ImpostorBakeLightingModel");
    const auto primaryFramebuffer = task.addJob<PreparePrimaryFramebuffer>("ImpostorBakePrimary");
    const auto prepareDeferredInputs = PrepareDeferred::Inputs(primaryFramebuffer, lightingModel).asVarying();
    const auto prepareDeferredOutputs = task.addJob<PrepareDeferred>("ImpostorBakePrepareDeferred", prepareDeferredInputs);
    const auto deferredFramebuffer = prepareDeferredOutputs.getN<PrepareDeferred::Outputs>(0);
    task.addJob<PrepareStencil>("ImpostorBakeStencil", primaryFramebuffer);

    ShapePlumberPointer shapePlumber = std::make_shared<ShapePlumber>();
    auto fadeEffect = DependencyManager::get<FadeEffect>();
    initDeferredPipelines(*shapePlumber, fadeEffect->getBatchSetter(), fadeEffect->getItemUniformSetter());
    const auto jitter = render::Varying(glm::vec2(0.0f));
    const auto opaqueInputs = DrawStateSortDeferred::Inputs(opaques, lightingModel, jitter).asVarying();
    task.addJob<DrawStateSortDeferred>("ImpostorBakeDrawOpaque", opaqueInputs, shapePlumber);

    const auto resolveInputs = BakeResolve::Inputs(deferredFramebuffer, frame).asVarying();
    task.addJob<BakeResolve>("ImpostorBakeResolve", resolveInputs);
    task.addJob<BakeEnd>("ImpostorBakeEnd", cachedArgs);
}
