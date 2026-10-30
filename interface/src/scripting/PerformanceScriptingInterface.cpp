//
//  Created by Bradley Austin Davis on 2019/05/14
//  Copyright 2013-2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "PerformanceScriptingInterface.h"

#include <mutex>

#include <QtCore/QSet>

#include "../Application.h"

std::once_flag PerformanceScriptingInterface::registry_flag;

static std::mutex instancesMutex;
static QSet<PerformanceScriptingInterface*> instances;

PerformanceScriptingInterface::PerformanceScriptingInterface() {
    std::call_once(registry_flag, [] {
        qmlRegisterType<PerformanceScriptingInterface>("PerformanceEnums", 1, 0, "PerformanceEnums");
    });
    std::lock_guard<std::mutex> lock(instancesMutex);
    instances.insert(this);
}

PerformanceScriptingInterface::~PerformanceScriptingInterface() {
    std::lock_guard<std::mutex> lock(instancesMutex);
    instances.remove(this);
}

void PerformanceScriptingInterface::announceChange() {
    // each object's signal is emitted on its own thread (a script engine's, or the main thread for a QML surface), and
    // always later, from its event loop: never under the lock, where a handler that sets a setting would take it again
    // (qa's 1.1 review #13). The lock keeps each object alive while its event is posted; a deleted one's is dropped.
    std::lock_guard<std::mutex> lock(instancesMutex);
    for (auto instance : instances) {
        QMetaObject::invokeMethod(instance, "settingsChanged", Qt::QueuedConnection);
    }
}

void PerformanceScriptingInterface::setPerformancePreset(PerformancePreset performancePreset) {
    qApp->getPerformanceManager().setPerformancePreset((PerformanceManager::PerformancePreset)performancePreset);
    announceChange();
}

PerformanceScriptingInterface::PerformancePreset PerformanceScriptingInterface::getPerformancePreset() const {
    return (PerformanceScriptingInterface::PerformancePreset)qApp->getPerformanceManager().getPerformancePreset();
}

QStringList PerformanceScriptingInterface::getPerformancePresetNames() const {
    static const QStringList performancePresetNames = { "UNKNOWN", "POTATO", "LOW", "MID", "HIGH" };
    return performancePresetNames;
}

void PerformanceScriptingInterface::setRefreshRateProfile(RefreshRateProfile refreshRateProfile) {
    qApp->getRefreshRateManager().setRefreshRateProfile((RefreshRateManager::RefreshRateProfile)refreshRateProfile);
    announceChange();
}

PerformanceScriptingInterface::RefreshRateProfile PerformanceScriptingInterface::getRefreshRateProfile() const {
    return (PerformanceScriptingInterface::RefreshRateProfile)qApp->getRefreshRateManager().getRefreshRateProfile();
}

QStringList PerformanceScriptingInterface::getRefreshRateProfileNames() const {
    static const QStringList refreshRateProfileNames = { "ECO", "INTERACTIVE", "REALTIME", "DISPLAY_BASED" };
    return refreshRateProfileNames;
}

int PerformanceScriptingInterface::getActiveRefreshRate() const {
    return qApp->getRefreshRateManager().getActiveRefreshRate();
}

PerformanceScriptingInterface::UXMode PerformanceScriptingInterface::getUXMode() const {
    return (PerformanceScriptingInterface::UXMode)qApp->getRefreshRateManager().getUXMode();
}

PerformanceScriptingInterface::RefreshRateRegime PerformanceScriptingInterface::getRefreshRateRegime() const {
    return (PerformanceScriptingInterface::RefreshRateRegime)qApp->getRefreshRateManager().getRefreshRateRegime();
}
