//
//  EditorClipboard.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorClipboard.h"

#include "EditorUndo.h"

void EditorClipboard::copy(const QList<QUuid>& ids, const glm::vec3& anchor) {
    _properties.clear();
    _ids.clear();
    _topLevel.clear();
    _anchor = anchor;

    QList<QUuid> all = EditorUndo::withDescendants(ids);
    QSet<QUuid> inBatch(all.begin(), all.end());
    for (const auto& id : all) {
        EntityItemProperties properties;
        if (!EditorUndo::capture(id, properties)) {
            continue;
        }
        _properties << properties;
        _ids << id;
        if (!inBatch.contains(properties.getParentID())) {
            _topLevel.insert(id);
        }
    }
}

void EditorClipboard::set(const QVector<EntityItemProperties>& properties, const QList<QUuid>& ids, const glm::vec3& anchor) {
    _properties = properties;
    _ids = ids;
    _topLevel.clear();
    _anchor = anchor;
    QSet<QUuid> inBatch(ids.begin(), ids.end());
    for (int i = 0; i < _properties.size() && i < _ids.size(); ++i) {
        if (!inBatch.contains(_properties[i].getParentID())) {
            _topLevel.insert(_ids[i]);
        }
    }
}

QList<QUuid> EditorClipboard::paste(EditorUndo& undo, const QString& text, const glm::vec3& offset) const {
    if (_properties.isEmpty()) {
        return QList<QUuid>();
    }
    QVector<EntityItemProperties> properties = _properties;
    if (offset != glm::vec3(0.0f)) {
        for (int i = 0; i < properties.size(); ++i) {
            if (_topLevel.contains(_ids[i])) {
                // moved in the world; with the local position unmarked, the add works out where that is in
                // the parent's frame (a copy stays attached to a parent outside the copy, like a palm on the
                // island). Children follow their copied parent and keep their local positions.
                properties[i].setPosition(properties[i].getPosition() + offset);
                properties[i].setLocalPositionChanged(false);
            }
        }
    }
    // the originals are the sources: copied children go onto the copied parents, and the originals keep
    // their ids (a paste makes new entities, nothing is remapped)
    QList<QUuid> newIds = undo.pushAdd(text, properties, _ids);
    QList<QUuid> topLevel;
    for (int i = 0; i < newIds.size() && i < _ids.size(); ++i) {
        if (_topLevel.contains(_ids[i]) && !newIds[i].isNull()) {
            topLevel << newIds[i];
        }
    }
    return topLevel;
}
