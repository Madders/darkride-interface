//
//  PhysicsWorld.h
//  libraries/physics/src
//
//  One Jolt physics system and the bookkeeping around it: the bodies in it, their contacts, which of them
//  fell asleep, the ray casts and shape queries against it, and its debug drawing.
//
//  The engine owns one such world for the domain (the root) and one for every moving reference frame
//  (PHYSICS.md 4.1): a frame's interior is simulated in the frame's own world, in the frame's local
//  coordinates, and never sees the frame move. The frame's transform in its parent world is handed to the
//  world before each substep, for gravity's direction and for whoever needs to convert coordinates.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PhysicsWorld_h
#define hifi_PhysicsWorld_h

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <QtCore/QStringList>
#include <QUuid>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <ShapeInfo.h>
#include <Transform.h>

#include "JoltLayers.h"
#include "ContactInfo.h"
#include "ObjectMotionState.h"

namespace JPH {
    class TempAllocator;
    class JobSystem;
    class BodyFilter;
}

class CharacterController;
class PhysicsDebugDraw;

// A contact between two simulated objects. Only MyAvatar's body has no ObjectMotionState, so a null _a
// means the avatar's character. Both null never happens.
class ContactKey {
public:
    ContactKey() = delete;
    ContactKey(void* a, void* b) : _a(a), _b(b) {}
    bool operator<(const ContactKey& other) const { return _a < other._a || (_a == other._a && _b < other._b); }
    bool operator==(const ContactKey& other) const { return _a == other._a && _b == other._b; }
    void* _a; // ObjectMotionState pointer
    void* _b; // ObjectMotionState pointer
};

struct ContactTestResult {
    ContactTestResult() = delete;

    ContactTestResult(const ContactTestResult& contactTestResult) :
        foundID(contactTestResult.foundID),
        testCollisionPoint(contactTestResult.testCollisionPoint),
        foundCollisionPoint(contactTestResult.foundCollisionPoint),
        collisionNormal(contactTestResult.collisionNormal) {
    }

    ContactTestResult(const QUuid& foundID, const glm::vec3& testCollisionPoint, const glm::vec3& otherCollisionPoint, const glm::vec3& collisionNormal) :
        foundID(foundID),
        testCollisionPoint(testCollisionPoint),
        foundCollisionPoint(otherCollisionPoint),
        collisionNormal(collisionNormal) {
    }

    QUuid foundID;
    // The deepest point of foundID's shape within the test shape, in world space.
    glm::vec3 testCollisionPoint;
    // The deepest point of the test shape within foundID's shape, in world space.
    glm::vec3 foundCollisionPoint;
    // The normal vector in world space of the test shape at foundCollisionPoint.
    glm::vec3 collisionNormal;
};

// What a ray cast hit, in the coordinates of the world that was cast in.
struct RayHit {
    bool hit { false };
    float fraction { 1.0f };
    JPH::BodyID bodyID;
    ObjectMotionState* motionState { nullptr }; // null for MyAvatar's own body
    glm::vec3 point { 0.0f };
    glm::vec3 normal { 0.0f, 1.0f, 0.0f };
};

using ContactMap = std::map<ContactKey, ContactInfo>;
using CollisionEvents = std::vector<Collision>;

class PhysicsWorld {
public:
    // The root world has a null frame ID; a frame's world carries its frame entity's ID.
    PhysicsWorld(const QUuid& frameID, JPH::TempAllocator* tempAllocator, JPH::JobSystem* jobSystem);
    ~PhysicsWorld();

    const QUuid& getFrameID() const { return _frameID; }
    bool isRoot() const { return _frameID.isNull(); }

    // The frame's transform in its parent world, set before every substep. Identity for the root.
    void setFrameTransform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& linearVelocity, const glm::vec3& angularVelocity);
    const glm::vec3& getFramePosition() const { return _framePosition; }
    const glm::quat& getFrameRotation() const { return _frameRotation; }
    const glm::vec3& getFrameLinearVelocity() const { return _frameLinearVelocity; }
    const glm::vec3& getFrameAngularVelocity() const { return _frameAngularVelocity; }
    // between this world's coordinates and its parent's
    glm::vec3 toParent(const glm::vec3& localPosition) const { return _framePosition + _frameRotation * localPosition; }
    glm::quat toParent(const glm::quat& localRotation) const { return _frameRotation * localRotation; }
    glm::vec3 toLocal(const glm::vec3& parentPosition) const { return glm::inverse(_frameRotation) * (parentPosition - _framePosition); }
    glm::quat toLocal(const glm::quat& parentRotation) const { return glm::inverse(_frameRotation) * parentRotation; }
    // a velocity at a point of the parent world, as seen from inside the frame (the frame's own motion removed)
    glm::vec3 velocityToLocal(const glm::vec3& parentVelocity, const glm::vec3& parentPosition) const {
        return glm::inverse(_frameRotation) * (parentVelocity - _frameLinearVelocity - glm::cross(_frameAngularVelocity, parentPosition - _framePosition));
    }
    glm::vec3 velocityToParent(const glm::vec3& localVelocity, const glm::vec3& localPosition) const {
        return _frameLinearVelocity + glm::cross(_frameAngularVelocity, _frameRotation * localPosition) + _frameRotation * localVelocity;
    }

    JPH::PhysicsSystem* getPhysicsSystem() const { return _system.get(); }
    JPH::BodyInterface& getBodyInterface() const;
    JPH::TempAllocator* getTempAllocator() const { return _tempAllocator; }
    const JoltLayers::Tables& getLayers() const { return *_layers; }
    int32_t getNumBodies() const;
    uint32_t getNumActiveBodies() const;
    // the active bodies that are dynamic (Jolt counts moving kinematic ones too, a keyframed entity among them), and a
    // few of their names for a log
    uint32_t getNumActiveDynamicBodies(QStringList* names = nullptr, int maxNames = 0) const;
    size_t getNumObjects() const { return _motionStates.size(); }
    bool contains(ObjectMotionState* motionState) const { return _motionStates.find(motionState) != _motionStates.end(); }
    const std::set<ObjectMotionState*>& getMotionStates() const { return _motionStates; }

    // bodies: the motion state's object position and rotation are taken as this world's coordinates
    void addObject(ObjectMotionState* motionState);
    void removeObject(ObjectMotionState* motionState);
    void reinsertObject(ObjectMotionState* motionState);
    void markStaticObjectActive(ObjectMotionState* motionState);
    void removeAllObjects();

    // The frame's own hull, as a static body at the origin of this world: the floor its riders stand on and
    // the walls they bump into. The frame's moving body is in the parent world; this is its twin here. Null
    // removes it. Contacts with it are reported against the frame's motion state.
    void setFrameHull(ObjectMotionState* frameState);
    bool hasFrameHull() const { return !_frameHullID.IsInvalid(); }
    // The interior changed (a static body came, went or moved): the frame's copy of it in the parent world
    // (ObjectMotionState::setInteriorShape) is rebuilt by the engine, which takes the flag.
    void markInteriorChanged() { _interiorChanged = true; }
    bool takeInteriorChanged() { bool changed = _interiorChanged; _interiorChanged = false; return changed; }

    // one substep: kinematics pushed (parents before children), the character moved, gravity applied, the
    // world stepped, contacts harvested
    void stepOnce(float dt, CharacterController* character);
    // Rollback for prediction (PHYSICS.md 4.6, round 6): the state of every non-static body but one (the vehicle, which
    // is set from the authority's state instead), as Jolt's state recorder writes it, and the same put back; then
    // substeps run again with nothing but gravity and Jolt (no kinematic push, no character, no contact events).
    std::string saveBodies(const JPH::BodyID& exclude) const;
    bool restoreBodies(const std::string& saved, const JPH::BodyID& exclude);
    void replayStep(float dt);

    // after the frame's steps: every body that was active is pulled, the sleepers are listed
    void harvestChanges(VectorOfMotionStates& changed, VectorOfMotionStates& deactivated);
    // contact events since the last call, in this world's coordinates plus the given offset
    void collectCollisionEvents(const glm::vec3& originOffset, CollisionEvents& events);

    void removeContacts(ObjectMotionState* motionState);
    void wakeBodiesTouching(ObjectMotionState* motionState);
    void recordCharacterContact(ObjectMotionState* other, const glm::vec3& positionOnOther, const glm::vec3& normalTowardCharacter, float distance);
    void doOwnershipInfectionForConstraints(const std::vector<std::vector<JPH::BodyID>>& constraintBodies, const JPH::BodyID& characterBodyID);
    size_t getNumContacts() const { return _contactMap.size(); }

    // queries, in this world's coordinates
    std::vector<ContactTestResult> contactTest(uint16_t mask, const ShapeInfo& regionShapeInfo, const Transform& regionTransform,
                                               uint16_t group, float threshold, const JPH::BodyID& characterBodyID) const;
    bool castRayClosest(const glm::vec3& start, const glm::vec3& end, int32_t group, int32_t mask,
                        const JPH::BodyFilter* bodyFilter, RayHit& result) const;
    void castRayAll(const glm::vec3& start, const glm::vec3& end, int32_t group, int32_t mask, std::vector<RayHit>& results) const;
    // a sphere of the radius swept from start to end, filtered as castRayClosest is: the first body it meets on the way.
    // A body the sphere is already inside at the start is skipped (it reports fraction 0); a ray from the same start
    // finds that one.
    bool castSphereClosest(const glm::vec3& start, const glm::vec3& end, float radius, int32_t group, int32_t mask,
                           const JPH::BodyFilter* bodyFilter, RayHit& result) const;
    // the way out for a sphere of the radius at the centre, filtered as the casts are: the sum, over the bodies it
    // overlaps, of the push along each one's shortest way out (zero when it touches nothing)
    glm::vec3 sphereOverlapPush(const glm::vec3& center, float radius, int32_t group, int32_t mask,
                                const JPH::BodyFilter* bodyFilter) const;

    void debugDraw(uint32_t mode, PhysicsDebugDraw* renderer);

    // the motion state behind a body, or null for MyAvatar's character and unknown bodies
    static ObjectMotionState* motionStateOf(const JPH::Body& body);

private:
    class ContactRecorder;
    class ActivationRecorder;

    void createBody(ObjectMotionState* motionState);
    void destroyBody(ObjectMotionState* motionState);
    void wakeBodiesAround(const JPH::Body* body);
    void applyGravityForces();
    void rememberSubstepStarts();
    void harvestContacts();
    void doOwnershipInfection(ObjectMotionState* motionStateA, bool aIsCharacter, bool aIsStatic,
                              ObjectMotionState* motionStateB, bool bIsStatic, bool bIsKinematic);

    QUuid _frameID;
    glm::vec3 _framePosition { 0.0f };
    glm::quat _frameRotation;
    glm::vec3 _frameLinearVelocity { 0.0f };
    glm::vec3 _frameAngularVelocity { 0.0f };

    JPH::TempAllocator* _tempAllocator { nullptr };
    JPH::JobSystem* _jobSystem { nullptr };
    std::unique_ptr<JoltLayers::Tables> _layers;
    std::unique_ptr<JPH::PhysicsSystem> _system;
    std::unique_ptr<ContactRecorder> _contactRecorder;
    bool _replaying { false };
    std::unique_ptr<ActivationRecorder> _activationRecorder;

    ContactMap _contactMap;
    std::set<ObjectMotionState*> _motionStates; // every motion state with a body in this world
    std::set<ObjectMotionState*> _kinematicMotionStates;
    std::set<ObjectMotionState*> _activeStaticObjects;
    JPH::BodyID _frameHullID;
    const JPH::Shape* _frameHullShape { nullptr };
    bool _interiorChanged { false };
    uint32_t _numContactFrames { 0 };
};

#endif // hifi_PhysicsWorld_h
