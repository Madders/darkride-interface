//
//  FancyCamera.cpp
//  interface/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "FancyCamera.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QThread>

#include <shared/QtHelpers.h>

#include "Application.h"
#include "avatar/AvatarManager.h"

FancyCamera::FancyCamera() : Camera() {
  QObject::connect(this, &FancyCamera::parentIDChanged, this, [this]{
    bool success;
    _parent = SpatiallyNestable::findByID(parentID, success);
  });
}

PickRay FancyCamera::computePickRay(float x, float y) const {
    return qApp->computePickRay(x, y);
}

float FancyCamera::collideBoom(const glm::vec3& pivot, const glm::vec3& wanted, const QVector<QUuid>& ignore) {
    if (QThread::currentThread() != thread()) {   // a script's thread: the physics' queries run on the main thread
        float result;
        BLOCKING_INVOKE_METHOD(this, "collideBoom", Q_RETURN_ARG(float, result), Q_ARG(const glm::vec3&, pivot),
                               Q_ARG(const glm::vec3&, wanted), Q_ARG(const QVector<QUuid>&, ignore));
        return result;
    }
    return qApp->getViewState().allowedBoomLength(pivot, wanted, ignore);
}

void FancyCamera::setChaseTarget(const QUuid& entityID, const QVariantMap& options) {
    ChaseSettings chase;
    auto number = [&](const char* name, float fallback, float least) {
        bool ok = false;
        float value = options.value(name).toFloat(&ok);
        return (ok && std::isfinite(value)) ? std::max(least, value) : fallback;
    };
    chase.entityID = entityID;
    chase.back = number("back", chase.back, 0.0f);
    chase.up = number("up", chase.up, -100.0f);
    chase.rate = number("rate", chase.rate, 0.01f);
    chase.pivotHeight = number("pivotHeight", chase.pivotHeight, -100.0f);
    chase.lookAhead = number("lookAhead", chase.lookAhead, -100.0f);
    chase.lookHeight = number("lookHeight", chase.lookHeight, -100.0f);
    chase.followPitch = options.value("followPitch").toBool();
    std::lock_guard<std::mutex> lock(_chaseMutex);
    chase.version = (entityID == _chase.entityID) ? _chase.version : _chase.version + 1;
    _chase = chase;
}

void FancyCamera::clearChaseTarget() {
    std::lock_guard<std::mutex> lock(_chaseMutex);
    _chase.entityID = QUuid();
    ++_chase.version;
}

FancyCamera::ChaseSettings FancyCamera::getChaseSettings() const {
    std::lock_guard<std::mutex> lock(_chaseMutex);
    return _chase;
}
