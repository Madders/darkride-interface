//
//  SafeLanding.cpp
//  interface/src/octree
//
//  Created by Simon Walton.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SafeLanding.h"
#include <SharedUtil.h>

#include "EntityTreeRenderer.h"
#include "RenderableModelEntityItem.h"
#include "InterfaceLogging.h"
#include "Application.h"
#include "avatar/AvatarManager.h"
#include <SettingHandle.h>
#include <PickFilter.h>

// The arrival gate (WORLD-LOADING.md): the landing waits for what is near the avatar, within this many metres of its
// body (the entity's box, not its centre: a floor you stand on counts), to be ready for physics and, while a loading
// screen asks for it, drawn. Nothing farther holds the arrival. The default is the radius of the client's initial
// entity query (Application.cpp INITIAL_QUERY_RADIUS); the setting is for Caity to tune.
static Setting::Handle<float> nearbyRadiusSetting { "arrival/nearbyRadius", 10.0f };

// TIVOLI_ARRIVAL_TRACE=1: a line per entity tracked and released, with the reason
static bool arrivalTrace() {
    static const bool trace = !qgetenv("TIVOLI_ARRIVAL_TRACE").isEmpty();
    return trace;
}

static QString describe(const EntityItemPointer& entity) {
    return QString("%1 '%2'").arg(EntityTypes::getEntityTypeName(entity->getType()), entity->getName());
}


CalculateEntityLoadingPriority SafeLanding::entityLoadingOperatorElevateCollidables = [](const EntityItem& entityItem) {
    const int COLLIDABLE_ENTITY_PRIORITY = 10.0f;
    return entityItem.getCollisionless() * COLLIDABLE_ENTITY_PRIORITY;
};

namespace {
    template<typename T> bool lessThanWraparound(int32_t a, int32_t b) {
        constexpr int32_t MAX_T_VALUE = std::numeric_limits<T>::max();
        if (b <= a) {
            b += MAX_T_VALUE;
        }
        return (b - a) < (MAX_T_VALUE / 2);
    }
}

bool SafeLanding::SequenceLessThan::operator()(const OCTREE_PACKET_SEQUENCE& a, const OCTREE_PACKET_SEQUENCE& b) const {
    return lessThanWraparound<OCTREE_PACKET_SEQUENCE>(a, b);
}

void SafeLanding::startTracking(QSharedPointer<EntityTreeRenderer> entityTreeRenderer) {
    if (!entityTreeRenderer.isNull()) {
        auto entityTree = entityTreeRenderer->getTree();
        if (entityTree && !_trackingEntities) {
            Locker lock(_lock);
            _entityTreeRenderer = entityTreeRenderer;
            _trackedEntities.clear();
            _maxTrackedEntityCount = 0;
            _sequenceStart = SafeLanding::INVALID_SEQUENCE;
            _sequenceEnd = SafeLanding::INVALID_SEQUENCE;
            _firstSequence = SafeLanding::INVALID_SEQUENCE;
            _sequenceNumbers.clear();
            _trackingEntities = true;
            _startTime = usecTimestampNow();
            _addedCount = 0;
            _notTrackedCreatedLater = 0;
            _notTrackedNotFound = 0;
            _notTrackedLocal = 0;
            _completionCount = 0;
            _nearbyRadius = nearbyRadiusSetting.get();

            connect(std::const_pointer_cast<EntityTree>(entityTree).get(),
                &EntityTree::addingEntity, this, &SafeLanding::addTrackedEntity, Qt::DirectConnection);
            connect(std::const_pointer_cast<EntityTree>(entityTree).get(),
                &EntityTree::deletingEntity, this, &SafeLanding::deleteTrackedEntity);

            _prevEntityLoadingPriorityOperator = EntityTreeRenderer::getEntityLoadingPriorityOperator();
            EntityTreeRenderer::setEntityLoadingPriorityFunction(entityLoadingOperatorElevateCollidables);

            // What is already near the avatar is tracked again: a landing restarted by a teleport (the arrival's own
            // path lookup, a hop within the world) lands among entities the tree already holds, which only an earlier
            // landing checked, at another place
            if (_nearbyRadius > 0.0f) {
                glm::vec3 center = DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldPosition();
                PickFilter::Flags flags;
                flags.set(PickFilter::DOMAIN_ENTITIES); flags.set(PickFilter::VISIBLE); flags.set(PickFilter::INVISIBLE);
                flags.set(PickFilter::COLLIDABLE); flags.set(PickFilter::NONCOLLIDABLE);
                QVector<QUuid> nearby;
                auto tree = std::const_pointer_cast<EntityTree>(entityTree);
                tree->withReadLock([&] { tree->evalEntitiesInSphere(center, _nearbyRadius, PickFilter(flags), nearby); });
                for (const QUuid& id : nearby) {
                    trackEntity(id, tree);
                }
                _seededCount = (int)nearby.size();
            }
        }
    }
}

void SafeLanding::addTrackedEntity(const EntityItemID& entityID) {
    if (_trackingEntities && _entityTreeRenderer) {
        Locker lock(_lock);
        auto entityTree = _entityTreeRenderer->getTree();
        if (entityTree) {
            trackEntity(entityID, entityTree);
        }
    }
}

// with _lock held
void SafeLanding::trackEntity(const EntityItemID& entityID, const EntityTreePointer& entityTree) {
    EntityItemPointer entity = entityTree->findEntityByID(entityID);
    if (!entity) {
        _notTrackedNotFound++;
    } else if (entity->isLocalEntity()) {
        _notTrackedLocal++;
    } else if (entity->getCreated() >= _startTime) {
        _notTrackedCreatedLater++;
    }
    if (entity && !entity->isLocalEntity() && entity->getCreated() < _startTime) {
        if (_trackedEntities.emplace(entityID, entity).second) {
            _addedCount++;
            if (arrivalTrace()) {
                qDebug() << "arrival trace: tracking" << qPrintable(describe(entity)) << "drawn" << entity->isVisuallyReady()
                    << "collisionless" << entity->getCollisionless() << "shape" << entity->getShapeType()
                    << (usecTimestampNow() - _startTime) / 1000 << "ms after the start";
            }
        }

        int32_t trackedEntityCount = (int32_t)_trackedEntities.size();
        if (trackedEntityCount > _maxTrackedEntityCount) {
            _maxTrackedEntityCount = trackedEntityCount;
            _trackedEntityStabilityCount = 0;
        }
    }
}

// Whether the entity is within the nearby radius of the avatar: the distance from the avatar's position to the
// entity's box (a floor you stand on counts, wherever its centre is). Without a box, it counts as near.
bool SafeLanding::isNearby(const EntityItemPointer& entity, const glm::vec3& avatarPosition) const {
    if (_nearbyRadius <= 0.0f) {
        return true;
    }
    bool success = false;
    AABox box = entity->getAABox(success);
    if (!success) {
        return true;
    }
    glm::vec3 closest = glm::clamp(avatarPosition, box.getMinimumPoint(), box.getMaximumPoint());
    return glm::distance(avatarPosition, closest) <= _nearbyRadius;
}

void SafeLanding::deleteTrackedEntity(const EntityItemID& entityID) {
    Locker lock(_lock);
    _trackedEntities.erase(entityID);
}

// The entity server reports its initial scene complete with the sequence number after the scene's last packet. The
// window to wait for runs from the first packet seen since tracking began to that number; when nothing was seen since
// (the scene had nothing to add here), the window is empty and the landing can complete.
void SafeLanding::finishSequence(OCTREE_PACKET_SEQUENCE last) {
    Locker lock(_lock);
    if (_trackingEntities) {
        _sequenceStart = (_firstSequence != SafeLanding::INVALID_SEQUENCE) ? _firstSequence : last;
        _sequenceEnd = last;
        _completionCount++;
        qDebug() << "safe landing: the entity server reports the initial scene complete (" << _completionCount << "), sequence"
            << _sequenceStart << "to" << last << "," << _sequenceNumbers.size() << "packets seen,"
            << (usecTimestampNow() - _startTime) / 1000 << "ms after tracking began";
    }
}

void SafeLanding::addToSequence(OCTREE_PACKET_SEQUENCE sequenceNumber) {
    Locker lock(_lock);
    if (!_trackingEntities) {
        return;
    }
    if (_firstSequence == SafeLanding::INVALID_SEQUENCE) {
        _firstSequence = sequenceNumber;
    }
    _sequenceNumbers.insert(sequenceNumber);
}

void SafeLanding::updateTracking() {
    if (!_trackingEntities || !_entityTreeRenderer) {
        return;
    }

    {
        Locker lock(_lock);
        bool enableInterstitial = DependencyManager::get<NodeList>()->getDomainHandler().getInterstitialModeEnabled() ||
            _waitForVisuals;
        glm::vec3 avatarPosition = DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldPosition();
        auto entityMapIter = _trackedEntities.begin();
        while (entityMapIter != _trackedEntities.end()) {
            auto entity = entityMapIter->second;
            // what lies beyond the nearby radius does not hold the arrival (the initial query's far view, at the lowest
            // level of detail, brings the whole visible world; its far models' collision shapes and textures are no
            // more needed on arrival than they are later)
            if (!isNearby(entity, avatarPosition)) {
                if (arrivalTrace()) {
                    qDebug() << "arrival trace: released" << qPrintable(describe(entity)) << ": beyond" << _nearbyRadius << "m,"
                        << (usecTimestampNow() - _startTime) / 1000 << "ms after the start";
                }
                entityMapIter = _trackedEntities.erase(entityMapIter);
                continue;
            }
            bool isVisuallyReady = true;
            bool hasRenderable = true;
            if (enableInterstitial) {
                auto entityRenderable = _entityTreeRenderer->renderableForEntityId(entityMapIter->first);
                hasRenderable = (bool)entityRenderable;
                if (!entityRenderable) {
                    _entityTreeRenderer->addingEntity(entityMapIter->first);
                }
                isVisuallyReady = entity->isVisuallyReady() || (!entityRenderable && !entity->isParentPathComplete());
            }
            if (isEntityPhysicsReady(entity) && isVisuallyReady) {
                if (arrivalTrace()) {
                    qDebug() << "arrival trace: released" << qPrintable(describe(entity)) << ": ready (drawn" << entity->isVisuallyReady()
                        << "renderable" << hasRenderable << "waiting for visuals" << enableInterstitial << ")"
                        << (usecTimestampNow() - _startTime) / 1000 << "ms after the start";
                }
                entityMapIter = _trackedEntities.erase(entityMapIter);
            } else {
                entityMapIter++;
            }
        }
        if (enableInterstitial) {
            _trackedEntityStabilityCount++;
        }
    }

    {
        // a line a second while tracking (PHYSICS.md round 10: what the loading screen waits for)
        quint64 now = usecTimestampNow();
        if (now - _lastProgressLog > USECS_PER_SECOND) {
            _lastProgressLog = now;
            Locker lock(_lock);
            bool enableInterstitial = DependencyManager::get<NodeList>()->getDomainHandler().getInterstitialModeEnabled() ||
                _waitForVisuals;
            int physicsWaiting = 0, visualsWaiting = 0;
            QStringList examples;
            glm::vec3 avatarPosition = DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldPosition();
            for (const auto& entry : _trackedEntities) {
                const auto& entity = entry.second;
                bool physicsReady = isEntityPhysicsReady(entity);
                bool visuallyReady = !enableInterstitial || entity->isVisuallyReady();
                if (!physicsReady) { physicsWaiting++; }
                if (!visuallyReady) { visualsWaiting++; }
                if (examples.size() < 4 && (!physicsReady || !visuallyReady)) {
                    examples << QString("%1 '%2' %3 m %4%5").arg(EntityTypes::getEntityTypeName(entity->getType()), entity->getName(),
                        QString::number(glm::distance(avatarPosition, entity->getWorldPosition()), 'f', 1),
                        physicsReady ? "" : "physics ", visuallyReady ? "" : "drawing");
                }
            }
            QStringList gaps;
            if (_sequenceStart != SafeLanding::INVALID_SEQUENCE) {
                for (OCTREE_PACKET_SEQUENCE n = _sequenceStart; n != _sequenceEnd && gaps.size() < 8; ++n) {
                    if (_sequenceNumbers.find(n) == _sequenceNumbers.end()) {
                        gaps << QString::number(n);
                    }
                }
            }
            qDebug() << "safe landing: tracking" << _trackedEntities.size() << "of" << _maxTrackedEntityCount << "entities within"
                << _nearbyRadius << "m (" << physicsWaiting << "not ready for physics," << visualsWaiting << "not drawn; added" << _addedCount
                << "of which" << _seededCount << "already held"
                << ", not tracked: created after the start" << _notTrackedCreatedLater << ", not found" << _notTrackedNotFound
                << ", local" << _notTrackedLocal << "), sequence"
                << _sequenceStart << "to" << _sequenceEnd << "," << _sequenceNumbers.size() << "packets seen, gaps ["
                << qPrintable(gaps.join(" ")) << "], missing" << qApp->isMissingSequenceNumbers()
                << (examples.isEmpty() ? "" : qPrintable("; waiting for " + examples.join("; ")));
        }
    }
    if (_trackedEntities.empty()) {
        // no more tracked entities --> check sequenceNumbers
        if (_sequenceStart != SafeLanding::INVALID_SEQUENCE) {
            bool shouldStop = false;
            {
                Locker lock(_lock);
                auto sequenceSize = _sequenceEnd - _sequenceStart; // this works even in rollover case
                auto startIter = _sequenceNumbers.find(_sequenceStart);
                auto endIter = _sequenceNumbers.find(_sequenceEnd - 1);

                bool missingSequenceNumbers = qApp->isMissingSequenceNumbers();
                shouldStop = (sequenceSize == 0 ||
                    (startIter != _sequenceNumbers.end() &&
                     endIter != _sequenceNumbers.end() &&
                     ((distance(startIter, endIter) == sequenceSize - 1) || !missingSequenceNumbers)));
            }
            if (shouldStop) {
                qDebug() << "safe landing: complete," << (usecTimestampNow() - _startTime) / 1000 << "ms after tracking began ("
                    << _maxTrackedEntityCount << "entities tracked, sequence" << _sequenceStart << "to" << _sequenceEnd << ")";
                stopTracking();
            }
        }
    }
}

void SafeLanding::stopTracking() {
    Locker lock(_lock);
    if (_trackingEntities) {
        _trackingEntities = false;
        if (_entityTreeRenderer) {
            auto entityTree = _entityTreeRenderer->getTree();
            disconnect(std::const_pointer_cast<EntityTree>(entityTree).get(),
                &EntityTree::addingEntity, this, &SafeLanding::addTrackedEntity);
            disconnect(std::const_pointer_cast<EntityTree>(entityTree).get(),
                &EntityTree::deletingEntity, this, &SafeLanding::deleteTrackedEntity);
            _entityTreeRenderer.reset();
        }
        EntityTreeRenderer::setEntityLoadingPriorityFunction(_prevEntityLoadingPriorityOperator);
    }
}

void SafeLanding::reset() {
    _trackingEntities = false;
    _trackedEntities.clear();
    _maxTrackedEntityCount = 0;
    _sequenceStart = SafeLanding::INVALID_SEQUENCE;
    _sequenceEnd = SafeLanding::INVALID_SEQUENCE;
    _firstSequence = SafeLanding::INVALID_SEQUENCE;
}

bool SafeLanding::trackingIsComplete() const {
    return !_trackingEntities && (_sequenceStart != SafeLanding::INVALID_SEQUENCE);
}

float SafeLanding::loadingProgressPercentage() {
    Locker lock(_lock);

    float entityReadyPercentage = 0.0f;
    if (_maxTrackedEntityCount > 0) {
        entityReadyPercentage = ((_maxTrackedEntityCount - _trackedEntities.size()) / (float)_maxTrackedEntityCount);
        qDebug() << "SAFE LANDING PROGRESS  " << _maxTrackedEntityCount << ". ERP IS " << entityReadyPercentage;
    }

    constexpr int32_t MINIMUM_TRACKED_ENTITY_STABILITY_COUNT = 15;
    if (_trackedEntityStabilityCount < MINIMUM_TRACKED_ENTITY_STABILITY_COUNT) {
        entityReadyPercentage *= 0.20f;
    }

    return entityReadyPercentage;
}

bool SafeLanding::isEntityPhysicsReady(const EntityItemPointer& entity) {
    if (entity && !entity->getCollisionless()) {
        const auto& entityType = entity->getType();
        if (entityType == EntityTypes::Model) {
            RenderableModelEntityItem * modelEntity = std::dynamic_pointer_cast<RenderableModelEntityItem>(entity).get();
            static const std::set<ShapeType> downloadedCollisionTypes
                { SHAPE_TYPE_COMPOUND, SHAPE_TYPE_SIMPLE_COMPOUND, SHAPE_TYPE_STATIC_MESH,  SHAPE_TYPE_SIMPLE_HULL };
            bool hasAABox;
            entity->getAABox(hasAABox);
            if (hasAABox && downloadedCollisionTypes.count(modelEntity->getShapeType()) != 0) {
                auto space = _entityTreeRenderer->getWorkloadSpace();
                uint8_t region = space ? space->getRegion(entity->getSpaceIndex()) : (uint8_t)workload::Region::INVALID;

                // Note: the meanings of the workload regions are:
                //   R1 = in physics simulation and willing to own simulation
                //   R2 = in physics simulation but does NOT want to own simulation
                //   R3 = not in physics simulation but kinematically animated when velocities are non-zero
                //   R4 = sorted by workload and found to be outside R3
                //   UNKNOWN = known to workload but not yet sorted
                //   INVALID = not known to workload
                // So any entity sorted into R3 or R4 is definitelyNotPhysical

                bool definitelyNotPhysical = region == workload::Region::R3 ||
                    region == workload::Region::R4 ||
                    !entity->shouldBePhysical() ||
                    modelEntity->unableToLoadCollisionShape();
                bool definitelyPhysical = entity->isInPhysicsSimulation();
                return definitelyNotPhysical || definitelyPhysical;
            }
        }
    }
    return true;
}

void SafeLanding::debugDumpSequenceIDs() const {
    qCDebug(interfaceapp) << "Sequence set size:" << _sequenceNumbers.size();

    auto itr = _sequenceNumbers.begin();
    OCTREE_PACKET_SEQUENCE p = SafeLanding::INVALID_SEQUENCE;
    if (itr != _sequenceNumbers.end()) {
        p = (*itr);
        qCDebug(interfaceapp) << "First:" << (int32_t)p;
        ++itr;
        while (itr != _sequenceNumbers.end()) {
            OCTREE_PACKET_SEQUENCE s = *itr;
            if (s != p + 1) {
                qCDebug(interfaceapp) << "Gap from" << (int32_t)p << "to" << (int32_t)s << "(exclusive)";
                p = s;
            }
            ++itr;
        }
        if (p != SafeLanding::INVALID_SEQUENCE) {
            qCDebug(interfaceapp) << "Last:" << p;
        }
    }
}
