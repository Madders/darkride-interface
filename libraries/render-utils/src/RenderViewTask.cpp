//
//  RenderViewTask.cpp
//  render-utils/src/
//
//  Created by Sam Gateau on 5/25/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "RenderViewTask.h"

#include "RenderShadowTask.h"
#include "RenderCommonTask.h"
#include "RenderDeferredTask.h"
#include "RenderForwardTask.h"
#include "RenderLocalShadowTask.h"

void DeferredForwardSwitchJob::build(JobModel& task, const render::Varying& input, render::Varying& output) {
    task.addBranch<RenderDeferredTask>("RenderDeferredTask", 0, input);

    task.addBranch<RenderForwardTask>("RenderForwardTask", 1, input);
}

void RenderViewTask::build(JobModel& task, const render::Varying& input, render::Varying& output, render::CullFunctor cullFunctor, uint8_t tagBits, uint8_t tagMask) {
    const auto items = task.addJob<RenderFetchCullSortTask>("FetchCullSort", cullFunctor, tagBits, tagMask);

    // Issue the lighting model, aka the big global settings for the view 
    const auto lightingModel = task.addJob<MakeLightingModel>("LightingModel");

    // Assemble the lighting stages current frames
    const auto lightingStageFramesAndZones = task.addJob<AssembleLightingStageTask>("AssembleStages", items);
    const auto lightFrame = lightingStageFramesAndZones.get<AssembleLightingStageTask::Output>().get0()[0];

    // The frame's shadows, drawn before the choice of render method so that deferred and forward light with the same:
    // the sun's cascades first, then the spot and point lights' shadow maps and cookies, which join them in the frame
    // Warning : the cull functor passed to the shadow pass should only be testing for LOD culling. If frustum culling
    // is performed, then casters not in the view frustum will be removed, which is not what we wish.
    const auto shadowTaskIn = RenderShadowTask::Input(lightFrame, lightingModel).asVarying();
    const auto shadowTaskOut = task.addJob<RenderShadowTask>("RenderShadowTask", shadowTaskIn, cullFunctor, tagBits, tagMask);

    const auto localShadowTaskIn = RenderLocalShadowTask::Input(lightFrame, lightingModel, shadowTaskOut.getN<RenderShadowTask::Output>(1)).asVarying();
    task.addJob<RenderLocalShadowTask>("RenderLocalShadowTask", localShadowTaskIn, tagBits, tagMask);

#ifndef Q_OS_ANDROID
        const auto deferredForwardIn = DeferredForwardSwitchJob::Input(items, lightingModel, lightingStageFramesAndZones, shadowTaskOut).asVarying();
        task.addJob<DeferredForwardSwitchJob>("DeferredForwardSwitch", deferredForwardIn);
#else
        const auto renderInput = RenderForwardTask::Input(items, lightingModel, lightingStageFramesAndZones, shadowTaskOut).asVarying();
        task.addJob<RenderForwardTask>("RenderForwardTask", renderInput);
#endif
}
