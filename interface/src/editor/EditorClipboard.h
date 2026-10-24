//
//  EditorClipboard.h
//  interface/src/editor
//
//  Copies of entities, with their descendants, ready to paste back as new entities.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorClipboard_h
#define hifi_EditorClipboard_h

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <QtCore/QVector>

#include <glm/glm.hpp>

#include <EntityItemProperties.h>

class EditorUndo;

class EditorClipboard {
public:
    // copies the entities and their descendants; the anchor is the point a paste lands on
    void copy(const QList<QUuid>& ids, const glm::vec3& anchor);
    // takes entities from elsewhere (an imported file), as an add takes them, parents before children; those whose
    // parent is not among them are the top level a paste moves
    void set(const QVector<EntityItemProperties>& properties, const QList<QUuid>& ids, const glm::vec3& anchor);
    bool isEmpty() const { return _properties.isEmpty(); }
    const glm::vec3& anchor() const { return _anchor; }

    // creates new entities from the copies, moved by the offset; returns the new top-level ids
    QList<QUuid> paste(EditorUndo& undo, const QString& text, const glm::vec3& offset = glm::vec3(0.0f)) const;

private:
    QVector<EntityItemProperties> _properties;
    QList<QUuid> _ids;
    QSet<QUuid> _topLevel;
    glm::vec3 _anchor;
};

#endif // hifi_EditorClipboard_h
