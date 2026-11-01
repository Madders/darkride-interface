//
//  EntityClipboard.h
//  interface/src
//
//  The entities' clipboard: an entity tree of its own that scripts export entities to (as a JSON file), import a JSON
//  file or URL into, and paste into the world (the Clipboard scripting API; the editor's copy and paste go through it).
//  exportEntities() writes the given entities, or those within a cube, to a file with their positions relative to a
//  root; importEntities() reads a file or URL into the clipboard; pasteEntities() sends the clipboard's entities to the
//  world at a position; tree() is the clipboard itself, for its contents' dimensions. Application owns one, reached by
//  getEntityClipboard(); the Clipboard scripting interface hops to the main thread and calls it. These were
//  Application's _entityClipboard and its exportEntities, importEntities and pasteEntities (ARCHITECTURE.md §5.4 step
//  4, the fifth owner class).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_EntityClipboard_h
#define hifi_EntityClipboard_h

#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QVector>

#include <glm/glm.hpp>

#include <EntityItemID.h>
#include <EntityTree.h>

class EntityClipboard {
public:
    EntityClipboard();   // the tree made, with its root
    const EntityTreePointer& tree() const { return _tree; }   // the clipboard's own entity tree; null after shutdown()

    bool exportEntities(const QString& filename, const QVector<QUuid>& entityIDs, const glm::vec3* givenOffset = nullptr);
    bool exportEntities(const QString& filename, float x, float y, float z, float scale);   // the entities within the cube
    bool importEntities(const QString& url, const bool isObservable = true, const qint64 callerId = -1);
    QVector<EntityItemID> pasteEntities(const QString& entityHostType, float x, float y, float z);

    void shutdown();   // the tree emptied and released, before the octree processor terminates

private:
    EntityTreePointer _tree;
};

#endif // hifi_EntityClipboard_h
