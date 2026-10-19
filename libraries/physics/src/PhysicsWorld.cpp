//
//  PhysicsWorld.cpp
//  libraries/physics/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PhysicsWorld.h"

#include <algorithm>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <atomic>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <PhysicsCollisionGroups.h>
#include <Profile.h>
#include <SharedUtil.h>

#include "CharacterController.h"
#include "JoltUtil.h"
#include "PhysicsDebugDraw.h"
#include "PhysicsHelpers.h"
#include "PhysicsLogging.h"

namespace {
    // Jolt's limits for one world. Bodies beyond this fail to add, so it is generous.
    const JPH::uint MAX_BODIES = 65536;
    const JPH::uint NUM_BODY_MUTEXES = 0; // 0 = Jolt's default
    const JPH::uint MAX_BODY_PAIRS = 65536;
    const JPH::uint MAX_CONTACT_CONSTRAINTS = 16384;

    // a body sleeps after this long below the velocity threshold (Bullet's hard-coded value was 2 s)
    const float TIME_BEFORE_SLEEP = 1.0f;
}

// Jolt reports contacts from its worker threads while a step runs; this gathers them under a lock so the
// world can fold them into its contact map afterwards, on the simulation thread.
class PhysicsWorld::ContactRecorder : public JPH::ContactListener {
public:
    struct Record {
        ObjectMotionState* a;
        ObjectMotionState* b;
        bool aIsStatic, bIsStatic, aIsKinematic, bIsKinematic;
        glm::vec3 positionOnB;
        glm::vec3 normalOnB; // points from B toward A
        float distance;      // negative when penetrating
    };

    void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override {
        record(body1, body2, manifold);
    }

    void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override {
        record(body1, body2, manifold);
    }

    void take(std::vector<Record>& out) {
        std::lock_guard<std::mutex> lock(_mutex);
        out.swap(_records);
        _records.clear();
    }
    // a replayed substep's contacts are not events (they happened already, or will again)
    void setMuted(bool muted) { _muted = muted; }

private:
    void record(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold) {
        if (_muted) {
            return;
        }
        ObjectMotionState* a = PhysicsWorld::motionStateOf(body1);
        ObjectMotionState* b = PhysicsWorld::motionStateOf(body2);
        if (!a && !b) {
            return;
        }
        // a contact with a frame's copy of its interior (round 8) is a contact with that part
        if (a) {
            if (ObjectMotionState* part = a->interiorPartAt(manifold.mSubShapeID1)) {
                a = part;
            }
        }
        if (b) {
            if (ObjectMotionState* part = b->interiorPartAt(manifold.mSubShapeID2)) {
                b = part;
            }
        }
        // A body without a motion state is MyAvatar's inner body; it always takes the A side of a contact.
        bool swap = (b == nullptr);
        const JPH::Body& bodyA = swap ? body2 : body1;
        const JPH::Body& bodyB = swap ? body1 : body2;
        Record record;
        record.a = swap ? b : a;
        record.b = swap ? a : b;
        record.aIsStatic = bodyA.IsStatic();
        record.bIsStatic = bodyB.IsStatic();
        record.aIsKinematic = bodyA.IsKinematic();
        record.bIsKinematic = bodyB.IsKinematic();
        // Jolt's normal points from body 1 to body 2; the engine's contact normal points from B toward A.
        glm::vec3 normalFrom1To2 = joltToGLM(manifold.mWorldSpaceNormal);
        record.normalOnB = swap ? normalFrom1To2 : -normalFrom1To2;
        record.distance = -manifold.mPenetrationDepth;
        if (swap) {
            record.positionOnB = manifold.mRelativeContactPointsOn1.empty() ? joltToGLM(manifold.mBaseOffset)
                                                                            : joltToGLM(manifold.GetWorldSpaceContactPointOn1(0));
        } else {
            record.positionOnB = manifold.mRelativeContactPointsOn2.empty() ? joltToGLM(manifold.mBaseOffset)
                                                                            : joltToGLM(manifold.GetWorldSpaceContactPointOn2(0));
        }
        std::lock_guard<std::mutex> lock(_mutex);
        _records.push_back(record);
    }

    std::mutex _mutex;
    std::vector<Record> _records;
    std::atomic<bool> _muted { false };
};

// Jolt tells us when bodies fall asleep, also from its worker threads.
class PhysicsWorld::ActivationRecorder : public JPH::BodyActivationListener {
public:
    void OnBodyActivated(const JPH::BodyID& bodyID, JPH::uint64 userData) override {
    }

    void OnBodyDeactivated(const JPH::BodyID& bodyID, JPH::uint64 userData) override {
        std::lock_guard<std::mutex> lock(_mutex);
        _deactivated.push_back((ObjectMotionState*)(uintptr_t)userData);
    }

    void takeDeactivated(std::vector<ObjectMotionState*>& out) {
        std::lock_guard<std::mutex> lock(_mutex);
        out.swap(_deactivated);
        _deactivated.clear();
    }

private:
    std::mutex _mutex;
    std::vector<ObjectMotionState*> _deactivated;
};

// static
ObjectMotionState* PhysicsWorld::motionStateOf(const JPH::Body& body) {
    return (ObjectMotionState*)(uintptr_t)body.GetUserData();
}

PhysicsWorld::PhysicsWorld(const QUuid& frameID, JPH::TempAllocator* tempAllocator, JPH::JobSystem* jobSystem) :
        _frameID(frameID), _tempAllocator(tempAllocator), _jobSystem(jobSystem) {
    _layers.reset(new JoltLayers::Tables());
    _system.reset(new JPH::PhysicsSystem());
    _system->Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS,
                  _layers->broadPhaseLayers, _layers->objectVsBroadPhase, _layers->objectLayerPairs);

    // the world has no gravity of its own; each object carries its own, applied as a force (and inside a
    // frame, turned into the frame's coordinates)
    _system->SetGravity(JPH::Vec3::sZero());

    JPH::PhysicsSettings settings = _system->GetPhysicsSettings();
    settings.mTimeBeforeSleep = TIME_BEFORE_SLEEP;
    settings.mDeterministicSimulation = true;
    _system->SetPhysicsSettings(settings);

    _contactRecorder.reset(new ContactRecorder());
    _system->SetContactListener(_contactRecorder.get());
    _activationRecorder.reset(new ActivationRecorder());
    _system->SetBodyActivationListener(_activationRecorder.get());
}

PhysicsWorld::~PhysicsWorld() {
    setFrameHull(nullptr);
    removeAllObjects();
    _system.reset();
    _contactRecorder.reset();
    _activationRecorder.reset();
    _layers.reset();
}

void PhysicsWorld::setFrameTransform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& linearVelocity, const glm::vec3& angularVelocity) {
    _framePosition = position;
    _frameRotation = rotation;
    _frameLinearVelocity = linearVelocity;
    _frameAngularVelocity = angularVelocity;
}

JPH::BodyInterface& PhysicsWorld::getBodyInterface() const {
    return _system->GetBodyInterfaceNoLock();
}

int32_t PhysicsWorld::getNumBodies() const {
    return _system ? (int32_t)_system->GetNumBodies() : 0;
}

uint32_t PhysicsWorld::getNumActiveBodies() const {
    return _system ? _system->GetNumActiveBodies(JPH::EBodyType::RigidBody) : 0;
}

uint32_t PhysicsWorld::getNumActiveDynamicBodies(QStringList* names, int maxNames) const {
    if (!_system || _system->GetNumActiveBodies(JPH::EBodyType::RigidBody) == 0) {
        return 0;
    }
    JPH::BodyIDVector ids;
    _system->GetActiveBodies(JPH::EBodyType::RigidBody, ids);
    const JPH::BodyLockInterfaceNoLock& lockInterface = _system->GetBodyLockInterfaceNoLock();
    uint32_t count = 0;
    for (const JPH::BodyID& id : ids) {
        JPH::BodyLockRead lock(lockInterface, id);
        if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) {
            continue;
        }
        ++count;
        if (names && (int)names->size() < maxNames) {
            ObjectMotionState* state = motionStateOf(lock.GetBody());
            names->append(state ? state->getName() : QString("(no motion state)"));
        }
    }
    return count;
}

// private
void PhysicsWorld::createBody(ObjectMotionState* motionState) {
    assert(motionState);
    const JPH::Shape* shape = motionState->getBodyShape();
    assert(shape);
    if (!shape) {
        return;
    }

    PhysicsMotionType motionType = motionState->computePhysicsMotionType();
    motionState->setMotionType(motionType);

    int32_t group, mask;
    motionState->computeCollisionGroupAndMask(group, mask);

    JPH::BodyCreationSettings settings(shape, glmToJoltR(motionState->getObjectPosition()), glmToJolt(motionState->getObjectRotation()),
                                       JPH::EMotionType::Static, JoltLayers::layerFor(group, mask));
    settings.mUserData = (JPH::uint64)(uintptr_t)motionState;
    settings.mFriction = motionState->getObjectFriction();
    settings.mRestitution = motionState->getObjectRestitution();
    settings.mAllowedDOFs = motionState->getAllowedDOFs();
    settings.mAllowSleeping = true;

    bool activate = false;
    switch(motionType) {
        case MOTION_TYPE_KINEMATIC: {
            settings.mMotionType = JPH::EMotionType::Kinematic;
            activate = true;
            break;
        }
        case MOTION_TYPE_DYNAMIC: {
            settings.mMotionType = JPH::EMotionType::Dynamic;
            float mass = motionState->getMass();
            const float MIN_DYNAMIC_MASS = 0.01f;
            if (mass != mass || mass < MIN_DYNAMIC_MASS) {
                mass = MIN_DYNAMIC_MASS;
            }
            // the mass and inertia of the object's own shape (a frame's interior copies add collision, not mass)
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride = motionState->getOwnMassProperties();
            settings.mMassPropertiesOverride.ScaleToMass(mass);
            settings.mMassPropertiesOverride.mInertia(3, 3) = 1.0f;
            // an object that is not moving starts asleep, so a world full of resting things costs nothing; but one with
            // gravity must start awake, because gravity here is a force applied to active bodies (applyGravityForces)
            // and Jolt ignores forces on a sleeping body: a ball placed in the air would hang there. Once it lands and
            // settles, Jolt puts it to sleep as usual.
            activate = motionState->isMoving() || glm::length2(motionState->getObjectGravity()) > 0.0f;
            break;
        }
        case MOTION_TYPE_STATIC:
        default: {
            settings.mMotionType = JPH::EMotionType::Static;
            break;
        }
    }

    JPH::BodyInterface& bodies = getBodyInterface();
    JPH::Body* body = bodies.CreateBody(settings);
    if (!body) {
        qCWarning(physics) << "PhysicsWorld: out of bodies, could not add" << motionState->getName();
        return;
    }
    bodies.AddBody(body->GetID(), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    motionState->setBody(body, this);
    _motionStates.insert(motionState);

    if (motionType == MOTION_TYPE_KINEMATIC) {
        motionState->updateBodyVelocities();
        motionState->updateLastKinematicStep();
        motionState->clearInternalKinematicChanges();
        _kinematicMotionStates.insert(motionState);
    } else if (motionType == MOTION_TYPE_DYNAMIC) {
        motionState->updateBodyVelocities();
        if (!activate) {
            bodies.DeactivateBody(body->GetID());
        }
    } else {
        // a new static body does not wake the sleeping things it may now be under or inside; we do
        wakeBodiesAround(body);
        if (motionState->isLocallyOwned()) {
            _activeStaticObjects.insert(motionState);
        }
        _interiorChanged = true;
    }
    motionState->updateBodyMaterialProperties();
}

// private
void PhysicsWorld::destroyBody(ObjectMotionState* motionState) {
    JPH::Body* body = motionState->getBody();
    if (!body) {
        return;
    }
    JPH::BodyID bodyID = body->GetID();
    if (body->IsStatic()) {
        _interiorChanged = true;
    }
    JPH::BodyInterface& bodies = getBodyInterface();
    bodies.RemoveBody(bodyID);
    motionState->setBody(nullptr, nullptr);
    bodies.DestroyBody(bodyID);
    _motionStates.erase(motionState);
    _kinematicMotionStates.erase(motionState);
    _activeStaticObjects.erase(motionState);
}

void PhysicsWorld::addObject(ObjectMotionState* motionState) {
    if (motionState->getBody()) {
        // the body is rebuilt from scratch so that its type, shape and layer are all fresh
        wakeBodiesTouching(motionState);
        removeContacts(motionState);
        destroyBody(motionState);
    }
    createBody(motionState);
}

void PhysicsWorld::removeObject(ObjectMotionState* motionState) {
    wakeBodiesTouching(motionState);
    removeContacts(motionState);
    destroyBody(motionState);
}

void PhysicsWorld::reinsertObject(ObjectMotionState* motionState) {
    if (motionState->getBody()) {
        addObject(motionState);
    }
}

void PhysicsWorld::markStaticObjectActive(ObjectMotionState* motionState) {
    if (motionState->getBody()) {
        wakeBodiesAround(motionState->getBody());
        _activeStaticObjects.insert(motionState);
    }
}

void PhysicsWorld::removeAllObjects() {
    _contactMap.clear();
    std::vector<ObjectMotionState*> all(_motionStates.begin(), _motionStates.end());
    for (ObjectMotionState* motionState : all) {
        destroyBody(motionState);
        motionState->clearIncomingDirtyFlags();
    }
    _activeStaticObjects.clear();
}

void PhysicsWorld::setFrameHull(ObjectMotionState* frameState) {
    const JPH::Shape* shape = frameState ? frameState->getShape() : nullptr;
    if (!_frameHullID.IsInvalid()) {
        if (shape && shape == _frameHullShape) {
            // same shape: keep the body, refresh who it belongs to and its material
            getBodyInterface().SetUserData(_frameHullID, (JPH::uint64)(uintptr_t)frameState);
            getBodyInterface().SetFriction(_frameHullID, frameState->getObjectFriction());
            getBodyInterface().SetRestitution(_frameHullID, frameState->getObjectRestitution());
            return;
        }
        JPH::BodyInterface& bodies = getBodyInterface();
        bodies.RemoveBody(_frameHullID);
        bodies.DestroyBody(_frameHullID);
        _frameHullID = JPH::BodyID();
        _frameHullShape = nullptr;
    }
    if (!shape) {
        return;
    }
    int32_t group, mask;
    frameState->computeCollisionGroupAndMask(group, mask);
    JPH::BodyCreationSettings settings(shape, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                       JoltLayers::layerFor(group, mask));
    settings.mUserData = (JPH::uint64)(uintptr_t)frameState;
    settings.mFriction = frameState->getObjectFriction();
    settings.mRestitution = frameState->getObjectRestitution();
    JPH::BodyInterface& bodies = getBodyInterface();
    JPH::Body* body = bodies.CreateBody(settings);
    if (!body) {
        qCWarning(physics) << "PhysicsWorld: out of bodies, no hull for frame" << _frameID;
        return;
    }
    bodies.AddBody(body->GetID(), JPH::EActivation::DontActivate);
    _frameHullID = body->GetID();
    _frameHullShape = shape;
    wakeBodiesAround(body);
}

void PhysicsWorld::removeContacts(ObjectMotionState* motionState) {
    ContactMap::iterator contactItr = _contactMap.begin();
    while (contactItr != _contactMap.end()) {
        if (contactItr->first._a == motionState || contactItr->first._b == motionState) {
            contactItr = _contactMap.erase(contactItr);
        } else {
            ++contactItr;
        }
    }
}

// Find all objects that touch the object corresponding to motionState and flag the other objects
// for simulation ownership by the local simulation, and wake them: their support may be going away.
void PhysicsWorld::wakeBodiesTouching(ObjectMotionState* motionState) {
    assert(motionState);
    for (auto& contact : _contactMap) {
        ObjectMotionState* other = nullptr;
        if (contact.first._a == motionState) {
            other = static_cast<ObjectMotionState*>(contact.first._b);
        } else if (contact.first._b == motionState) {
            other = static_cast<ObjectMotionState*>(contact.first._a);
        }
        if (other && other->getBody() && other->getBody()->IsDynamic()) {
            other->bump(VOLUNTEER_SIMULATION_PRIORITY);
            other->activateBody();
        }
    }
}

// private
void PhysicsWorld::wakeBodiesAround(const JPH::Body* body) {
    if (!body) {
        return;
    }
    const float MARGIN = 0.1f; // meters: a resting body may sit just outside the bounds
    JPH::AABox bounds = body->GetWorldSpaceBounds();
    bounds.ExpandBy(JPH::Vec3::sReplicate(MARGIN));
    getBodyInterface().ActivateBodiesInAABox(bounds, JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter());
}

// private
void PhysicsWorld::rememberSubstepStarts() {
    JPH::BodyIDVector activeBodies;
    _system->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
    const JPH::BodyLockInterfaceNoLock& lockInterface = _system->GetBodyLockInterfaceNoLock();
    for (const JPH::BodyID& bodyID : activeBodies) {
        JPH::BodyLockRead lock(lockInterface, bodyID);
        if (!lock.Succeeded()) {
            continue;
        }
        ObjectMotionState* motionState = motionStateOf(lock.GetBody());
        if (motionState) {
            motionState->rememberSubstepStart();
        }
    }
}

// private
void PhysicsWorld::applyGravityForces() {
    // Every object carries its own gravity, in world coordinates, applied as a force to the active dynamic
    // bodies. Inside a frame it is turned into the frame's coordinates, so "down" stays down when the frame tilts.
    glm::quat toLocal = glm::inverse(_frameRotation);
    JPH::BodyIDVector activeBodies;
    _system->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
    const JPH::BodyLockInterfaceNoLock& lockInterface = _system->GetBodyLockInterfaceNoLock();
    for (const JPH::BodyID& bodyID : activeBodies) {
        JPH::BodyLockWrite lock(lockInterface, bodyID);
        if (!lock.Succeeded()) {
            continue;
        }
        JPH::Body& body = lock.GetBody();
        if (!body.IsDynamic()) {
            continue;
        }
        ObjectMotionState* motionState = motionStateOf(body);
        if (!motionState) {
            continue;
        }
        const glm::vec3& gravity = motionState->getBodyGravity();
        if (glm::length2(gravity) > 0.0f) {
            float mass = 1.0f / glm::max(body.GetMotionProperties()->GetInverseMass(), 1.0e-6f);
            body.AddForce(glmToJolt((toLocal * gravity) * mass));
        }
    }
}

void PhysicsWorld::stepOnce(float dt, CharacterController* character) {
    // keyframed objects move toward where their owners say they are, parents before their children
    std::vector<ObjectMotionState*> kinematics(_kinematicMotionStates.begin(), _kinematicMotionStates.end());
    std::stable_sort(kinematics.begin(), kinematics.end(), [](ObjectMotionState* a, ObjectMotionState* b) {
        return a->getNestingDepth() < b->getNestingDepth();
    });
    for (ObjectMotionState* motionState : kinematics) {
        motionState->pushTransformToBody(dt);
    }

    // MyAvatar's character moves itself through this world, before the world moves
    if (character) {
        character->substep(dt);
    }

    // every awake body's pose before the substep, for the presentation (ObjectMotionState::getPresentedBodyTransform)
    rememberSubstepStarts();
    applyGravityForces();

    JPH::EPhysicsUpdateError error = _system->Update(dt, 1, _tempAllocator, _jobSystem);
    if (error != JPH::EPhysicsUpdateError::None) {
        qCWarning(physics) << "PhysicsWorld: Jolt update error" << (int)error;
    }

    harvestContacts();
}

namespace {
    // the non-static bodies, less the one the authority's state replaces
    class RollbackFilter : public JPH::StateRecorderFilter {
    public:
        explicit RollbackFilter(const JPH::BodyID& exclude) : _exclude(exclude) {}
        bool ShouldSaveBody(const JPH::Body& body) const override { return !body.IsStatic() && body.GetID() != _exclude; }
        bool ShouldSaveConstraint(const JPH::Constraint&) const override { return false; }
        bool ShouldSaveContact(const JPH::BodyID&, const JPH::BodyID&) const override { return false; }
        bool ShouldRestoreContact(const JPH::BodyID&, const JPH::BodyID&) const override { return false; }
    private:
        JPH::BodyID _exclude;
    };
}

std::string PhysicsWorld::saveBodies(const JPH::BodyID& exclude) const {
    JPH::StateRecorderImpl recorder;
    RollbackFilter filter(exclude);
    _system->SaveState(recorder, JPH::EStateRecorderState::Bodies, &filter);
    return recorder.GetData();
}

bool PhysicsWorld::restoreBodies(const std::string& saved, const JPH::BodyID& exclude) {
    JPH::StateRecorderImpl recorder;
    recorder.WriteBytes(saved.data(), saved.size());
    recorder.Rewind();
    RollbackFilter filter(exclude);
    return _system->RestoreState(recorder, &filter);
}

void PhysicsWorld::replayStep(float dt) {
    applyGravityForces();
    _replaying = true;
    _contactRecorder->setMuted(true);
    JPH::EPhysicsUpdateError error = _system->Update(dt, 1, _tempAllocator, _jobSystem);
    _contactRecorder->setMuted(false);
    _replaying = false;
    if (error != JPH::EPhysicsUpdateError::None) {
        qCWarning(physics) << "PhysicsWorld: Jolt replay error" << (int)error;
    }
    std::vector<ContactRecorder::Record> dropped;
    _contactRecorder->take(dropped);   // anything recorded between mute and unmute
}

// private
void PhysicsWorld::doOwnershipInfection(ObjectMotionState* motionStateA, bool aIsCharacter, bool aIsStatic,
                                        ObjectMotionState* motionStateB, bool bIsStatic, bool bIsKinematic) {
    // "A owns itself" means: A is MyAvatar's character, or A is simulated by us and is not static.
    bool aIsOurs = aIsCharacter || (motionStateA && motionStateA->getSimulatorID() == Physics::getSessionUUID() && !aIsStatic);
    if (!aIsOurs || !motionStateB) {
        return;
    }
    // NOTE: we might own the simulation of a kinematic object (A)
    // but we don't claim ownership of kinematic objects (B) based on collisions here.
    if (!bIsStatic && !bIsKinematic && motionStateB->getSimulatorID() != Physics::getSessionUUID()) {
        uint8_t priorityA = motionStateA ? motionStateA->getSimulationPriority() : PERSONAL_SIMULATION_PRIORITY;
        motionStateB->bump(priorityA);
    }
}

// private
void PhysicsWorld::harvestContacts() {
    std::vector<ContactRecorder::Record> records;
    _contactRecorder->take(records);
    ++_numContactFrames;
    for (const auto& record : records) {
        // the manifold has up to 4 distinct points, but only extract info from the first
        _contactMap[ContactKey(record.a, record.b)].update(_numContactFrames, record.positionOnB, record.normalOnB, record.distance);

        if (!Physics::getSessionUUID().isNull()) {
            doOwnershipInfection(record.a, record.a == nullptr, record.aIsStatic, record.b, record.bIsStatic, record.bIsKinematic);
            if (record.a) {
                doOwnershipInfection(record.b, false, record.bIsStatic, record.a, record.aIsStatic, record.aIsKinematic);
            }
        }
    }
}

void PhysicsWorld::recordCharacterContact(ObjectMotionState* other, const glm::vec3& positionOnOther, const glm::vec3& normalTowardCharacter, float distance) {
    if (!other) {
        return;
    }
    _contactMap[ContactKey(nullptr, other)].update(_numContactFrames, positionOnOther, normalTowardCharacter, distance);
    if (!Physics::getSessionUUID().isNull()) {
        const JPH::Body* body = other->getBody();
        bool isStatic = body ? body->IsStatic() : true;
        bool isKinematic = body ? body->IsKinematic() : false;
        doOwnershipInfection(nullptr, true, false, other, isStatic, isKinematic);
    }
}

void PhysicsWorld::doOwnershipInfectionForConstraints(const std::vector<std::vector<JPH::BodyID>>& constraintBodies, const JPH::BodyID& characterBodyID) {
    for (const std::vector<JPH::BodyID>& bodies : constraintBodies) {
        if (bodies.size() <= 1) {
            continue;
        }
        int32_t numOwned = 0;
        int32_t numStatic = 0;
        uint8_t priority = VOLUNTEER_SIMULATION_PRIORITY;
        std::vector<ObjectMotionState*> motionStates;
        for (const JPH::BodyID& bodyID : bodies) {
            JPH::BodyLockRead lock(_system->GetBodyLockInterfaceNoLock(), bodyID);
            if (!lock.Succeeded()) {
                continue;
            }
            const JPH::Body& body = lock.GetBody();
            ObjectMotionState* motionState = motionStateOf(body);
            if (body.IsStatic()) {
                ++numStatic;
            } else if (motionState && motionState->getType() == MOTIONSTATE_TYPE_AVATAR) {
                // we can never take ownership of this constraint
                numOwned = 0;
                break;
            } else {
                if (motionState && motionState->getSimulatorID() == Physics::getSessionUUID()) {
                    priority = glm::max(priority, motionState->getSimulationPriority());
                } else if (bodyID == characterBodyID) {
                    priority = glm::max(priority, PERSONAL_SIMULATION_PRIORITY);
                }
                numOwned++;
            }
            if (motionState) {
                motionStates.push_back(motionState);
            }
        }

        if (numOwned > 0 && numOwned + numStatic != (int32_t)bodies.size()) {
            // we have partial ownership but it isn't complete so we walk each object
            // and bump the simulation priority to the highest priority we encountered earlier
            for (ObjectMotionState* motionState : motionStates) {
                // NOTE: we submit priority+1 because the default behavior of bump() is to actually use priority - 1
                // and we want all priorities of the objects to be at the SAME level
                motionState->bump(priority + 1);
            }
        }
    }
}

void PhysicsWorld::collectCollisionEvents(const glm::vec3& originOffset, CollisionEvents& events) {
    // scan known contacts and trigger events
    ContactMap::iterator contactItr = _contactMap.begin();

    while (contactItr != _contactMap.end()) {
        ContactInfo& contact = contactItr->second;
        ContactEventType type = contact.computeType(_numContactFrames);
        const float SIGNIFICANT_DEPTH = -0.002f; // penetrations have negative distance
        if (type != CONTACT_EVENT_TYPE_CONTINUE ||
                (contact.distance < SIGNIFICANT_DEPTH &&
                 contact.readyForContinue(_numContactFrames))) {
            ObjectMotionState* motionStateA = static_cast<ObjectMotionState*>(contactItr->first._a);
            ObjectMotionState* motionStateB = static_cast<ObjectMotionState*>(contactItr->first._b);

            // NOTE: the MyAvatar character is the only object in the simulation that does NOT have a MotionState
            // which means should we ever want to report ALL collision events against the avatar we can
            // modify the logic below.
            //
            // We only create events when at least one of the objects is (or should be) owned in the local simulation.
            if (motionStateA && (motionStateA->isLocallyOwnedOrShouldBe())) {
                QUuid idA = motionStateA->getObjectID();
                QUuid idB;
                if (motionStateB) {
                    idB = motionStateB->getObjectID();
                }
                glm::vec3 position = toParent(contact.getPositionWorldOnB()) + originOffset;
                glm::vec3 velocityChange = motionStateA->getObjectLinearVelocityChange() +
                    (motionStateB ? motionStateB->getObjectLinearVelocityChange() : glm::vec3(0.0f));
                glm::vec3 penetration = _frameRotation * (contact.distance * contact.normalWorldOnB);
                events.push_back(Collision(type, idA, idB, position, penetration, velocityChange));
            } else if (motionStateB && (motionStateB->isLocallyOwnedOrShouldBe())) {
                QUuid idB = motionStateB->getObjectID();
                QUuid idA;
                if (motionStateA) {
                    idA = motionStateA->getObjectID();
                }
                glm::vec3 position = toParent(contact.getPositionWorldOnA()) + originOffset;
                glm::vec3 velocityChange = motionStateB->getObjectLinearVelocityChange() +
                    (motionStateA ? motionStateA->getObjectLinearVelocityChange() : glm::vec3(0.0f));
                // NOTE: we're flipping the order of A and B (so that the first objectID is never NULL)
                // hence we negate the penetration (because penetration always points from B to A).
                glm::vec3 penetration = - (_frameRotation * (contact.distance * contact.normalWorldOnB));
                events.push_back(Collision(type, idB, idA, position, penetration, velocityChange));
            }
        }

        if (type == CONTACT_EVENT_TYPE_END) {
            contactItr = _contactMap.erase(contactItr);
        } else {
            ++contactItr;
        }
    }
}

void PhysicsWorld::harvestChanges(VectorOfMotionStates& changed, VectorOfMotionStates& deactivated) {
    // every body that is awake has (possibly) moved: copy it out
    JPH::BodyIDVector activeBodies;
    _system->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);
    const JPH::BodyLockInterfaceNoLock& lockInterface = _system->GetBodyLockInterfaceNoLock();
    for (const JPH::BodyID& bodyID : activeBodies) {
        JPH::BodyLockRead lock(lockInterface, bodyID);
        if (!lock.Succeeded()) {
            continue;
        }
        ObjectMotionState* motionState = motionStateOf(lock.GetBody());
        if (motionState) {
            motionState->pullTransformFromBody();
            changed.push_back(motionState);
        }
    }

    // bodies that fell asleep since the last harvest
    std::vector<ObjectMotionState*> sleepers;
    _activationRecorder->takeDeactivated(sleepers);
    for (ObjectMotionState* motionState : sleepers) {
        if (_motionStates.find(motionState) != _motionStates.end()) {
            motionState->pullTransformFromBody();
            deactivated.push_back(motionState);
        }
    }

    // moved static bodies are reported once, then forgotten
    for (ObjectMotionState* motionState : _activeStaticObjects) {
        changed.push_back(motionState);
    }
    _activeStaticObjects.clear();
}

void PhysicsWorld::debugDraw(uint32_t mode, PhysicsDebugDraw* renderer) {
    if (!renderer || mode == PhysicsDebugDraw::DRAW_NOTHING) {
        return;
    }
    renderer->setFrameTransform(_framePosition, _frameRotation);
    JPH::BodyManager::DrawSettings drawSettings;
    drawSettings.mDrawShape = (mode & PhysicsDebugDraw::DRAW_WIREFRAME) != 0;
    drawSettings.mDrawShapeWireframe = true;
    drawSettings.mDrawBoundingBox = (mode & PhysicsDebugDraw::DRAW_AABBS) != 0;
    if (drawSettings.mDrawShape || drawSettings.mDrawBoundingBox) {
        _system->DrawBodies(drawSettings, renderer);
    }
    if (mode & PhysicsDebugDraw::DRAW_CONSTRAINTS) {
        _system->DrawConstraints(renderer);
    }
    if (mode & PhysicsDebugDraw::DRAW_CONSTRAINT_LIMITS) {
        _system->DrawConstraintLimits(renderer);
    }
    if (mode & PhysicsDebugDraw::DRAW_CONTACT_POINTS) {
        const glm::vec3 RED(1.0f, 0.0f, 0.0f);
        for (auto& contact : _contactMap) {
            renderer->drawContactPoint(contact.second.positionWorldOnB, contact.second.normalWorldOnB, contact.second.distance, RED);
        }
    }
    renderer->setFrameTransform(glm::vec3(0.0f), glm::quat());
}

// Everything within the given shape, as the zones use it to find what is inside them.
std::vector<ContactTestResult> PhysicsWorld::contactTest(uint16_t mask, const ShapeInfo& regionShapeInfo, const Transform& regionTransform,
                                                         uint16_t group, float threshold, const JPH::BodyID& characterBodyID) const {
    std::vector<ContactTestResult> contacts;
    if (!_system) {
        return contacts;
    }
    const JPH::Shape* shape = ObjectMotionState::getShapeManager()->getShape(regionShapeInfo);
    if (!shape) {
        return contacts;
    }

    JPH::ObjectLayer layer = JoltLayers::layerFor((int32_t)group, (int32_t)mask);
    JPH::DefaultBroadPhaseLayerFilter broadPhaseFilter(_layers->objectVsBroadPhase, layer);
    JPH::DefaultObjectLayerFilter objectLayerFilter(_layers->objectLayerPairs, layer);

    JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(glmToJolt(regionTransform.getRotation()), glmToJoltR(regionTransform.getTranslation()));
    JPH::RMat44 centerOfMassTransform = transform.PreTranslated(shape->GetCenterOfMass());
    JPH::CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0f;
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    _system->GetNarrowPhaseQueryNoLock().CollideShape(shape, JPH::Vec3::sReplicate(1.0f), centerOfMassTransform, settings,
                                                       JPH::RVec3::sZero(), collector, broadPhaseFilter, objectLayerFilter);

    for (const JPH::CollideShapeResult& hit : collector.mHits) {
        if (hit.mPenetrationDepth < threshold) {
            continue;
        }
        // the penetration axis points from the test shape (1) toward the found shape (2)
        glm::vec3 axis = joltToGLM(hit.mPenetrationAxis);
        float axisLength = glm::length(axis);
        glm::vec3 normal = axisLength > 0.0f ? -axis / axisLength : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 pointOnTest = joltToGLM(hit.mContactPointOn1);
        glm::vec3 pointOnFound = joltToGLM(hit.mContactPointOn2);

        if (!characterBodyID.IsInvalid() && hit.mBodyID2 == characterBodyID) {
            contacts.emplace_back(Physics::getSessionUUID(), pointOnTest, pointOnFound, normal);
            continue;
        }
        JPH::BodyLockRead lock(_system->GetBodyLockInterfaceNoLock(), hit.mBodyID2);
        if (!lock.Succeeded()) {
            continue;
        }
        ObjectMotionState* candidate = motionStateOf(lock.GetBody());
        if (!candidate) {
            continue;
        }
        contacts.emplace_back(candidate->getObjectID(), pointOnTest, pointOnFound, normal);
    }

    ObjectMotionState::getShapeManager()->releaseShape(shape);
    return contacts;
}

bool PhysicsWorld::castRayClosest(const glm::vec3& start, const glm::vec3& end, int32_t group, int32_t mask,
                                  const JPH::BodyFilter* bodyFilter, RayHit& result) const {
    result = RayHit();
    if (!_system) {
        return false;
    }
    JPH::ObjectLayer layer = JoltLayers::layerFor(group, mask);
    JPH::DefaultBroadPhaseLayerFilter broadPhaseFilter(_layers->objectVsBroadPhase, layer);
    JPH::DefaultObjectLayerFilter objectLayerFilter(_layers->objectLayerPairs, layer);
    JPH::BodyFilter noBodyFilter;

    JPH::RRayCast ray(glmToJoltR(start), glmToJolt(end - start));
    JPH::RayCastResult hit;
    bool found = _system->GetNarrowPhaseQueryNoLock().CastRay(ray, hit, broadPhaseFilter, objectLayerFilter,
                                                              bodyFilter ? *bodyFilter : noBodyFilter);
    if (!found) {
        return false;
    }
    result.hit = true;
    result.fraction = hit.mFraction;
    result.bodyID = hit.mBodyID;
    result.point = start + hit.mFraction * (end - start);
    JPH::BodyLockRead lock(_system->GetBodyLockInterfaceNoLock(), hit.mBodyID);
    if (lock.Succeeded()) {
        const JPH::Body& body = lock.GetBody();
        result.motionState = motionStateOf(body);
        result.normal = joltToGLM(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, glmToJoltR(result.point)));
    }
    return true;
}

void PhysicsWorld::castRayAll(const glm::vec3& start, const glm::vec3& end, int32_t group, int32_t mask,
                              std::vector<RayHit>& results) const {
    results.clear();
    if (!_system) {
        return;
    }
    JPH::ObjectLayer layer = JoltLayers::layerFor(group, mask);
    JPH::DefaultBroadPhaseLayerFilter broadPhaseFilter(_layers->objectVsBroadPhase, layer);
    JPH::DefaultObjectLayerFilter objectLayerFilter(_layers->objectLayerPairs, layer);

    JPH::RRayCast ray(glmToJoltR(start), glmToJolt(end - start));
    JPH::RayCastSettings settings;
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    _system->GetNarrowPhaseQueryNoLock().CastRay(ray, settings, collector, broadPhaseFilter, objectLayerFilter);
    collector.Sort();
    results.reserve(collector.mHits.size());
    for (const JPH::RayCastResult& hit : collector.mHits) {
        RayHit result;
        result.hit = true;
        result.fraction = hit.mFraction;
        result.bodyID = hit.mBodyID;
        result.point = start + hit.mFraction * (end - start);
        JPH::BodyLockRead lock(_system->GetBodyLockInterfaceNoLock(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            result.motionState = motionStateOf(body);
            result.normal = joltToGLM(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, glmToJoltR(result.point)));
        }
        results.push_back(result);
    }
}
