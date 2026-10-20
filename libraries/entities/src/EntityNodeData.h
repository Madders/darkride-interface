//
//  EntityNodeData.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 4/29/14
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityNodeData_h
#define hifi_EntityNodeData_h

#include <udt/PacketHeaders.h>

#include <OctreeQueryNode.h>

namespace EntityJSONQueryProperties {
    static const QString SERVER_SCRIPTS_PROPERTY = "serverScripts";
    static const QString TRACK_ID_PROPERTY = "trackID";
    static const QString FRAME_PROPERTY = "frame";
    static const QString ENTITY_TYPE_PROPERTY = "type";
    static const QString VEHICLE_PROPERTY = "vehicle";       // "+": a vehicle chassis or wheel (PHYSICS.md round 6)
    static const QString COLLIDABLE_PROPERTY = "collidable"; // "+": collidable with a primitive shape
    static const QString FLAGS_PROPERTY = "flags";
    static const QString INCLUDE_ANCESTORS_PROPERTY = "includeAncestors";
    static const QString INCLUDE_DESCENDANTS_PROPERTY = "includeDescendants";
    // true: the client draws impostor cards, so a carded entity (and the models under it) is sent out to where its
    // card is drawn (ENTITY-STREAMING.md 3.3, DiffTraversal::View::computePriority); a server that does not know
    // the flag sends as before
    static const QString IMPOSTOR_CARDS_PROPERTY = "impostorCards";
}

class EntityNodeData : public OctreeQueryNode {
public:
    virtual PacketType getMyPacketType() const override { return PacketType::EntityData; }

    quint64 getLastDeletedEntitiesSentAt() const { return _lastDeletedEntitiesSentAt; }
    void setLastDeletedEntitiesSentAt(quint64 sentAt) { _lastDeletedEntitiesSentAt = sentAt; }
    
    // these can only be called from the OctreeSendThread for the given Node
    void insertSentFilteredEntity(const QUuid& entityID) { _sentFilteredEntities.insert(entityID); }
    void removeSentFilteredEntity(const QUuid& entityID) { _sentFilteredEntities.remove(entityID); }
    bool sentFilteredEntity(const QUuid& entityID) const { return _sentFilteredEntities.contains(entityID); }
    QSet<QUuid> getSentFilteredEntities() { return _sentFilteredEntities; }

    // the following flagged extra entity methods can only be called from the OctreeSendThread for the given Node

    // inserts the extra entity and returns a boolean indicating wether the extraEntityID was a new addition
    bool insertFlaggedExtraEntity(const QUuid& filteredEntityID, const QUuid& extraEntityID);
    
    bool isEntityFlaggedAsExtra(const QUuid& entityID) const;
    void resetFlaggedExtraEntities() { _previousFlaggedExtraEntities = _flaggedExtraEntities; _flaggedExtraEntities.clear(); }

private:
    quint64 _lastDeletedEntitiesSentAt { usecTimestampNow() };
    QSet<QUuid> _sentFilteredEntities;
    QHash<QUuid, QSet<QUuid>> _flaggedExtraEntities;
    QHash<QUuid, QSet<QUuid>> _previousFlaggedExtraEntities;
};

#endif // hifi_EntityNodeData_h
