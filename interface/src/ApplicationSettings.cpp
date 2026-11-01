//
//  ApplicationSettings.cpp
//  interface/src
//
//  Application's settings: loaded once the menu and the plug-ins exist (loadSettings: the audio client's and the
//  LOD manager's settings, the menu's, the display plug-in the menu names, the render settings, the performance
//  preset, the camera mode, the input plug-ins activated, the audio scripting interface's and the avatar's data),
//  saved at shutdown (saveSettings), and the preferences' setters (the field of view and the clip planes, the tablet
//  scales and toolbar modes, stylus over laser and finger over stylus, the preferred cursor, the toolbar's constraint,
//  the away state when focus is lost in VR, the mini tablet). The members as they stood in Application.cpp, in their
//  order; a home of their own until an owner takes them (ARCHITECTURE.md §5.4 step 4).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "Application.h"
#include "ApplicationConstants.h"
#include <StartupProfiler.h>
#include <OffscreenUi.h>
#include <plugins/PluginManager.h>
#include <display-plugins/hmd/HmdDisplayPlugin.h>
#include "AudioClient.h"
#include "InterfaceLogging.h"
#include "LODManager.h"
#include "scripting/Audio.h"
#include "scripting/RenderScriptingInterface.h"

using namespace std;

QSharedPointer<OffscreenUi> getOffscreenUI();   // Application.cpp's

void Application::setFieldOfView(float fov) {
    if (fov != _fieldOfView.get()) {
        _fieldOfView.set(fov);
        _display.resizeGL();
    }
}

void Application::setFarClip(float farClip) {
    if (farClip != _farClip.get()) {
        _farClip.set(farClip);
    }
}

void Application::setNearClip(float nearClip) {
    if (nearClip != _nearClip.get()) {
        _nearClip.set(nearClip);
        _display.resizeGL();   // the projection is rebuilt there
    }
}

void Application::setHMDTabletScale(float hmdTabletScale) {
    _hmdTabletScale.set(hmdTabletScale);
}

void Application::setDesktopTabletScale(float desktopTabletScale) {
    _desktopTabletScale.set(desktopTabletScale);
}

void Application::setDesktopTabletBecomesToolbarSetting(bool value) {
    _desktopTabletBecomesToolbarSetting.set(value);
    updateSystemTabletMode();
}

void Application::setHmdTabletBecomesToolbarSetting(bool value) {
    _hmdTabletBecomesToolbarSetting.set(value);
    updateSystemTabletMode();
}

void Application::setPreferStylusOverLaser(bool value) {
    _preferStylusOverLaserSetting.set(value);
}

void Application::setPreferAvatarFingerOverStylus(bool value) {
    _preferAvatarFingerOverStylusSetting.set(value);
}

void Application::setPreferredCursor(const QString& cursorName) {
    qCDebug(interfaceapp) << "setPreferredCursor" << cursorName;

    if (_display.plugin() && _display.plugin()->isHmd()) {
        _preferredCursor.set(cursorName.isEmpty() ? DEFAULT_CURSOR_NAME : cursorName);
    }
    else { 
        _preferredCursor.set(cursorName.isEmpty() ? Cursor::Manager::getIconName(Cursor::Icon::SYSTEM) : cursorName); 
    }

    _windowCursor.show(Cursor::Manager::lookupIcon(_preferredCursor.get()));
}

void Application::setSettingConstrainToolbarPosition(bool setting) {
    _constrainToolbarPosition.set(setting);
    getOffscreenUI()->setConstrainToolbarToCenterX(setting);
}

void Application::setAwayStateWhenFocusLostInVREnabled(bool enabled) {
    _awayStateWhenFocusLostInVREnabled.set(enabled);
    emit awayStateWhenFocusLostInVRChanged(enabled);
}

void Application::loadSettings() {

    {
        StartupProfiler::Scope step("loadSettings: audio and LOD");
        DependencyManager::get<AudioClient>()->loadSettings();
        DependencyManager::get<LODManager>()->loadSettings();
    }

    auto menu = Menu::getInstance();
    {
        StartupProfiler::Scope step("loadSettings: menu");
        menu->loadSettings();
    }

    // override the menu option show overlays to always be true on startup
    menu->setIsOptionChecked(MenuOption::Overlays, true);

    int displayPluginStep = StartupProfiler::begin("loadSettings: display plugin from the menu");
    // If there is a preferred plugin, we probably messed it up with the menu settings, so fix it.
    auto pluginManager = PluginManager::getInstance();
    auto plugins = pluginManager->getPreferredDisplayPlugins();
    if (plugins.size() > 0) {
        for (auto plugin : plugins) {
            if (auto action = menu->getActionForOption(plugin->getName())) {
                action->setChecked(true);
                action->trigger();
                // Find and activated highest priority plugin, bail for the rest
                break;
            }
        }
    }

    bool isFirstPerson = false;
    auto displayPlugins = pluginManager->getDisplayPlugins();
    for (auto& plugin : displayPlugins) {
        if (!plugin->isHmd()) {
            if (auto action = menu->getActionForOption(plugin->getName())) {
                action->setChecked(true);
                action->trigger();
                break;
            }
        }
    }
    isFirstPerson = (qApp->isHMDMode());
    StartupProfiler::end(displayPluginStep);

    // Load settings of the RenderScritpingInterface
    // Do that explicitely before being used
    {
        StartupProfiler::Scope step("loadSettings: render settings");
        RenderScriptingInterface::getInstance()->loadSettings();
    }

    // Setup the PerformanceManager which will enforce the several settings to match the Preset
    // On the first run, the Preset is evaluated from the 
    {
        StartupProfiler::Scope step("loadSettings: performance preset");
        getPerformanceManager().setupPerformancePresetSettings(_firstRun.get());
    }

    // finish initializing the camera, based on everything we checked above. Third person camera will be used if no settings
    // dictated that we should be in first person
    Menu::getInstance()->setIsOptionChecked(MenuOption::FirstPersonLookAt, isFirstPerson);
    Menu::getInstance()->setIsOptionChecked(MenuOption::ThirdPerson, !isFirstPerson);
    _viewState.camera().setMode((isFirstPerson) ? CAMERA_MODE_FIRST_PERSON_LOOK_AT : CAMERA_MODE_LOOK_AT);
    _viewState.cameraMenuChanged();

    {
        StartupProfiler::Scope step("loadSettings: input plugins activated");
        auto inputs = pluginManager->getInputPlugins();
        for (auto plugin : inputs) {
            if (!plugin->isActive()) {
                plugin->activate();
            }
        }
    }

    QSharedPointer<scripting::Audio> audioScriptingInterface = qSharedPointerDynamicCast<scripting::Audio>(DependencyManager::get<AudioScriptingInterface>());
    if (audioScriptingInterface) {
        audioScriptingInterface->loadData();
    }

    {
        StartupProfiler::Scope step("loadSettings: avatar");
        getMyAvatar()->loadData();
    }
    _settingsLoaded = true;
}

void Application::saveSettings() const {
    DependencyManager::get<AudioClient>()->saveSettings();
    DependencyManager::get<LODManager>()->saveSettings();

    QSharedPointer<scripting::Audio> audioScriptingInterface = qSharedPointerDynamicCast<scripting::Audio>(DependencyManager::get<AudioScriptingInterface>());
    if (audioScriptingInterface) {
        audioScriptingInterface->saveData();
    }

    Menu::getInstance()->saveSettings();
    getMyAvatar()->saveData();
    PluginManager::getInstance()->saveSettings();
}
