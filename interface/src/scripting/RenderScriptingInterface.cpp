//
//  Created by Sam Gondelman on 5/16/19
//  Copyright 2013-2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "RenderScriptingInterface.h"
#include <plugins/DisplayPlugin.h>

#include "LightingModel.h"
#include "AntialiasingEffect.h"
#include <Mirrors.h>
#include <gpu/Context.h>

namespace {
    // 1, 2, 4 or 8, and no more than the GPU can do (Apple GPUs stop at 4)
    int validMsaaSamples(int samples) {
        const int maxSamples = gpu::Context::getMaxSamples();
        if (maxSamples > 0) {
            samples = std::min(samples, maxSamples);
        }
        return samples >= 8 ? 8 : samples >= 4 ? 4 : samples >= 2 ? 2 : 1;
    }
}

RenderScriptingInterface* RenderScriptingInterface::getInstance() {
    static RenderScriptingInterface sharedInstance;
    return &sharedInstance;
}

std::once_flag RenderScriptingInterface::registry_flag;

RenderScriptingInterface::RenderScriptingInterface() {
    std::call_once(registry_flag, [] {
        qmlRegisterType<RenderScriptingInterface>("RenderEnums", 1, 0, "RenderEnums");
    });
}

void RenderScriptingInterface::loadSettings() {
    _renderSettingLock.withReadLock([&] {
        _renderMethod = (_renderMethodSetting.get());
        _shadowsEnabled = (_shadowsEnabledSetting.get());
        _vsyncEnabled = (_vsyncEnabledSetting.get());   // the display plugin applies it when it activates
        _ambientOcclusionEnabled = (_ambientOcclusionEnabledSetting.get());
        _antialiasingMethod = (_antialiasingMethodSetting.get());
        _msaaSamples = (_msaaSamplesSetting.get());
        _viewportResolutionScale = (_viewportResolutionScaleSetting.get());
        // forward became the default on 2026-10-16: a settings file from before keeps its own method until this one
        // move; a user who goes back to deferred after it is left there
        if (!_forwardDefaultAppliedSetting.get()) {
            _renderMethod = render::Args::RenderMethod::FORWARD;
            _forwardDefaultAppliedSetting.set(true);
        }
        _nametagsEnabled = (_nametagsEnabledSetting.get());
        _nametagsShowSelf = (_nametagsShowSelfSetting.get());
        // _maximumTextureMemory = (_maximumTextureMemorySetting.get()); // handled by gpu::Texture
        _fieldOfView = (_fieldOfViewSetting.get());
        _farClip = (_farClipSetting.get());
        _nearClip = (_nearClipSetting.get());
    });
    forceRenderMethod((RenderMethod)_renderMethod);
    forceShadowsEnabled(_shadowsEnabled);
    forceAmbientOcclusionEnabled(_ambientOcclusionEnabled);
    forceAntialiasingMethod((AntialiasingMethod)_antialiasingMethod);
    forceMsaaSamples(_msaaSamples);
    forceViewportResolutionScale(_viewportResolutionScale);
    forceNametagsEnabled(_nametagsEnabled);
    forceNametagsShowSelf(_nametagsShowSelf);
    forceMaximumTextureMemory(_maximumTextureMemorySetting.get());
    forceFieldOfView(_fieldOfView);
    forceFarClip(_farClip);
    forceNearClip(_nearClip);
    emit settingsChanged();   // the menu items that show these follow, whether they were built before or after
}

RenderScriptingInterface::RenderMethod RenderScriptingInterface::getRenderMethod() const {
    return (RenderMethod)_renderMethod;
}

void RenderScriptingInterface::setRenderMethod(RenderMethod renderMethod) {
    if (isValidRenderMethod(renderMethod) && (_renderMethod != (int)renderMethod)) {
        forceRenderMethod(renderMethod);
        emit settingsChanged();
    }
}
void RenderScriptingInterface::forceRenderMethod(RenderMethod renderMethod) {
    _renderSettingLock.withWriteLock([&] {
        _renderMethod = (int)renderMethod;
        _renderMethodSetting.set((int)renderMethod);

        auto config = dynamic_cast<render::SwitchConfig*>(
            qApp->getRenderEngine()->getConfiguration()->getConfig(
                "RenderMainView.DeferredForwardSwitch"
            )
        );
        if (config) config->setBranch((int)renderMethod);

        auto secondaryConfig = dynamic_cast<render::SwitchConfig*>(
            qApp->getRenderEngine()->getConfiguration()->getConfig(
                "RenderSecondView.DeferredForwardSwitch"
            )
        );
        if (secondaryConfig) secondaryConfig->setBranch((int)renderMethod);

        // the mirror views render by the frame's method too: left on the switch's first branch, they drew deferred
        // under the forward default, and their reflections lost the zone's ambient light (MIRRORS.md section 5)
        for (int slot = 0; slot < Mirrors::SLOT_COUNT; ++slot) {
            auto mirrorConfig = dynamic_cast<render::SwitchConfig*>(
                qApp->getRenderEngine()->getConfiguration()->getConfig(
                    "RenderMirrors.MirrorView" + QString::number(slot) + ".DeferredForwardSwitch"
                )
            );
            if (mirrorConfig) mirrorConfig->setBranch((int)renderMethod);
        }
    });
}

int RenderScriptingInterface::getMsaaSamples() const {
    return _msaaSamples;
}

void RenderScriptingInterface::setMsaaSamples(int samples) {
    samples = validMsaaSamples(samples);
    if (_msaaSamples != samples) {
        forceMsaaSamples(samples);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceMsaaSamples(int samples) {
    samples = validMsaaSamples(samples);
    _renderSettingLock.withWriteLock([&] {
        _msaaSamples = samples;
        _msaaSamplesSetting.set(samples);

        // the forward task's primary framebuffer, in every view that has one: the main view, the second one, the
        // mirror views (which render by the frame's method)
        auto configuration = qApp->getRenderEngine()->getConfiguration();
        std::vector<std::string> views = { "RenderMainView.", "RenderSecondView." };
        for (int slot = 0; slot < Mirrors::SLOT_COUNT; ++slot) {
            views.push_back("RenderMirrors.MirrorView" + std::to_string(slot) + ".");
        }
        for (const auto& view : views) {
            if (auto config = configuration->getJobConfig(view + "PreparePrimaryBufferForward")) {
                config->setProperty("numSamples", samples);
            }
        }
    });
}

QStringList RenderScriptingInterface::getRenderMethodNames() const {
    static const QStringList renderMethodNames = { "DEFERRED", "FORWARD" };
    return renderMethodNames;
}

bool RenderScriptingInterface::getShadowsEnabled() const {
    return _shadowsEnabled;
}

void RenderScriptingInterface::setShadowsEnabled(bool enabled) {
    if (_shadowsEnabled != enabled) {
        forceShadowsEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceShadowsEnabled(bool enabled) {
    _renderSettingLock.withWriteLock([&] {
        _shadowsEnabled = (enabled);
        _shadowsEnabledSetting.set(enabled);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        assert(renderConfig);
        auto lightingModelConfig = renderConfig->getConfig<MakeLightingModel>("RenderMainView.LightingModel");
        if (lightingModelConfig) {
            Menu::getInstance()->setIsOptionChecked(MenuOption::Shadows, enabled);
            lightingModelConfig->setShadow(enabled);
        }
        auto secondaryLightingModelConfig = renderConfig->getConfig<MakeLightingModel>("RenderSecondView.LightingModel");
        if (secondaryLightingModelConfig) {
            Menu::getInstance()->setIsOptionChecked(MenuOption::Shadows, enabled);
            secondaryLightingModelConfig->setShadow(enabled);
        }
    });
}

bool RenderScriptingInterface::getVsyncEnabled() const {
    return _vsyncEnabled;
}

void RenderScriptingInterface::setVsyncEnabled(bool enabled) {
    if (_vsyncEnabled != enabled) {
        forceVsyncEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceVsyncEnabled(bool enabled) {
    _renderSettingLock.withWriteLock([&] {
        _vsyncEnabled = enabled;
        _vsyncEnabledSetting.set(enabled);
    });
    // the active display plugin's present thread applies it from its next present (one activated later reads the
    // setting itself); only a change comes here, never the load, which would start the display early
    auto displayPlugin = qApp->getActiveDisplayPlugin();
    if (displayPlugin) {
        displayPlugin->setVsyncWanted(enabled);
    }
}

bool RenderScriptingInterface::getAmbientOcclusionEnabled() const {
    return _ambientOcclusionEnabled;
}

void RenderScriptingInterface::setAmbientOcclusionEnabled(bool enabled) {
    if (_ambientOcclusionEnabled != enabled) {
        forceAmbientOcclusionEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceAmbientOcclusionEnabled(bool enabled) {
    _renderSettingLock.withWriteLock([&] {
        _ambientOcclusionEnabled = (enabled);
        _ambientOcclusionEnabledSetting.set(enabled);

        auto config = qApp->getRenderEngine()->getConfiguration();

        auto lightingModelConfig = config->getConfig<MakeLightingModel>(
            "RenderMainView.LightingModel"
        );
        if (lightingModelConfig) {
            Menu::getInstance()->setIsOptionChecked(MenuOption::AmbientOcclusion, enabled);
            lightingModelConfig->setAmbientOcclusion(enabled);
        }

        auto secondaryLightingModelConfig = config->getConfig<MakeLightingModel>(
            "RenderSecondView.LightingModel"
        );
        if (secondaryLightingModelConfig) {
            Menu::getInstance()->setIsOptionChecked(MenuOption::AmbientOcclusion, enabled);
            secondaryLightingModelConfig->setAmbientOcclusion(enabled);
        }
    });
}

RenderScriptingInterface::AntialiasingMethod RenderScriptingInterface::getAntialiasingMethod() const {
    return (AntialiasingMethod)_antialiasingMethod;
}

void RenderScriptingInterface::setAntialiasingMethod(AntialiasingMethod antialiasingMethod) {
    if (isValidAntialiasingMethod(antialiasingMethod) && (_antialiasingMethod != (int)antialiasingMethod)) {
        forceAntialiasingMethod(antialiasingMethod);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceAntialiasingMethod(AntialiasingMethod antialiasingMethod) {
    _renderSettingLock.withWriteLock([&] {
        _antialiasingMethod = (int)antialiasingMethod;
        _antialiasingMethodSetting.set((int)antialiasingMethod);

        bool useTAA = antialiasingMethod == AntialiasingMethod::TAA;
        bool useFXAA = antialiasingMethod == AntialiasingMethod::FXAA;

        auto instance = qApp->getRenderEngine()->getConfiguration();
        
        auto mainAa = instance->getConfig<Antialiasing>("RenderMainView.Antialiasing");
        auto mainQAa = instance->getConfig("RenderMainView.Antialiasing");
        auto mainJitter = instance->getConfig<JitterSample>("RenderMainView.JitterCam");

        auto secondAa = instance->getConfig<Antialiasing>("RenderSecondView.Antialiasing");
        auto secondQAa = instance->getConfig("RenderSecondView.Antialiasing");
        auto secondJitter = instance->getConfig<JitterSample>("RenderSecondView.JitterCam");

        if (
            mainAa && mainQAa && mainJitter &&
            secondAa && secondQAa && secondJitter
        ) {
            mainAa->setDebugFXAA(useFXAA);
            mainQAa->setProperty("constrainColor", useTAA);
            mainQAa->setProperty("feedbackColor", useTAA);
            mainQAa->setProperty("blend", useTAA ? 0.25f : 1);
            mainQAa->setProperty("sharpen", useTAA ? 0.05f : 0);

            secondAa->setDebugFXAA(useFXAA);
            secondQAa->setProperty("constrainColor", useTAA);
            secondQAa->setProperty("feedbackColor", useTAA);
            secondQAa->setProperty("blend", useTAA ? 0.25f : 1);
            secondQAa->setProperty("sharpen", useTAA ? 0.05f : 0);

            if (useTAA) {
                mainJitter->play();
                secondJitter->play();
            } else {
                mainJitter->none();
                secondJitter->none();
            }
        }
    });
}

QStringList RenderScriptingInterface::getAntialiasingMethodNames() const {
    static const QStringList antialiasingMethodNames = { "NONE", "TAA", "FXAA" };
    return antialiasingMethodNames;
}

float RenderScriptingInterface::getViewportResolutionScale() const {
    return _viewportResolutionScale;
}

void RenderScriptingInterface::setViewportResolutionScale(float scale) {
    if (_viewportResolutionScale != scale) {
        forceViewportResolutionScale(scale);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceViewportResolutionScale(float scale) {
    // just not negative values or zero
    if (scale <= 0.f) {
        return;
    }
    _renderSettingLock.withWriteLock([&] {
        _viewportResolutionScale = (scale);
        _viewportResolutionScaleSetting.set(scale);

        auto renderConfig = qApp->getRenderEngine()->getConfiguration();
        assert(renderConfig);
        auto deferredView = renderConfig->getConfig("RenderMainView.RenderDeferredTask");
        // mainView can be null if we're rendering in forward mode
        if (deferredView) {
            deferredView->setProperty("resolutionScale", _viewportResolutionScale);
        }
        auto forwardView = renderConfig->getConfig("RenderMainView.RenderForwardTask");
        // mainView can be null if we're rendering in forward mode
        if (forwardView) {
            forwardView->setProperty("resolutionScale", _viewportResolutionScale);
        }
    });
}

bool RenderScriptingInterface::getNametagsEnabled() const {
    return _nametagsEnabled;
}

void RenderScriptingInterface::setNametagsEnabled(bool enabled) {
    if (_nametagsEnabled != enabled) {
        forceNametagsEnabled(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceNametagsEnabled(bool enabled) {
    _nametagsEnabled = (enabled);
    _nametagsEnabledSetting.set(enabled);

    // put nametags logic here when implementing in cpp
    // code is in javascript right now and uses these functions
}

bool RenderScriptingInterface::getNametagsShowSelf() const {
    return _nametagsShowSelf;
}

void RenderScriptingInterface::setNametagsShowSelf(bool enabled) {
    if (_nametagsShowSelf != enabled) {
        forceNametagsShowSelf(enabled);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceNametagsShowSelf(bool enabled) {
    _nametagsShowSelf = (enabled);
    _nametagsShowSelfSetting.set(enabled);

    // put nametags logic here when implementing in cpp
    // code is in javascript right now and uses these functions
}

// TODO: Hook this up to a ToneMappingStage
// void RenderScriptingInterface::setToneMappingMode(int curve, float exposure) {
//    // tonemapping curve {0:RGB, 1:SRGB, 2:Reinhard, 3:Filmic} 
//     _renderSettingLock.withWriteLock([&] {
//         auto renderConfig = qApp->getRenderEngine()->getConfiguration();
//         assert(renderConfig);
//         auto deferredView = renderConfig->getConfig("RenderMainView.ToneMapping");  
//           if (deferredView) {
//               deferredView->setProperty("toneMappingMode", curve);
//               deferredView->setProperty("Tonemapping.exposure", exposure);
//           }
//          auto forwardView = renderConfig->getConfig("RenderMainView.ToneMapping");
//           if (forwardView) {
//               forwardView->setProperty("toneMappingMode", curve);
//               forwardView->setProperty("Tonemapping.exposure", exposure);
//           }
//     });
// }

int RenderScriptingInterface::getMaximumTextureMemory() const {
    return BYTES_TO_MB(gpu::Texture::getAllowedGPUMemoryUsage());
}

void RenderScriptingInterface::setMaximumTextureMemory(int maximumTextureMemory) {
    if (getMaximumTextureMemory() != maximumTextureMemory) {
        forceMaximumTextureMemory(maximumTextureMemory);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceMaximumTextureMemory(int maximumTextureMemory) {
    // 0 is automatic which doesn't work very well right now
    if (maximumTextureMemory > 8192) maximumTextureMemory = 8192;
    if (maximumTextureMemory < 0) maximumTextureMemory = 0;
    _maximumTextureMemorySetting.set(maximumTextureMemory);
   
    gpu::Context::Size newMaximumTextureMemory = MB_TO_BYTES(maximumTextureMemory);
    gpu::Texture::setAllowedGPUMemoryUsage(newMaximumTextureMemory);
}

float RenderScriptingInterface::getFieldOfView() const {
    return _fieldOfView;
}

void RenderScriptingInterface::setFieldOfView(float fieldOfView) {
    if (_fieldOfView != fieldOfView) {
        forceFieldOfView(fieldOfView);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceFieldOfView(float fieldOfView) {
    _fieldOfView = fieldOfView;
    _fieldOfViewSetting.set(fieldOfView);
    qApp->setFieldOfView(fieldOfView);
}

float RenderScriptingInterface::getFarClip() const {
    return _farClip;
}

void RenderScriptingInterface::setFarClip(float farClip) {
    if (_farClip != farClip) {
        forceFarClip(farClip);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceFarClip(float farClip) {
    _farClip = farClip;
    _farClipSetting.set(farClip);
    qApp->setFarClip(farClip);
}

float RenderScriptingInterface::getNearClip() const {
    return _nearClip;
}

void RenderScriptingInterface::setNearClip(float nearClip) {
    nearClip = glm::clamp(nearClip, 0.01f, 5.0f);
    if (_nearClip != nearClip) {
        forceNearClip(nearClip);
        emit settingsChanged();
    }
}

void RenderScriptingInterface::forceNearClip(float nearClip) {
    _nearClip = nearClip;
    _nearClipSetting.set(nearClip);
    qApp->setNearClip(nearClip);
}
