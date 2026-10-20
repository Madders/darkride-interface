//
//  CallbackConditional.cpp
//  libraries/controllers/src/controllers/impl/conditionals
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "CallbackConditional.h"

#include <QtCore/QThread>

using namespace controller;

bool CallbackConditional::satisfied() {
    updateValue();
    return _lastValue;
}

void CallbackConditional::updateValue() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "updateValue", Qt::QueuedConnection);
        return;
    }
    _lastValue = _callback.call().toBool();
}
