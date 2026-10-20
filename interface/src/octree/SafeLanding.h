//
//  SafeLanding.h
//  interface/src/octree
//
//  Created by Simon Walton.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// Controller for logic to wait for local collision bodies before enabling physics.

#ifndef hifi_SafeLanding_h
#define hifi_SafeLanding_h

#include <atomic>

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

#include <set>

#include "EntityItem.h"
#include "EntityDynamicInterface.h"

#include "EntityTreeRenderer.h"

class EntityTreeRenderer;
class EntityItemID;

class SafeLanding : public QObject {
public:
    static constexpr OCTREE_PACKET_SEQUENCE MAX_SEQUENCE = std::numeric_limits<OCTREE_PACKET_SEQUENCE>::max();
    static constexpr OCTREE_PACKET_SEQUENCE INVALID_SEQUENCE = MAX_SEQUENCE; // not technically invalid, but close enough

    void startTracking(QSharedPointer<EntityTreeRenderer> entityTreeRenderer);
    void updateTracking();
    void stopTracking();
    void reset();
    bool isTracking() const { return _trackingEntities; }
    // wait until the entities are also drawn (models and textures loaded), as interstitial mode does
    void setWaitForVisuals(bool wait) { _waitForVisuals = wait; }
    bool trackingIsComplete() const;

    void finishSequence(OCTREE_PACKET_SEQUENCE last);  // the number after the scene's last packet; the start is the first seen since tracking began
    void addToSequence(OCTREE_PACKET_SEQUENCE sequenceNumber);
    float getNearbyRadius() const { return _nearbyRadius; }
    float loadingProgressPercentage();

private slots:
    void addTrackedEntity(const EntityItemID& entityID);
    void deleteTrackedEntity(const EntityItemID& entityID);

private:
    void trackEntity(const EntityItemID& entityID, const EntityTreePointer& entityTree);
    bool isNearby(const EntityItemPointer& entity, const glm::vec3& avatarPosition) const;
    bool isEntityPhysicsReady(const EntityItemPointer& entity);
    void debugDumpSequenceIDs() const;

    std::mutex _lock;
    using Locker = std::lock_guard<std::mutex>;
    bool _trackingEntities { false };
    std::atomic<bool> _waitForVisuals { false };
    QSharedPointer<EntityTreeRenderer> _entityTreeRenderer;
    using EntityMap = std::map<EntityItemID, EntityItemPointer>;
    EntityMap _trackedEntities;

    OCTREE_PACKET_SEQUENCE _sequenceStart { INVALID_SEQUENCE };
    OCTREE_PACKET_SEQUENCE _sequenceEnd { INVALID_SEQUENCE };
    OCTREE_PACKET_SEQUENCE _firstSequence { INVALID_SEQUENCE };   // the first packet seen since tracking began
    float _nearbyRadius { 10.0f };
    int _seededCount { 0 };
    int32_t _maxTrackedEntityCount { 0 };
    int32_t _trackedEntityStabilityCount { 0 };
    quint64 _lastProgressLog { 0 };
    int _addedCount { 0 };               // diagnostics: entities tracked since the start, and those not tracked, by reason
    int _notTrackedCreatedLater { 0 };
    int _notTrackedNotFound { 0 };
    int _notTrackedLocal { 0 };
    int _completionCount { 0 };

    quint64 _startTime { 0 };

    struct SequenceLessThan {
        bool operator()(const OCTREE_PACKET_SEQUENCE& a, const OCTREE_PACKET_SEQUENCE& b) const;
    };

    using SequenceSet = std::set<OCTREE_PACKET_SEQUENCE, SequenceLessThan>;
    SequenceSet _sequenceNumbers;

    static CalculateEntityLoadingPriority entityLoadingOperatorElevateCollidables;
    CalculateEntityLoadingPriority _prevEntityLoadingPriorityOperator { nullptr };
};

#endif  // hifi_SafeLanding_h
