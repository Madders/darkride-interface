//
//  CallbackEndpoint.cpp
//  libraries/controllers/src/controllers/impl/endpoints
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "CallbackEndpoint.h"

#include <QtCore/QThread>
#include <QtCore/QVariantMap>

#include <RegisteredMetaTypes.h>

#include "../../Logging.h"

using namespace controller;

QVariant CallbackEndpoint::poseToVariant(const Pose& pose) {
    QVariantMap map;
    map["translation"] = vec3toVariant(pose.translation);
    map["rotation"] = quatToVariant(pose.rotation);
    map["velocity"] = vec3toVariant(pose.velocity);
    map["angularVelocity"] = vec3toVariant(pose.angularVelocity);
    map["valid"] = pose.valid;
    return map;
}

bool CallbackEndpoint::poseFromVariant(const QVariant& value, Pose& pose) {
    if (value.type() != QVariant::Map) {
        return false;
    }
    QVariantMap map = value.toMap();
    pose.translation = vec3FromVariant(map.value("translation"));
    pose.rotation = quatFromVariant(map.value("rotation"));
    pose.velocity = vec3FromVariant(map.value("velocity"));
    pose.angularVelocity = vec3FromVariant(map.value("angularVelocity"));
    pose.valid = map.value("valid").toBool();
    return true;
}

AxisValue CallbackEndpoint::peek() const {
    const_cast<CallbackEndpoint*>(this)->updateValue();
    return AxisValue(_lastValueRead, 0);
}

void CallbackEndpoint::updateValue() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "updateValue", Qt::QueuedConnection);
        return;
    }
    QVariant result = _callback.call();
    if (result.canConvert<double>() && result.type() != QVariant::Map) {
        _lastValueRead = result.toFloat();
    } else if (poseFromVariant(result, _lastPoseRead)) {
        _returnPose = true;
    } else {
        _lastValueRead = 0.0f;
    }
}

void CallbackEndpoint::apply(AxisValue value, const Pointer& source) {
    if (value == _lastValueWritten) {
        return;
    }
    _lastValueWritten = value;
    internalApply(value.value, source->getInput().getID());
}

void CallbackEndpoint::internalApply(float value, int sourceID) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "internalApply", Qt::QueuedConnection, Q_ARG(float, value), Q_ARG(int, sourceID));
        return;
    }
    _callback.call(QVariantList { value, sourceID });
}

Pose CallbackEndpoint::peekPose() const {
    const_cast<CallbackEndpoint*>(this)->updatePose();
    return _lastPoseRead;
}

void CallbackEndpoint::updatePose() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "updatePose", Qt::QueuedConnection);
        return;
    }
    poseFromVariant(_callback.call(), _lastPoseRead);
}

void CallbackEndpoint::apply(const Pose& newPose, const Pointer& source) {
    // two invalid poses never compare equal (Pose::operator==), so an untracked hand would call the script every frame;
    // once is enough
    if (newPose == _lastPoseWritten || (!newPose.isValid() && !_lastPoseWritten.isValid() && _poseWritten)) {
        return;
    }
    _poseWritten = true;
    _lastPoseWritten = newPose;
    internalApplyPose(poseToVariant(newPose), source->getInput().getID());
}

void CallbackEndpoint::internalApplyPose(const QVariant& pose, int sourceID) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "internalApplyPose", Qt::QueuedConnection, Q_ARG(QVariant, pose), Q_ARG(int, sourceID));
        return;
    }
    _callback.call(QVariantList { pose, sourceID });
}
