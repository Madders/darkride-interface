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
