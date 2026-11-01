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
#include "RenderableMaterialEntityItem.h"
#include "InterfaceLogging.h"
#include "Application.h"
#include "avatar/AvatarManager.h"
#include <ResourceCache.h>
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


float SafeLanding::effectiveRadius() const {
    float wholeWorld = _visualsRadius.load();
    return (_waitForVisuals && wholeWorld >= 0.0f) ? wholeWorld : _nearbyRadius;
}

// While landing, what the landing waits for loads first: the entities within the radius it waits within, when it
// waits for drawing (a loading screen is up); then the solid ones physics waits for; and within each, the usual order
// (the nearest and biggest first). High Fidelity's rule here put the collisionless first, by mistake, and dropped the
// distance, so a world's nearby models loaded no sooner than its far ones (WORLD-LOADING.md, 10-18).
float SafeLanding::loadingPriority(const EntityItem& entityItem) const {
    const float WAITED_FOR_PRIORITY = 20.0f;
    const float COLLIDABLE_PRIORITY = 10.0f;
    float priority = _prevEntityLoadingPriorityOperator ? _prevEntityLoadingPriorityOperator(entityItem) : 0.0f;
    if (!entityItem.getCollisionless()) {
        priority += COLLIDABLE_PRIORITY;
    }
    if (_waitForVisuals) {
        float radius = effectiveRadius();
        bool waitedFor = true;
        if (radius > 0.0f) {
            bool success = false;
            AABox box = entityItem.getAABox(success);
            if (success) {
                glm::vec3 center = DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldPosition();
                glm::vec3 closest = glm::clamp(center, box.getMinimumPoint(), box.getMaximumPoint());
                waitedFor = glm::distance(center, closest) <= radius;
            }
        }
        if (waitedFor) {
            priority += WAITED_FOR_PRIORITY;
        }
    }
    return priority;
}

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
    bool started = false;
    if (!entityTreeRenderer.isNull()) {
        auto entityTree = entityTreeRenderer->getTree();
        if (entityTree && !_trackingEntities) {
            started = true;
            Locker lock(_lock);
            // (this lock is released before the seed below takes the tree's)
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
            _downloadsIdleSince = 0;
            _releasedStalled = 0;
            _receivedEntityCount = 0;
            _blockingCount = 0;
            _nearbyRadius = nearbyRadiusSetting.get();

            connect(std::const_pointer_cast<EntityTree>(entityTree).get(),
                &EntityTree::addingEntity, this, &SafeLanding::addTrackedEntity, Qt::DirectConnection);
            connect(std::const_pointer_cast<EntityTree>(entityTree).get(),
                &EntityTree::deletingEntity, this, &SafeLanding::deleteTrackedEntity);

            // the loading priority, once: a landing begun while this one's is still installed must not take its own for
            // the one before (loadingPriority calls it: the client overflowed its stack on a hop into a whole-world
            // world, 10-21, after a landing ended by reset() had left its function in place)
            if (!_priorityInstalled) {
                _prevEntityLoadingPriorityOperator = EntityTreeRenderer::getEntityLoadingPriorityOperator();
                EntityTreeRenderer::setEntityLoadingPriorityFunction([this](const EntityItem& item) { return loadingPriority(item); });
                _priorityInstalled = true;
            }
        }
    }
    if (started) {
        seedNearby(entityTreeRenderer);
    }
}

// What is already near the avatar is tracked again: a landing restarted by a teleport (the arrival's own path lookup,
// a hop within the world) lands among entities the tree already holds, which only an earlier landing checked, at
// another place. The tree's read lock is taken with this object's lock NOT held: the entity stream adds entities under
// the tree's write lock (OctreeProcessor::processDatagram) and signals addingEntity from there, a direct connection
// into addTrackedEntity, which takes this object's lock; with the sphere read under this lock, the two orders crossed,
// and a landing restarted while the stream arrived could deadlock the client with the octree thread (avatar-ai's
// survey, ARCHITECTURE.md 3.12, 10-22). The signals are connected before the sphere is read, so an entity added
// meanwhile is tracked either way; trackEntity takes an entity once.
void SafeLanding::seedNearby(const QSharedPointer<EntityTreeRenderer>& entityTreeRenderer) {
    if (entityTreeRenderer.isNull() || !_trackingEntities) {
        return;
    }
    auto entityTree = entityTreeRenderer->getTree();
    if (!entityTree) {
        return;
    }
    float seedRadius = effectiveRadius();
    if (seedRadius < 0.0f) {
        return;
    }
    // (0 is whole-world loading's "every entity": all the tree holds)
    const float EVERYWHERE = 100000.0f;
    seedRadius = seedRadius > 0.0f ? seedRadius : EVERYWHERE;
    glm::vec3 center = DependencyManager::get<AvatarManager>()->getMyAvatar()->getWorldPosition();
    PickFilter::Flags flags;
    flags.set(PickFilter::DOMAIN_ENTITIES); flags.set(PickFilter::VISIBLE); flags.set(PickFilter::INVISIBLE);
    flags.set(PickFilter::COLLIDABLE); flags.set(PickFilter::NONCOLLIDABLE);
    QVector<QUuid> nearby;
    auto tree = std::const_pointer_cast<EntityTree>(entityTree);
    tree->withReadLock([&] { tree->evalEntitiesInSphere(center, seedRadius, PickFilter(flags), nearby); });

    Locker lock(_lock);
    if (!_trackingEntities) {
        return;   // the landing ended while the sphere was read
    }
    for (const QUuid& id : nearby) {
        trackEntity(id, tree);
    }
    _seededCount = (int)nearby.size();
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
            _receivedEntityCount++;
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
    float radius = effectiveRadius();
    if (radius <= 0.0f) {
        return true;
    }
    bool success = false;
    AABox box = entity->getAABox(success);
    if (entity->getType() == EntityTypes::Material) {
        // a material is as near as what it paints: a ground's material sits at the ground's origin, which may be
        // far from where the avatar stands on it
        bool parentFound = false;
        auto parent = entity->getParentPointer(parentFound);
        if (parentFound && parent && parent->getNestableType() == NestableType::Entity) {
            bool parentSuccess = false;
            AABox parentBox = std::static_pointer_cast<EntityItem>(parent)->getAABox(parentSuccess);
            if (parentSuccess) {
                box = parentBox;
                success = true;
            }
        }
    }
    if (!success) {
        return true;
    }
    glm::vec3 closest = glm::clamp(avatarPosition, box.getMinimumPoint(), box.getMaximumPoint());
    return glm::distance(avatarPosition, closest) <= radius;
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
                    qDebug() << "arrival trace: released" << qPrintable(describe(entity)) << ": beyond" << effectiveRadius() << "m,"
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
                if (entity->getType() == EntityTypes::Material) {
                    // a material entity draws through its parent, so its own flag says nothing: what it draws must
                    // have loaded (its textures; a procedural one's shader and image channels)
                    // (no renderer yet is not ready: it has not begun to load; one that never comes is let go with the
                    // downloads that never come, below)
                    auto materialRenderable = std::dynamic_pointer_cast<render::entities::MaterialEntityRenderer>(entityRenderable);
                    isVisuallyReady = materialRenderable && materialRenderable->isMaterialReady();
                }
                if (!isVisuallyReady && entity->getType() == EntityTypes::Model) {
                    // the model and its textures in counts as drawn even before its renderer updates again: a far model
                    // that nothing else updates kept its "not drawn" flag for good, with everything loaded (10-21)
                    auto modelEntity = std::dynamic_pointer_cast<RenderableModelEntityItem>(entity);
                    isVisuallyReady = modelEntity && modelEntity->isModelLoadedWithTextures();
                }
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
        // What waits for a download that will never come (a link no request can be made for, as a tea:// model on a
        // build without the tea protocol plugin, or one that neither loads nor fails) does not hold the landing: once
        // the resource caches have had nothing in flight or queued for a few seconds while entities still wait, nothing
        // more is coming for them, and they are let go. Whole-world loading met it in the Tivoli Town copy, 10-21: 460
        // trees from four tea:// models held the screen to its time limit with no download in flight.
        const quint64 STALLED_DOWNLOADS_USECS = 3 * USECS_PER_SECOND;
        quint64 now = usecTimestampNow();
        bool downloadsIdle = ResourceCache::getLoadingRequestCount() == 0 && ResourceCache::getPendingRequestCount() == 0;
        if (!downloadsIdle || _trackedEntities.empty()) {
            _downloadsIdleSince = 0;
        } else if (_downloadsIdleSince == 0) {
            _downloadsIdleSince = now;
        } else if (now - _downloadsIdleSince > STALLED_DOWNLOADS_USECS) {
            // only what waits to be drawn: an entity not yet ready for physics (its collision shape still being made,
            // which takes no download) keeps holding the landing, so the avatar cannot fall through it
            QStringList released;
            int count = 0;
            for (auto it = _trackedEntities.begin(); it != _trackedEntities.end();) {
                if (isEntityPhysicsReady(it->second)) {
                    if (released.size() < 4) {
                        released << describe(it->second);
                    }
                    ++count;
                    it = _trackedEntities.erase(it);
                } else {
                    ++it;
                }
            }
            if (count > 0) {
                _releasedStalled += count;
                qDebug() << "safe landing: let go of" << count << "entities waiting to be drawn, for downloads none of which is"
                    << "in flight or queued (for" << (now - _downloadsIdleSince) / USECS_PER_MSEC << "ms), such as"
                    << qPrintable(released.join(", "));
            }
            _downloadsIdleSince = 0;
        }
        _blockingCount = (int32_t)_trackedEntities.size();   // what is left is near and not ready
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
                    // a model's own state: whether its renderer has made it, its geometry and its textures
                    QString state;
                    if (entity->getType() == EntityTypes::Model) {
                        auto modelEntity = std::dynamic_pointer_cast<RenderableModelEntityItem>(entity);
                        auto renderable = _entityTreeRenderer->renderableForEntityId(entry.first);
                        state = QString(" (%1, %2)").arg(modelEntity ? modelEntity->loadStateForLog() : QString("not a renderable model"),
                            renderable ? "renderer made" : "no renderer");
                    }
                    examples << QString("%1 '%2' %3 m %4%5%6").arg(EntityTypes::getEntityTypeName(entity->getType()), entity->getName(),
                        QString::number(glm::distance(avatarPosition, entity->getWorldPosition()), 'f', 1),
                        physicsReady ? "" : "physics ", visuallyReady ? "" : "drawing", state);
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
                << effectiveRadius() << "m (" << physicsWaiting << "not ready for physics," << visualsWaiting << "not drawn; added" << _addedCount
                << "of which" << _seededCount << "already held"
                << ", not tracked: created after the start" << _notTrackedCreatedLater << ", not found" << _notTrackedNotFound
                << ", local" << _notTrackedLocal << "), sequence"
                << _sequenceStart << "to" << _sequenceEnd << "," << _sequenceNumbers.size() << "packets seen, gaps ["
                << qPrintable(gaps.join(" ")) << "], missing" << qApp->isMissingSequenceNumbers()
                << (examples.isEmpty() ? "" : qPrintable("; waiting for " + examples.join("; ")));
            // the downloads in flight and queued: the resource cache loads a few at a time, and a stall there holds
            // every model behind it (whole-world loading, 10-21)
            QStringList inFlight;
            for (const auto& resource : ResourceCache::getLoadingRequests()) {
                if (resource && inFlight.size() < 6) {
                    inFlight << resource->getURL().toString().right(70);
                }
            }
            qDebug() << "safe landing: downloads" << ResourceCache::getLoadingRequestCount() << "in flight,"
                << ResourceCache::getPendingRequestCount() << "queued:" << qPrintable(inFlight.join(" | "));
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
    }
    if (_priorityInstalled) {
        EntityTreeRenderer::setEntityLoadingPriorityFunction(_prevEntityLoadingPriorityOperator);
        _priorityInstalled = false;
    }
}

// The end of a landing (physics on): what tracking took (the tree's signals, the loading priority) is given back as when
// it completes, then everything is cleared for the next. Until 10-21 this only cleared, leaving both in place.
void SafeLanding::reset() {
    stopTracking();
    Locker lock(_lock);
    _trackedEntities.clear();
    _maxTrackedEntityCount = 0;
    _receivedEntityCount = 0;
    _blockingCount = 0;
    _sequenceStart = SafeLanding::INVALID_SEQUENCE;
    _sequenceEnd = SafeLanding::INVALID_SEQUENCE;
    _firstSequence = SafeLanding::INVALID_SEQUENCE;
}

SafeLanding::Progress SafeLanding::getProgress() {
    Locker lock(_lock);
    Progress progress;
    progress.received = _receivedEntityCount;
    progress.waiting = _blockingCount;
    progress.serverDone = _sequenceStart != SafeLanding::INVALID_SEQUENCE;
    return progress;
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
