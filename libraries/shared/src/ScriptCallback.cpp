//
//  ScriptCallback.cpp
//  libraries/shared/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptCallback.h"

QVariant ScriptCallback::call(const QVariantList& arguments) const {
    auto engine = target.lock();
    if (!engine || !isValid()) {
        return QVariant();
    }
    QVariant result;
    QString error;
    if (!engine->invoke(id, arguments, result, error)) {
        return QVariant();
    }
    return result;
}

void ScriptCallback::post(const QVariantList& arguments) const {
    auto engine = target.lock();
    if (engine && isValid()) {
        engine->post(id, arguments);
    }
}

void ScriptCallback::request(const QVariantList& arguments, std::function<void(const QVariant&)> onResult) const {
    auto engine = target.lock();
    if (engine && isValid()) {
        engine->request(id, arguments, std::move(onResult));
    }
}
