//
//  RenderViewTask.h
//  render-utils/src/
//
//  Created by Sam Gateau on 5/25/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_RenderViewTask_h
#define hifi_RenderViewTask_h

#include <render/Engine.h>
#include <render/RenderFetchCullSortTask.h>

#include "AssembleLightingStageTask.h"
#include "RenderShadowTask.h"

// Deferred or forward rendering of the view, the frame's shadows (RenderShadowTask's output: the sun's cascades, and
// the lights' maps and cookies with them) having been drawn before the choice, so that both methods light with them
class DeferredForwardSwitchJob {
public:
    using Input = render::VaryingSet4<RenderFetchCullSortTask::Output, LightingModelPointer, AssembleLightingStageTask::Output, RenderShadowTask::Output>;
    using JobModel = render::Switch::ModelI<DeferredForwardSwitchJob, Input>;

    DeferredForwardSwitchJob() {}

    void configure(const render::SwitchConfig& config) {}
    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs);

};

class RenderViewTask {
public:
    using Input = RenderFetchCullSortTask::Output;
    using JobModel = render::Task::ModelI<RenderViewTask, Input>;

    RenderViewTask() {}

    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor, uint8_t tagBits = 0x00, uint8_t tagMask = 0x00);

};


#endif // hifi_RenderViewTask_h
