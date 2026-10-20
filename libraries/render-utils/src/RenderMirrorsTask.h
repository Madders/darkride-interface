//
//  RenderMirrorsTask.h
//  libraries/render-utils/src
//
//  Draws the mirrors' pictures before the main view (see Mirrors.h): a slot's turn swaps the frame's framebuffer,
//  viewport and view for the mirror's, a whole view (shadows, deferred lighting) is drawn, and the frame's own are
//  put back. An idle slot draws a few texels of nothing.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderMirrorsTask_h
#define hifi_RenderMirrorsTask_h

#include <render/Engine.h>
#include <render/CullTask.h>

class RenderMirrorsTaskConfig : public render::Task::Config {
    Q_OBJECT
public:
    RenderMirrorsTaskConfig() : render::Task::Config(true) {}
};

class RenderMirrorsTask {
public:
    using Config = RenderMirrorsTaskConfig;
    using JobModel = render::Task::Model<RenderMirrorsTask, Config>;

    RenderMirrorsTask() {}
    void configure(const Config& config) {}
    void build(JobModel& task, const render::Varying& inputs, render::Varying& outputs, render::CullFunctor cullFunctor);
};

#endif // hifi_RenderMirrorsTask_h
