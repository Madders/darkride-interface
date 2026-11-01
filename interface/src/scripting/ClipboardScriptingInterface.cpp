//
//  ClipboardScriptingInterface.cpp
//  interface/src/scripting
//
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ClipboardScriptingInterface.h"

#include <shared/QtHelpers.h>

#include "Application.h"

ClipboardScriptingInterface::ClipboardScriptingInterface() {
}

glm::vec3 ClipboardScriptingInterface::getContentsDimensions() {
    return qApp->getEntityClipboard().tree()->getContentsDimensions();
}

float ClipboardScriptingInterface::getClipboardContentsLargestDimension() {
    return qApp->getEntityClipboard().tree()->getContentsLargestDimension();
}

// Each call runs on the main thread, where the clipboard lives, and waits for its result.
bool ClipboardScriptingInterface::exportEntities(const QString& filename, const QVector<QUuid>& entityIDs) {
    auto& clipboard = qApp->getEntityClipboard();
    return BLOCKING_INVOKE(qApp, [&] { return clipboard.exportEntities(filename, entityIDs); });
}

bool ClipboardScriptingInterface::exportEntities(const QString& filename, float x, float y, float z, float scale) {
    auto& clipboard = qApp->getEntityClipboard();
    return BLOCKING_INVOKE(qApp, [&] { return clipboard.exportEntities(filename, x, y, z, scale); });
}

bool ClipboardScriptingInterface::importEntities(
    const QString& filename,
    const bool isObservable,
    const qint64 callerId
) {
    auto& clipboard = qApp->getEntityClipboard();
    return BLOCKING_INVOKE(qApp, [&] { return clipboard.importEntities(filename, isObservable, callerId); });
}

QVector<EntityItemID> ClipboardScriptingInterface::pasteEntities(glm::vec3 position, const QString& entityHostType) {
    auto& clipboard = qApp->getEntityClipboard();
    return BLOCKING_INVOKE(qApp, [&] { return clipboard.pasteEntities(entityHostType, position.x, position.y, position.z); });
}
