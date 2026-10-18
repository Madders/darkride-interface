//
//  PhysicsEngine.cpp
//  libraries/physics/src
//
//  Created by Andrew Meadows 2014.10.29
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PhysicsEngine.h"

#include <algorithm>
#include <functional>
#include <thread>

#include <QFile>
#include <QTextStream>

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Constraints/Constraint.h>

#include <EntityItem.h>
#include <PerfStat.h>
#include <PhysicsCollisionGroups.h>
#include <Profile.h>
#include <SharedUtil.h>

#include "CharacterController.h"
#include "JoltSupport.h"
#include "JoltUtil.h"
#include "PhysicsHelpers.h"
#include "PhysicsDebugDraw.h"
#include "PhysicsLogging.h"

namespace {
    // the motion state of the entity a dynamic belongs to, if it has a body
    ObjectMotionState* ownerMotionState(EntityDynamicInterface* dynamic) {
        EntityItemPointer entity = dynamic ? dynamic->getOwnerEntity().lock() : nullptr;
        return entity ? static_cast<ObjectMotionState*>(entity->getPhysicsInfo()) : nullptr;
    }

    const JPH::uint TEMP_ALLOCATOR_BYTES = 64 * 1024 * 1024;
    // the hysteresis of a frame's membership volume: stepping in needs this much inside, stepping out this much beyond
    const float ENTER_MARGIN = 0.05f;
    const float LEAVE_MARGIN = 0.3f;
}

PhysicsEngine::PhysicsEngine(const glm::vec3& offset) :
        _originOffset(offset),
        _myAvatarController(nullptr) {
    // Jolt's allocator and types must exist before any Jolt object does, including the members of this class
    JoltSupport::initialize();
}

PhysicsEngine::~PhysicsEngine() {
    _myAvatarController = nullptr;
    // the worlds go before the job system and the allocator they use
    _frameWorlds.clear();
    _rootWorld.reset();
    _jobSystem.reset();
    _tempAllocator.reset();
}

void PhysicsEngine::init() {
    JoltSupport::initialize();
    if (!_rootWorld) {
        _tempAllocator.reset(new JPH::TempAllocatorImpl(TEMP_ALLOCATOR_BYTES));
        int numThreads = glm::max(1, (int)std::thread::hardware_concurrency() - 1);
        _jobSystem.reset(new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads));
        _rootWorld.reset(new PhysicsWorld(QUuid(), _tempAllocator.get(), _jobSystem.get()));

        _physicsDebugDraw.reset(new PhysicsDebugDraw());
        _physicsDebugDraw->setWorldOffset(_originOffset);

        _lastStepTime = usecTimestampNow();
        qCDebug(physics) << "PhysicsEngine: Jolt root world with" << numThreads << "worker threads";
    }
}

JPH::TempAllocator* PhysicsEngine::getTempAllocator() const {
    return _tempAllocator.get();
}

int32_t PhysicsEngine::getNumCollisionObjects() const {
    int32_t count = _rootWorld ? _rootWorld->getNumBodies() : 0;
    for (const auto& entry : _frameWorlds) {
        count += entry.second->getNumBodies();
    }
    return count;
}

PhysicsWorld* PhysicsEngine::findWorld(const QUuid& frameID) const {
    if (frameID.isNull()) {
        return _rootWorld.get();
    }
    auto itr = _frameWorlds.find(frameID);
    return itr == _frameWorlds.end() ? nullptr : itr->second.get();
}

PhysicsWorld* PhysicsEngine::getWorld(const QUuid& frameID) {
    if (frameID.isNull()) {
        return _rootWorld.get();
    }
    auto itr = _frameWorlds.find(frameID);
    if (itr != _frameWorlds.end()) {
        return itr->second.get();
    }
    PhysicsWorld* world = new PhysicsWorld(frameID, _tempAllocator.get(), _jobSystem.get());
    _frameWorlds[frameID].reset(world);
    glm::vec3 position, linearVelocity, angularVelocity;
    glm::quat rotation;
    if (getFrameTransform(frameID, position, rotation, linearVelocity, angularVelocity)) {
        world->setFrameTransform(position, rotation, linearVelocity, angularVelocity);
    }
    qCDebug(physics) << "PhysicsEngine: new world for frame" << frameID;
    return world;
}

PhysicsWorld* PhysicsEngine::worldFor(ObjectMotionState* motionState) {
    return getWorld(motionState->getFrameID());
}

bool PhysicsEngine::getFrameTransform(const QUuid& frameID, glm::vec3& position, glm::quat& rotation, glm::vec3& linearVelocity, glm::vec3& angularVelocity) const {
    auto itr = _frameStates.find(frameID);
    if (itr == _frameStates.end()) {
        return false;
    }
    return itr->second->getFrameTransformInDomain(position, rotation, linearVelocity, angularVelocity);
}

QUuid PhysicsEngine::frameContaining(const glm::vec3& position, const QUuid& currentFrame, bool supported) const {
    // a point in domain coordinates, each frame's volume in its own: the frame's transform converts
    auto inside = [&](ObjectMotionState* frameState, float margin) -> bool {
        glm::vec3 framePosition, linearVelocity, angularVelocity, minCorner, maxCorner;
        glm::quat frameRotation;
        if (!frameState->getFrameTransformInDomain(framePosition, frameRotation, linearVelocity, angularVelocity) ||
            !frameState->getFrameVolume(minCorner, maxCorner)) {
            return false;
        }
        glm::vec3 local = glm::inverse(frameRotation) * (position - framePosition);
        return local.x > minCorner.x - margin && local.x < maxCorner.x + margin &&
               local.y > minCorner.y - margin && local.y < maxCorner.y + margin &&
               local.z > minCorner.z - margin && local.z < maxCorner.z + margin;
    };
    // staying is easier than entering: a rider in a doorway does not flap between worlds. But only while standing
    // on the frame: a rider who has stepped past the floor's edge, with nothing of the frame under them, is out as
    // soon as they are outside the volume itself, so they meet the platform in the world instead of falling here.
    if (!currentFrame.isNull()) {
        auto itr = _frameStates.find(currentFrame);
        if (itr != _frameStates.end() && inside(itr->second, LEAVE_MARGIN) && (supported || inside(itr->second, 0.0f))) {
            return currentFrame;
        }
    }
    for (const auto& entry : _frameStates) {
        if (entry.first != currentFrame && inside(entry.second, -ENTER_MARGIN)) {
            return entry.first;
        }
    }
    return QUuid();
}

// private
PhysicsWorld* PhysicsEngine::characterWorld() const {
    return _myAvatarController ? _myAvatarController->getWorld() : nullptr;
}

// private
std::vector<EntityDynamicPointer> PhysicsEngine::removeDynamicsForObject(ObjectMotionState* motionState) {
    std::vector<EntityDynamicPointer> removed;
    JPH::Body* body = motionState->getBody();
    PhysicsWorld* world = motionState->getWorld();
    if (!body || !world) {
        return removed;
    }
    auto key = std::make_pair(world, body->GetID().GetIndexAndSequenceNumber());
    auto itr = _objectDynamicsByBody.find(key);
    if (itr == _objectDynamicsByBody.end()) {
        return removed;
    }
    QSet<QUuid> ids = itr->second;
    for (const QUuid& dynamicID : ids) {
        if (!dynamicID.isNull() && _objectDynamics.contains(dynamicID)) {
            EntityDynamicPointer dynamic = _objectDynamics[dynamicID];
            removeDynamic(dynamicID);
            if (dynamic) {
                removed.push_back(dynamic);
            }
        }
    }
    return removed;
}

// private: puts back the dynamics that couldn't go back on a rebuilt body at once
void PhysicsEngine::restoreDynamics() {
    if (_dynamicsToRestore.isEmpty()) {
        return;
    }
    // (addDynamic can remove a dynamic, which takes it out of _dynamicsToRestore too)
    QHash<QUuid, EntityDynamicPointer> pending;
    pending.swap(_dynamicsToRestore);
    for (const EntityDynamicPointer& dynamic : pending) {
        if (!addDynamic(dynamic)) {
            _dynamicsToRestore[dynamic->getID()] = dynamic;
        }
    }
}

// private: a frame is known to the engine (membership, transforms) and has its hull twin in its own world
void PhysicsEngine::registerFrame(ObjectMotionState* frameState) {
    const QUuid& frameID = frameState->getObjectID();
    if (_frameStates.find(frameID) == _frameStates.end()) {
        qCDebug(physics) << "PhysicsEngine: frame" << frameID << "registered, at" << frameState->getObjectPosition();
    }
    _frameStates[frameID] = frameState;
    _framesChanged = true;
    getWorld(frameID)->setFrameHull(frameState);
}

// private
void PhysicsEngine::unregisterFrame(const QUuid& frameID) {
    if (_frameStates.erase(frameID)) {
        _framesChanged = true;
        PhysicsWorld* world = findWorld(frameID);
        if (world) {
            world->setFrameHull(nullptr);
        }
    }
}

void PhysicsEngine::removeObjects(const VectorOfMotionStates& objects) {
    for (auto object : objects) {
        PhysicsWorld* world = object->getWorld();
        if (world) {
            removeDynamicsForObject(object);
            world->removeObject(object);
        }
        unregisterFrame(object->getObjectID());
    }
    dropEmptyWorlds();
}

// Same as above, but takes a Set instead of a Vector.  Should only be called during teardown.
void PhysicsEngine::removeSetOfObjects(const SetOfMotionStates& objects) {
    for (auto object : objects) {
        PhysicsWorld* world = object->getWorld();
        if (world) {
            removeDynamicsForObject(object);
            world->removeObject(object);
        }
        object->clearIncomingDirtyFlags();
    }
    std::vector<QUuid> frameIDs;
    for (const auto& entry : _frameStates) {
        frameIDs.push_back(entry.first);
    }
    for (const QUuid& frameID : frameIDs) {
        unregisterFrame(frameID);
    }
    dropEmptyWorlds();
}

void PhysicsEngine::addObjects(const VectorOfMotionStates& objects) {
    for (auto object : objects) {
        PhysicsWorld* world = worldFor(object);
        world->addObject(object);
        if (object->isFrame()) {
            registerFrame(object);
        }
    }
}

void PhysicsEngine::reinsertObject(ObjectMotionState* object) {
    if (!object->getBody()) {
        return;
    }
    PhysicsWorld* oldWorld = object->getWorld();
    PhysicsWorld* newWorld = worldFor(object);

    // The body is rebuilt (PhysicsWorld::addObject destroys it and creates a new one), and Jolt's constraints hold
    // pointers to their bodies: a constraint left on the old body crashed the next step, in
    // ConstraintManager::GetActiveConstraints, as a world with hinges loaded. They come off first, and go back on
    // the new body after, built afresh; one that can't yet (its other body isn't there) is retried every transaction.
    std::vector<EntityDynamicPointer> dynamics = removeDynamicsForObject(object);

    if (oldWorld && oldWorld != newWorld) {
        // the object changed frame: out of one world, into another, in the new frame's coordinates
        oldWorld->removeObject(object);
    }
    newWorld->addObject(object);
    if (object->isFrame()) {
        // (also refreshes the hull twin when the shape changed)
        registerFrame(object);
    } else {
        // in case it was a frame until now
        unregisterFrame(object->getObjectID());
    }

    for (const EntityDynamicPointer& dynamic : dynamics) {
        if (!addDynamic(dynamic)) {
            _dynamicsToRestore[dynamic->getID()] = dynamic;
        }
    }
}

void PhysicsEngine::processTransaction(PhysicsEngine::Transaction& transaction) {
    restoreDynamics();

    // removes
    for (auto object : transaction.objectsToRemove) {
        PhysicsWorld* world = object->getWorld();
        if (world) {
            removeDynamicsForObject(object);
            world->removeObject(object);
        }
        unregisterFrame(object->getObjectID());
    }

    // adds
    addObjects(VectorOfMotionStates::fromStdVector(transaction.objectsToAdd));

    // reinserts
    for (auto object : transaction.objectsToReinsert) {
        reinsertObject(object);
    }

    // static bodies that were moved: wake whatever is now touching them, and report them as changed
    for (auto object : transaction.activeStaticObjects) {
        if (object->getWorld()) {
            object->getWorld()->markStaticObjectActive(object);
        }
    }

    if (_framesChanged) {
        rehomeObjects();
    }
    dropEmptyWorlds();
}

// A frame came or went: things that were added before their frame was known (a child that arrived before
// its parent, the contents of an entity that just became a frame) move to the world they belong in.
void PhysicsEngine::rehomeObjects() {
    _framesChanged = false;
    std::vector<ObjectMotionState*> misplaced;
    auto check = [&](PhysicsWorld* world) {
        for (ObjectMotionState* motionState : world->getMotionStates()) {
            if (motionState->getFrameID() != world->getFrameID()) {
                misplaced.push_back(motionState);
            }
        }
    };
    check(_rootWorld.get());
    for (auto& entry : _frameWorlds) {
        check(entry.second.get());
    }
    for (ObjectMotionState* motionState : misplaced) {
        reinsertObject(motionState);
    }
    if (!misplaced.empty()) {
        qCDebug(physics) << "PhysicsEngine: moved" << misplaced.size() << "objects to their frames' worlds";
    }
}

// private
void PhysicsEngine::dropEmptyWorlds() {
    PhysicsWorld* keep = characterWorld();
    for (auto itr = _frameWorlds.begin(); itr != _frameWorlds.end();) {
        PhysicsWorld* world = itr->second.get();
        if (world != keep && world->getNumObjects() == 0 && _frameStates.find(itr->first) == _frameStates.end()) {
            qCDebug(physics) << "PhysicsEngine: dropping the empty world of frame" << itr->first;
            itr = _frameWorlds.erase(itr);
        } else {
            ++itr;
        }
    }
}

void PhysicsEngine::removeContacts(ObjectMotionState* motionState) {
    if (motionState && motionState->getWorld()) {
        motionState->getWorld()->removeContacts(motionState);
    } else {
        // the character has no motion state; its contacts are in its world
        PhysicsWorld* world = characterWorld();
        if (world) {
            world->removeContacts(motionState);
        }
    }
}

// private
void PhysicsEngine::feedFrameTransforms() {
    for (auto& entry : _frameWorlds) {
        glm::vec3 position, linearVelocity, angularVelocity;
        glm::quat rotation;
        if (getFrameTransform(entry.first, position, rotation, linearVelocity, angularVelocity)) {
            entry.second->setFrameTransform(position, rotation, linearVelocity, angularVelocity);
        }
    }
}

// private
void PhysicsEngine::stepOnce(float dt) {
    ++_numSubsteps;
    ObjectMotionState::setWorldSimulationStep(_numSubsteps);

    // actions (grabs, tractors, offsets) push on their bodies, wherever those are
    for (auto& dynamic : _objectDynamics) {
        if (dynamic && dynamic->isAction()) {
            static_cast<ObjectAction*>(dynamic.get())->updateAction(dt);
        }
    }

    PhysicsWorld* avatarWorld = characterWorld();
    _rootWorld->stepOnce(dt, avatarWorld == _rootWorld.get() ? _myAvatarController : nullptr);

    // each frame's world steps in the frame's coordinates, knowing only where the frame is for gravity's sake
    feedFrameTransforms();
    for (auto& entry : _frameWorlds) {
        entry.second->stepOnce(dt, avatarWorld == entry.second.get() ? _myAvatarController : nullptr);
    }

    doOwnershipInfectionForConstraints();
}

void PhysicsEngine::stepSimulation() {
    PROFILE_RANGE(simulation_physics, "stepSimulation");
    // NOTE: the grand order of operations is:
    // (1) pull incoming changes
    // (2) step simulation
    // (3) synchronize outgoing motion states
    // (4) send outgoing packets

    const float MAX_TIMESTEP = (float)PHYSICS_ENGINE_MAX_NUM_SUBSTEPS * PHYSICS_ENGINE_FIXED_SUBSTEP;
    uint64_t now = usecTimestampNow();
    float dt = glm::min(1.0e-6f * (float)(now - _lastStepTime), MAX_TIMESTEP);
    _lastStepTime = now;

    // The simulation runs on a paced clock, not the wall clock: frames are presented evenly, but this update
    // runs early or late within them by several milliseconds, and with the results read ahead to the present
    // (below) that wobble would show as uneven motion. The paced clock advances by the smoothed frame
    // duration and is eased back to the wall clock, within a frame of it; a hitch snaps it back.
    const float FRAME_SMOOTHING = 0.05f;
    const float CLOCK_EASING = 0.05f;
    if (_smoothedFrameDuration <= 0.0f) {
        _smoothedFrameDuration = dt;
    } else {
        _smoothedFrameDuration += FRAME_SMOOTHING * (dt - _smoothedFrameDuration);
    }
    float paced = _smoothedFrameDuration;
    _pacedClockLead += paced - dt;
    if (fabsf(_pacedClockLead) > _smoothedFrameDuration) {
        paced -= _pacedClockLead;
        _pacedClockLead = 0.0f;
    } else {
        float correction = CLOCK_EASING * _pacedClockLead;
        paced -= correction;
        _pacedClockLead -= correction;
    }
    _accumulatedTime = glm::max(_accumulatedTime + paced, 0.0f);

    int numSubsteps = (int)(_accumulatedTime / PHYSICS_ENGINE_FIXED_SUBSTEP);
    if (numSubsteps > PHYSICS_ENGINE_MAX_NUM_SUBSTEPS) {
        // the simulation can't keep up; drop time rather than spiral
        numSubsteps = PHYSICS_ENGINE_MAX_NUM_SUBSTEPS;
        _accumulatedTime = 0.0f;
    } else {
        _accumulatedTime -= (float)numSubsteps * PHYSICS_ENGINE_FIXED_SUBSTEP;
    }

    uint64_t start = usecTimestampNow();
    for (int i = 0; i < numSubsteps; ++i) {
        stepOnce(PHYSICS_ENGINE_FIXED_SUBSTEP);
    }
    _lastStepDuration = 1.0e-3f * (float)(usecTimestampNow() - start);

    // The results are read once per rendered frame, this much past the last substep, and every object reads
    // itself that far ahead along its velocity (ObjectMotionState::pullTransformFromBody); so a frame with no
    // substep of its own (a display faster than the simulation) still reads fresh positions.
    ObjectMotionState::setLeftoverTime(_accumulatedTime);
    _hasOutgoingChanges = true;

    if (numSubsteps > 0) {
        if (_debugDrawMode != PhysicsDebugDraw::DRAW_NOTHING) {
            PROFILE_RANGE(simulation_physics, "debugDrawWorld");
            _rootWorld->debugDraw(_debugDrawMode, _physicsDebugDraw.get());
            for (auto& entry : _frameWorlds) {
                entry.second->debugDraw(_debugDrawMode, _physicsDebugDraw.get());
            }
        }
    }
}

void PhysicsEngine::harvestPerformanceStats() {
    PerformanceTimer::addTimerRecord("physics/...step", (uint64_t)(_lastStepDuration * 1000.0f));
}

void PhysicsEngine::printPerformanceStatsToFile(const QString& filename) {
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        qCDebug(physics) << "unable to open file " << file.fileName() << " to save stepSimulation() stats";
        return;
    }
    QTextStream s(&file);
    s << "stepSimulation = " << _lastStepDuration << "\n";
    s << "worlds = " << (1 + (int)_frameWorlds.size()) << "\n";
    s << "bodies = " << getNumCollisionObjects() << "\n";
    s << "activeBodies = " << (_rootWorld ? _rootWorld->getNumActiveBodies() : 0) << "\n";
    s << "contacts = " << (int)(_rootWorld ? _rootWorld->getNumContacts() : 0) << "\n";
    file.close();
}

void PhysicsEngine::updateContactMap() {
    // contacts arrive through each world's step; nothing more to do
}

void PhysicsEngine::doOwnershipInfectionForConstraints() {
    JPH::BodyID characterBodyID = _myAvatarController ? _myAvatarController->getInnerBodyID() : JPH::BodyID();
    PhysicsWorld* avatarWorld = characterWorld();
    // the constraints' bodies, grouped by the world they live in
    std::map<PhysicsWorld*, std::vector<std::vector<JPH::BodyID>>> byWorld;
    foreach(const auto& dynamic, _objectDynamics) {
        if (!dynamic || !dynamic->isConstraint()) {
            continue;
        }
        PhysicsWorld* world = _dynamicWorlds.value(dynamic->getID(), nullptr);
        if (world) {
            byWorld[world].push_back(std::static_pointer_cast<ObjectDynamic>(dynamic)->getBodyIDs());
        }
    }
    for (auto& entry : byWorld) {
        entry.first->doOwnershipInfectionForConstraints(entry.second, entry.first == avatarWorld ? characterBodyID : JPH::BodyID());
    }
}

const CollisionEvents& PhysicsEngine::getCollisionEvents() {
    _collisionEvents.clear();
    _rootWorld->collectCollisionEvents(_originOffset, _collisionEvents);
    for (auto& entry : _frameWorlds) {
        entry.second->collectCollisionEvents(_originOffset, _collisionEvents);
    }
    return _collisionEvents;
}

const VectorOfMotionStates& PhysicsEngine::getChangedMotionStates() {
    PROFILE_RANGE(simulation_physics, "copyOutgoingChanges");
    _changedMotionStates.clear();
    _deactivatedMotionStates.clear();
    _rootWorld->harvestChanges(_changedMotionStates, _deactivatedMotionStates);
    for (auto& entry : _frameWorlds) {
        entry.second->harvestChanges(_changedMotionStates, _deactivatedMotionStates);
    }
    _hasOutgoingChanges = false;
    return _changedMotionStates;
}

void PhysicsEngine::dumpStatsIfNecessary() {
    if (_dumpNextStats) {
        _dumpNextStats = false;
        if (_saveNextStats) {
            _saveNextStats = false;
            printPerformanceStatsToFile(_statsFilename);
        }
        qCDebug(physics) << "PhysicsEngine: last step" << _lastStepDuration << "ms," << getNumCollisionObjects() << "bodies in"
                         << (1 + (int)_frameWorlds.size()) << "worlds," << (_rootWorld ? _rootWorld->getNumActiveBodies() : 0) << "active in the root";
    }
}

void PhysicsEngine::saveNextPhysicsStats(QString filename) {
    _saveNextStats = true;
    _dumpNextStats = true;
    _statsFilename = filename;
}

void PhysicsEngine::setCharacterController(CharacterController* character) {
    _myAvatarController = character;
}

EntityDynamicPointer PhysicsEngine::getDynamicByID(const QUuid& dynamicID) const {
    if (_objectDynamics.contains(dynamicID)) {
        return _objectDynamics[dynamicID];
    }
    return nullptr;
}

bool PhysicsEngine::addDynamic(EntityDynamicPointer dynamic) {
    assert(dynamic);

    if (!dynamic->isReadyForAdd()) {
        return false;
    }

    const QUuid& dynamicID = dynamic->getID();
    if (_objectDynamics.contains(dynamicID)) {
        if (_objectDynamics[dynamicID] == dynamic) {
            return true;
        }
        removeDynamic(dynamic->getID());
    }

    bool success { false };
    PhysicsWorld* world = nullptr;
    if (dynamic->isAction()) {
        success = true;
    } else if (dynamic->isConstraint()) {
        ObjectConstraint* objectConstraint = static_cast<ObjectConstraint*>(dynamic.get());
        JPH::Constraint* constraint = objectConstraint->getConstraint();
        if (constraint) {
            // a constraint lives in the world of its owner's body; both bodies must be there
            ObjectMotionState* owner = ownerMotionState(objectConstraint);
            world = owner ? owner->getWorld() : nullptr;
            if (world) {
                world->getPhysicsSystem()->AddConstraint(constraint);
                success = true;
            }
        } // else perhaps not all the rigid bodies are available, yet
    }

    if (success) {
        _objectDynamics[dynamicID] = dynamic;
        if (world) {
            _dynamicWorlds[dynamicID] = world;
        }
        ObjectMotionState* owner = ownerMotionState(dynamic.get());
        PhysicsWorld* ownerWorld = owner ? owner->getWorld() : nullptr;
        if (ownerWorld) {
            for (const JPH::BodyID& bodyID : std::static_pointer_cast<ObjectDynamic>(dynamic)->getBodyIDs()) {
                _objectDynamicsByBody[std::make_pair(ownerWorld, bodyID.GetIndexAndSequenceNumber())] += dynamic->getID();
            }
        }
    }
    return success;
}

void PhysicsEngine::removeDynamic(const QUuid dynamicID) {
    // (one waiting to go back on a rebuilt body is removed for good)
    _dynamicsToRestore.remove(dynamicID);
    if (_objectDynamics.contains(dynamicID)) {
        ObjectDynamicPointer dynamic = std::static_pointer_cast<ObjectDynamic>(_objectDynamics[dynamicID]);
        if (!dynamic) {
            return;
        }
        std::vector<JPH::BodyID> bodies = dynamic->getBodyIDs();
        if (dynamic->isConstraint()) {
            ObjectConstraint* objectConstraint = static_cast<ObjectConstraint*>(dynamic.get());
            JPH::Constraint* constraint = objectConstraint->getConstraint();
            PhysicsWorld* world = _dynamicWorlds.value(dynamicID, nullptr);
            if (constraint && world) {
                world->getPhysicsSystem()->RemoveConstraint(constraint);
            } else {
                qCDebug(physics) << "PhysicsEngine::removeDynamic of constraint failed";
            }
        }
        _objectDynamics.remove(dynamicID);
        _dynamicWorlds.remove(dynamicID);
        for (auto itr = _objectDynamicsByBody.begin(); itr != _objectDynamicsByBody.end();) {
            itr->second.remove(dynamicID);
            itr = itr->second.isEmpty() ? _objectDynamicsByBody.erase(itr) : std::next(itr);
        }
        dynamic->invalidate();
    }
}

void PhysicsEngine::forEachDynamic(std::function<void(EntityDynamicPointer)> actor) {
    QMutableHashIterator<QUuid, EntityDynamicPointer> iter(_objectDynamics);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value()) {
            actor(iter.value());
        }
    }
}

static void setDebugBit(uint32_t& mode, uint32_t bit, bool value) {
    if (value) {
        mode |= bit;
    } else {
        mode &= ~bit;
    }
}

void PhysicsEngine::setShowBulletWireframe(bool value) {
    setDebugBit(_debugDrawMode, PhysicsDebugDraw::DRAW_WIREFRAME, value);
}

void PhysicsEngine::setShowBulletAABBs(bool value) {
    setDebugBit(_debugDrawMode, PhysicsDebugDraw::DRAW_AABBS, value);
}

void PhysicsEngine::setShowBulletContactPoints(bool value) {
    setDebugBit(_debugDrawMode, PhysicsDebugDraw::DRAW_CONTACT_POINTS, value);
}

void PhysicsEngine::setShowBulletConstraints(bool value) {
    setDebugBit(_debugDrawMode, PhysicsDebugDraw::DRAW_CONSTRAINTS, value);
}

void PhysicsEngine::setShowBulletConstraintLimits(bool value) {
    setDebugBit(_debugDrawMode, PhysicsDebugDraw::DRAW_CONSTRAINT_LIMITS, value);
}

std::vector<ContactTestResult> PhysicsEngine::contactTest(uint16_t mask, const ShapeInfo& regionShapeInfo, const Transform& regionTransform, uint16_t group, float threshold) const {
    if (!_rootWorld) {
        return std::vector<ContactTestResult>();
    }
    JPH::BodyID characterBodyID = ((mask & USER_COLLISION_GROUP_MY_AVATAR) && _myAvatarController && characterWorld() == _rootWorld.get()) ?
        _myAvatarController->getInnerBodyID() : JPH::BodyID();
    return _rootWorld->contactTest(mask, regionShapeInfo, regionTransform, group, threshold, characterBodyID);
}

bool PhysicsEngine::castRayClosest(const glm::vec3& start, const glm::vec3& end, int32_t group, int32_t mask,
                                   const JPH::BodyFilter* bodyFilter, RayHit& result) const {
    return _rootWorld ? _rootWorld->castRayClosest(start, end, group, mask, bodyFilter, result) : false;
}

void PhysicsEngine::castRayAll(const glm::vec3& start, const glm::vec3& end, int32_t group, int32_t mask,
                               std::vector<RayHit>& results) const {
    results.clear();
    if (_rootWorld) {
        _rootWorld->castRayAll(start, end, group, mask, results);
    }
    // the ray goes into each frame's world in that frame's coordinates, and the hits come back out
    for (const auto& entry : _frameWorlds) {
        const PhysicsWorld* world = entry.second.get();
        std::vector<RayHit> hits;
        world->castRayAll(world->toLocal(start), world->toLocal(end), group, mask, hits);
        for (RayHit& hit : hits) {
            hit.point = world->toParent(hit.point);
            hit.normal = world->getFrameRotation() * hit.normal;
            results.push_back(hit);
        }
    }
    std::sort(results.begin(), results.end(), [](const RayHit& a, const RayHit& b) { return a.fraction < b.fraction; });
}
