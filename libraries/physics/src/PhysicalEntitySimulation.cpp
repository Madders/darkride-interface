//
//  PhysicalEntitySimulation.cpp
//  libraries/physics/src
//
//  Created by Andrew Meadows 2015.04.27
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "PhysicalEntitySimulation.h"

#include "TrackSimulation.h"
#include "VehicleSimulation.h"

#include <Profile.h>

#include "PhysicsHelpers.h"
#include "PhysicsLogging.h"
#include "ShapeManager.h"

#include <algorithm>
#include <chrono>

#include <NLPacket.h>
#include <NodeList.h>
#include <PortableHighResolutionClock.h>
#include <VehicleState.h>

namespace {
    // The physics server drives a car on a track without its real shape: it collides with nothing this round and loads
    // no models, so a model car would wait for a shape forever. It gets a box of its dimensions instead.
    bool serverDrivesAsBox(const EntityItemPointer& entity) {
        // the server loads no model hulls: a car on a track and a vehicle's chassis are boxes of their dimensions there
        return Physics::isServerAuthority() && (!entity->getTrackID().isNull() || entity->isVehicle());
    }

    void computeEntityShape(const EntityItemPointer& entity, ShapeInfo& shapeInfo) {
        if (serverDrivesAsBox(entity)) {
            shapeInfo.setBox(0.5f * entity->getScaledDimensions());
            return;
        }
        entity->computeShapeInfo(shapeInfo);
        if (Physics::isServerAuthority() && shapeInfo.getPointCollection().empty()) {
            // the server loads no assets: a hull, a compound or a mesh without its points gets no body here (such an
            // entity is in the server's tree as the ancestor of something it simulates, for its transform)
            switch (shapeInfo.getType()) {
                case SHAPE_TYPE_SIMPLE_HULL:
                case SHAPE_TYPE_SIMPLE_COMPOUND:
                case SHAPE_TYPE_COMPOUND:
                case SHAPE_TYPE_STATIC_MESH:
                    shapeInfo.setParams(SHAPE_TYPE_NONE, shapeInfo.getHalfExtents());
                    break;
                default:
                    break;
            }
        }
    }
}


PhysicalEntitySimulation::PhysicalEntitySimulation() {
    _trackSimulation = std::make_unique<TrackSimulation>();
    _vehicleSimulation = std::make_unique<VehicleSimulation>();
}

PhysicalEntitySimulation::~PhysicalEntitySimulation() {
    if (_physicsEngine) {
        _physicsEngine->setSubstepListener(nullptr);
    }
    _vehicleSimulation->clear();
}

void PhysicalEntitySimulation::init(
        EntityTreePointer tree,
        PhysicsEnginePointer physicsEngine,
        EntityEditPacketSender* packetSender) {
    assert(tree);
    setEntityTree(tree);

    assert(physicsEngine);
    _physicsEngine = physicsEngine;
    _vehicleSimulation->setEngine(physicsEngine.get());
    physicsEngine->setSubstepListener(_vehicleSimulation.get());

    assert(packetSender);
    _entityPacketSender = packetSender;

    // the physics server's state stream (PHYSICS.md round 9): a client hears the states, the server the requests
    auto nodeList = DependencyManager::get<NodeList>();
    if (nodeList && !_streamListenersRegistered) {
        auto& packetReceiver = nodeList->getPacketReceiver();
        packetReceiver.registerListener(PacketType::VehicleState, this, "handleVehicleStatePacket");
        packetReceiver.registerListener(PacketType::VehicleStateRequest, this, "handleVehicleStateRequest");
        _streamListenersRegistered = true;
    }
}

void PhysicalEntitySimulation::handleVehicleStatePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    if (!senderNode || senderNode->getType() != NodeType::PhysicsServer || !_vehicleSimulation) {
        return;
    }
    std::vector<VehicleState> states;
    if (!VehicleStatePacket::unpack(message->getMessage(), states)) {
        qCWarning(physics) << "a vehicle state packet from" << senderNode->getUUID() << "could not be read";
        return;
    }
    // when it arrived at the socket, on this clock (the receive time is on another clock; the wait since is the same)
    quint64 arrival = usecTimestampNow();
    if (message->getFirstPacketReceiveTime() > 0) {
        qint64 nowReceiveClock = std::chrono::duration_cast<std::chrono::microseconds>(
            p_high_resolution_clock::now().time_since_epoch()).count();
        qint64 queued = nowReceiveClock - message->getFirstPacketReceiveTime();
        if (queued > 0 && (quint64)queued < arrival) {
            arrival -= (quint64)queued;
        }
    }
    _vehicleSimulation->receiveStates(states, senderNode->getClockSkewUsec(), arrival);
}

void PhysicalEntitySimulation::handleVehicleStateRequest(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    if (!senderNode || !_vehicleSimulation) {
        return;
    }
    std::vector<QUuid> ids;
    if (!VehicleStateRequestPacket::unpack(message->getMessage(), ids)) {
        qCWarning(physics) << "a vehicle state request from" << senderNode->getUUID() << "could not be read";
        return;
    }
    _vehicleSimulation->receiveStateRequest(senderNode->getUUID(), ids);
}

uint8_t PhysicalEntitySimulation::regionOf(const EntityItemPointer& entity) const {
    return _space ? _space->getRegion(entity->getSpaceIndex()) : (uint8_t)workload::Region::R1;
}

// begin EntitySimulation overrides
void PhysicalEntitySimulation::addEntityToInternalLists(EntityItemPointer entity) {
    EntitySimulation::addEntityToInternalLists(entity);
    entity->deserializeActions(); // TODO: do this elsewhere
    uint8_t region = regionOf(entity);
    bool maybeShouldBePhysical = (region < workload::Region::R3 || region == workload::Region::UNKNOWN) && entity->shouldBePhysical();
    bool canBeKinematic = region <= workload::Region::R4;
    if (maybeShouldBePhysical) {
        EntityMotionState* motionState = static_cast<EntityMotionState*>(entity->getPhysicsInfo());
        if (motionState) {
            motionState->setRegion(region);
        } else {
            _entitiesToAddToPhysics.insert(entity);
        }
    } else if (canBeKinematic && entity->isMovingRelativeToParent()) {
        SetOfEntities::iterator itr = _simpleKinematicEntities.find(entity);
        if (itr == _simpleKinematicEntities.end()) {
            _simpleKinematicEntities.insert(entity);
        }
    }
}

void PhysicalEntitySimulation::removeEntityFromInternalLists(EntityItemPointer entity) {
    _entitiesToAddToPhysics.remove(entity);
    EntityMotionState* motionState = static_cast<EntityMotionState*>(entity->getPhysicsInfo());
    if (motionState) {
        removeOwnershipData(motionState);
        _entitiesToRemoveFromPhysics.insert(entity);
    }
    if (entity->isDead() && entity->getElement()) {
        _deadEntitiesToRemoveFromTree.insert(entity);
    }
    if (entity->isAvatarEntity()) {
        _deadAvatarEntities.insert(entity);
    }
    EntitySimulation::removeEntityFromInternalLists(entity);
}

void PhysicalEntitySimulation::removeOwnershipData(EntityMotionState* motionState) {
    assert(motionState);
    if (motionState->getOwnershipState() == EntityMotionState::OwnershipState::LocallyOwned) {
        for (uint32_t i = 0; i < _owned.size(); ++i) {
            if (_owned[i] == motionState) {
                _owned[i]->clearOwnershipState();
                _owned.remove(i);
            }
        }
    } else if (motionState->getOwnershipState() == EntityMotionState::OwnershipState::PendingBid) {
        for (uint32_t i = 0; i < _bids.size(); ++i) {
            if (_bids[i] == motionState) {
                _bids[i]->clearOwnershipState();
                _bids.remove(i);
            }
        }
    }
}

void PhysicalEntitySimulation::clearOwnershipData() {
    for (uint32_t i = 0; i < _owned.size(); ++i) {
        _owned[i]->clearOwnershipState();
    }
    _owned.clear();
    for (uint32_t i = 0; i < _bids.size(); ++i) {
        _bids[i]->clearOwnershipState();
    }
    _bids.clear();
}

void PhysicalEntitySimulation::takeDeadAvatarEntities(SetOfEntities& deadEntities) {
    _deadAvatarEntities.swap(deadEntities);
    _deadAvatarEntities.clear();
}

void PhysicalEntitySimulation::processChangedEntity(const EntityItemPointer& entity) {
    EntitySimulation::processChangedEntity(entity);

    // queue incoming changes: from external sources (script, EntityServer, etc) to physics engine
    EntityMotionState* motionState = static_cast<EntityMotionState*>(entity->getPhysicsInfo());
    uint8_t region = regionOf(entity);
    bool shouldBePhysical = region < workload::Region::R3 && entity->shouldBePhysical();
    bool canBeKinematic = region <= workload::Region::R4;
    if (motionState) {
        if (!shouldBePhysical) {
            if (motionState->isLocallyOwned()) {
                // zero velocities by first deactivating the body
                motionState->deactivateBody();

                // send packet to remove ownership
                // NOTE: this packet will NOT be resent if lost, but the good news is:
                // the entity-server will eventually clear velocity and ownership for timeout
                motionState->sendUpdate(_entityPacketSender, _physicsEngine->getNumSubsteps());
            }

            // remove from the physical simulation
            _incomingChanges.remove(motionState);
            removeOwnershipData(motionState);
            _entitiesToRemoveFromPhysics.insert(entity);
            if (canBeKinematic && entity->isMovingRelativeToParent()) {
                SetOfEntities::iterator itr = _simpleKinematicEntities.find(entity);
                if (itr == _simpleKinematicEntities.end()) {
                    _simpleKinematicEntities.insert(entity);
                }
            }
        } else {
            _incomingChanges.insert(motionState);
        }
        motionState->setRegion(region);
    } else if (shouldBePhysical) {
        // The intent is for this object to be in the PhysicsEngine, but it has no MotionState yet.
        // Perhaps it's shape has changed and it can now be added?
        _entitiesToAddToPhysics.insert(entity);
        SetOfEntities::iterator itr = _simpleKinematicEntities.find(entity);
        if (itr != _simpleKinematicEntities.end()) {
            _simpleKinematicEntities.erase(itr);
        }
    } else if (canBeKinematic && entity->isMovingRelativeToParent()) {
        SetOfEntities::iterator itr = _simpleKinematicEntities.find(entity);
        if (itr == _simpleKinematicEntities.end()) {
            _simpleKinematicEntities.insert(entity);
        }
    } else {
        SetOfEntities::iterator itr = _simpleKinematicEntities.find(entity);
        if (itr != _simpleKinematicEntities.end()) {
            _simpleKinematicEntities.erase(itr);
        }
    }
}

void PhysicalEntitySimulation::processDeadEntities() {
    // Note: this override is a complete rewite of the base class's method because we cannot assume all entities
    // are domain entities, and the consequence of trying to delete a domain-entity in this case is very different.
    if (_deadEntitiesToRemoveFromTree.empty()) {
        return;
    }
    PROFILE_RANGE(simulation_physics, "Deletes");
    std::vector<EntityItemPointer> entitiesToDeleteImmediately;
    entitiesToDeleteImmediately.reserve(_deadEntitiesToRemoveFromTree.size());
    QUuid sessionID = Physics::getSessionUUID();
    QMutexLocker lock(&_mutex);
    for (auto entity : _deadEntitiesToRemoveFromTree) {
        EntityMotionState* motionState = static_cast<EntityMotionState*>(entity->getPhysicsInfo());
        if (motionState) {
            _entitiesToRemoveFromPhysics.insert(entity);
        }
        if (entity->isDomainEntity()) {
            // interface-client can't delete domainEntities outright, they must roundtrip through the entity-server
            _entityPacketSender->queueEraseEntityMessage(entity->getID());
        } else if (entity->isLocalEntity() || entity->isMyAvatarEntity()) {
            entitiesToDeleteImmediately.push_back(entity);
            entity->collectChildrenForDelete(entitiesToDeleteImmediately, sessionID);
        }
    }
    _deadEntitiesToRemoveFromTree.clear();

    if (!entitiesToDeleteImmediately.empty()) {
        getEntityTree()->deleteEntitiesByPointer(entitiesToDeleteImmediately);
    }
}

void PhysicalEntitySimulation::clearEntities() {
    // TODO: we should probably wait to lock the _physicsEngine so we don't mess up data structures
    // while it is in the middle of a simulation step.  As it is, we're probably in shutdown mode
    // anyway, so maybe the simulation was already properly shutdown?  Cross our fingers...

    QMutexLocker lock(&_mutex);
    _vehicleSimulation->clear();
    // remove the objects (aka MotionStates) from physics
    _physicsEngine->removeSetOfObjects(_physicalObjects);

    clearOwnershipData();

    // delete the MotionStates
    for (auto stateItr : _physicalObjects) {
        EntityMotionState* motionState = static_cast<EntityMotionState*>(&(*stateItr));
        assert(motionState);
        EntityItemPointer entity = motionState->getEntity();
        // TODO: someday when we invert the entities/physics lib dependencies we can let EntityItem delete its own PhysicsInfo
        // until then we must do it here
        delete motionState;
    }
    _physicalObjects.clear();

    // clear all other lists specific to this derived class
    _entitiesToRemoveFromPhysics.clear();
    _entitiesToAddToPhysics.clear();
    _incomingChanges.clear();
    _entitiesToDeleteLater.clear();

    EntitySimulation::clearEntities();
}

QVariantMap PhysicalEntitySimulation::getVehicleStats() const {
    const VehicleSimulation::Stats& stats = _vehicleSimulation->getStats();
    QVariantMap map;
    map["vehicles"] = stats.vehicles;
    map["corrections"] = stats.corrections;
    map["replaySteps"] = stats.replaySteps;
    map["lastError"] = stats.lastError;
    map["maxError"] = stats.maxError;
    map["inputsSent"] = stats.inputsSent;
    map["inputsReceived"] = stats.inputsReceived;
    map["inputsDropped"] = stats.inputsDropped;
    map["inputsStarved"] = stats.inputsStarved;
    // the input buffer (round 10)
    map["inputsLate"] = stats.inputsLate;
    map["inputsEarly"] = stats.inputsEarly;
    map["bufferRealigned"] = stats.bufferRealigned;
    map["bufferMargin"] = stats.bufferMargin;
    map["leadShifts"] = stats.leadShifts;
    map["leadJumps"] = stats.leadJumps;
    map["timeShiftMs"] = stats.timeShiftMs;
    // the state stream (round 9)
    map["streamStatesSent"] = stats.streamStatesSent;
    map["streamStatesReceived"] = stats.streamStatesReceived;
    map["streamExtrapolations"] = stats.streamExtrapolations;
    map["streamSubscribers"] = stats.streamSubscribers;
    map["rttMs"] = stats.rttMs;
    map["delayMs"] = stats.delayMs;
    map["streamIntervalMs"] = stats.streamIntervalMs;
    map["streamJitterMs"] = stats.streamJitterMs;

    VehicleSimulation::Telemetry live = _vehicleSimulation->getTelemetry();
    if (live.valid) {
        QVariantMap telemetry;
        telemetry["engineRPM"] = live.engineRPM;
        telemetry["gear"] = live.gear;
        telemetry["clutch"] = live.clutch;
        telemetry["forward"] = live.forward;
        telemetry["right"] = live.right;
        telemetry["brake"] = live.brake;
        telemetry["handBrake"] = live.handBrake;
        telemetry["speed"] = live.speed;
        if (live.airship) {
            telemetry["airship"] = true;
            telemetry["altitude"] = live.altitude;
            telemetry["airDensity"] = live.airDensity;
            telemetry["buoyancy"] = live.buoyancy;
            telemetry["ballastWeight"] = live.ballastWeight;
            telemetry["thrust"] = live.thrust;
            telemetry["verticalThrust"] = live.verticalThrust;
            telemetry["airspeed"] = live.airspeed;
            telemetry["sideslip"] = live.sideslip;
            telemetry["incidence"] = live.incidence;
            telemetry["climbRate"] = live.climbRate;
            telemetry["ballast"] = live.ballast;
            telemetry["fill"] = live.fill;
            telemetry["pitch"] = live.pitch;
            telemetry["lift"] = live.lift;
            telemetry["trim"] = live.trim;
        }
        if (live.aircraft) {
            telemetry["aircraft"] = true;
            telemetry["altitude"] = live.altitude;
            telemetry["airDensity"] = live.airDensity;
            telemetry["thrust"] = live.thrust;
            telemetry["tilt"] = live.tilt;
            telemetry["airspeed"] = live.airspeed;
            telemetry["angleOfAttack"] = live.angleOfAttack;
            telemetry["sideslip"] = live.sideslip;
            telemetry["climbRate"] = live.climbRate;
            telemetry["wingLift"] = live.wingLift;
            telemetry["wingDrag"] = live.wingDrag;
            telemetry["liftCoefficient"] = live.liftCoefficient;
            telemetry["pitch"] = live.pitch;
            telemetry["lift"] = live.lift;
            telemetry["trim"] = live.trim;
        }
        QVariantList wheelRPM, wheelSlip, suspension, contact;
        for (size_t i = 0; i < live.wheelRPM.size(); ++i) {
            wheelRPM << live.wheelRPM[i];
            wheelSlip << live.wheelSlip[i];
            suspension << live.suspension[i];
            contact << (bool)live.contact[i];
        }
        telemetry["wheelRPM"] = wheelRPM;
        telemetry["wheelSlip"] = wheelSlip;
        telemetry["suspension"] = suspension;
        telemetry["contact"] = contact;
        map["live"] = telemetry;
    }
    return map;
}

void PhysicalEntitySimulation::queueEraseDomainEntity(const QUuid& id) const {
    if (_entityPacketSender) {
        _entityPacketSender->queueEraseEntityMessage(id);
    }
}

// virtual
void PhysicalEntitySimulation::prepareEntityForDelete(EntityItemPointer entity) {
    // DANGER! this can be called on any thread
    // do no dirty deeds here --> assemble list for later
    assert(entity);
    assert(entity->isDead());
    QMutexLocker lock(&_mutex);
    _entitiesToDeleteLater.push_back(entity);
}

void PhysicalEntitySimulation::removeDeadEntities() {
    // DANGER! only ever call this on the main thread
    QMutexLocker lock(&_mutex);
    for (auto& entity : _entitiesToDeleteLater) {
        entity->clearActions(getThisPointer());
        EntitySimulation::prepareEntityForDelete(entity);
    }
    _entitiesToDeleteLater.clear();
}
// end EntitySimulation overrides

void PhysicalEntitySimulation::buildMotionStatesForEntitiesThatNeedThem() {
    // this lambda for when we decide to actually build the motionState
    auto buildMotionState = [&](const JPH::Shape* shape, EntityItemPointer entity) {
        EntityMotionState* motionState = new EntityMotionState(shape, entity);
        entity->setPhysicsInfo(static_cast<void*>(motionState));
        motionState->setRegion(regionOf(entity));
        motionState->setTrackSimulation(_trackSimulation.get());
        motionState->setVehicleSimulation(_vehicleSimulation.get());
        if (entity->isVehicle()) {
            _vehicleSimulation->attach(motionState);
        }
        _physicalObjects.insert(motionState);
        _incomingChanges.insert(motionState);
    };

    uint32_t deliveryCount = ObjectMotionState::getShapeManager()->getWorkDeliveryCount();
    if (deliveryCount != _lastWorkDeliveryCount) {
        // new off-thread shapes have arrived --> find adds whose shapes have arrived
        _lastWorkDeliveryCount = deliveryCount;
        ShapeRequests::iterator requestItr = _shapeRequests.begin();
        while (requestItr != _shapeRequests.end()) {
            EntityItemPointer entity = requestItr->entity;

            EntityMotionState* motionState = static_cast<EntityMotionState*>(entity->getPhysicsInfo());
            if (!motionState) {
                // this is an ADD because motionState doesn't exist yet
                const JPH::Shape* shape = ObjectMotionState::getShapeManager()->getShapeByKey(requestItr->shapeHash);
                if (shape) {
                    // shape is ready at last!
                    // but the entity's physics desired physics status may have changed since last requested
                    if (!entity->shouldBePhysical()) {
                        requestItr = _shapeRequests.erase(requestItr);
                        continue;
                    }
                    // rebuild the ShapeInfo to verify hash because entity's desired shape may have changed
                    // TODO? is there a better way to do this?
                    ShapeInfo shapeInfo;
                    computeEntityShape(entity, shapeInfo);

                    if (shapeInfo.getType() == SHAPE_TYPE_NONE) {
                        requestItr = _shapeRequests.erase(requestItr);
                    } else if (shapeInfo.getHash() != requestItr->shapeHash) {
                        // bummer, the hashes are different and we no longer want the shape we've received
                        ObjectMotionState::getShapeManager()->releaseShape(shape);
                        // try again
                        shape = ObjectMotionState::getShapeManager()->getShape(shapeInfo);
                        if (shape) {
                            buildMotionState(shape, entity);
                            requestItr = _shapeRequests.erase(requestItr);
                        } else {
                            requestItr->shapeHash = shapeInfo.getHash();
                            ++requestItr;
                        }
                    } else {
                        buildMotionState(shape, entity);
                        requestItr = _shapeRequests.erase(requestItr);
                    }
                } else {
                    // shape not ready
                    ++requestItr;
                }
            } else {
                // this is a CHANGE because motionState already exists
                if (ObjectMotionState::getShapeManager()->hasShapeWithKey(requestItr->shapeHash)) {
                    entity->markDirtyFlags(Simulation::DIRTY_SHAPE);
                    _incomingChanges.insert(motionState);
                    requestItr = _shapeRequests.erase(requestItr);
                } else {
                    // shape not ready
                    ++requestItr;
                }
            }
        }
    }

    SetOfEntities::iterator entityItr = _entitiesToAddToPhysics.begin();
    while (entityItr != _entitiesToAddToPhysics.end()) {
        EntityItemPointer entity = (*entityItr);
        if (entity->isDead()) {
            prepareEntityForDelete(entity);
            entityItr = _entitiesToAddToPhysics.erase(entityItr);
            continue;
        }
        if (entity->getPhysicsInfo()) {
            entityItr = _entitiesToAddToPhysics.erase(entityItr);
            continue;
        }
        if (!entity->shouldBePhysical()) {
            // this entity should no longer be on _entitiesToAddToPhysics
            if (entity->isMovingRelativeToParent()) {
                SetOfEntities::iterator itr = _simpleKinematicEntities.find(entity);
                if (itr == _simpleKinematicEntities.end()) {
                    _simpleKinematicEntities.insert(entity);
                }
            }
            entityItr = _entitiesToAddToPhysics.erase(entityItr);
            continue;
        }

        uint8_t region = regionOf(entity);
        if (region == workload::Region::UNKNOWN) {
            // the workload hasn't categorized it yet --> skip for later
            ++entityItr;
            continue;
        }
        if (region > workload::Region::R2) {
            // not in physical zone --> remove from list
            entityItr = _entitiesToAddToPhysics.erase(entityItr);
            continue;
        }
        if (Physics::isServerAuthority() && !entity->getParentID().isNull() && !serverDrivesAsBox(entity)) {
            // the server's tree has no ancestors: a child whose parent is not here (the rail of a deck the server does
            // not hold, a part of a model) would stand at its local coordinates near the origin; it gets no body
            bool parentHere = false;
            entity->getParentPointer(parentHere);
            if (!parentHere) {
                entityItr = _entitiesToAddToPhysics.erase(entityItr);
                continue;
            }
        }

        if (entity->isReadyToComputeShape() || serverDrivesAsBox(entity)) {
            ShapeRequest shapeRequest(entity);
            ShapeRequests::iterator  requestItr = _shapeRequests.find(shapeRequest);
            if (requestItr == _shapeRequests.end()) {
                // not waiting for a shape (yet)
                ShapeInfo shapeInfo;
                computeEntityShape(entity, shapeInfo);
                uint32_t requestCount = ObjectMotionState::getShapeManager()->getWorkRequestCount();
                const JPH::Shape* shape = ObjectMotionState::getShapeManager()->getShape(shapeInfo);
                if (shape) {
                    buildMotionState(shape, entity);
                } else if (requestCount != ObjectMotionState::getShapeManager()->getWorkRequestCount()) {
                    // shape doesn't exist but a new worker has been spawned to build it --> add to shapeRequests and wait
                    shapeRequest.shapeHash = shapeInfo.getHash();
                    _shapeRequests.insert(shapeRequest);
                } else {
                    // failed to build shape --> will not be added
                }
            }
            entityItr = _entitiesToAddToPhysics.erase(entityItr);
        } else {
            // skip for later
            ++entityItr;
        }
    }
}

void PhysicalEntitySimulation::buildPhysicsTransaction(PhysicsEngine::Transaction& transaction) {
    QMutexLocker lock(&_mutex);
    // entities being removed
    for (auto entity : _entitiesToRemoveFromPhysics) {
        EntityMotionState* motionState = static_cast<EntityMotionState*>(entity->getPhysicsInfo());
        if (motionState) {
            transaction.objectsToRemove.push_back(motionState);
            _incomingChanges.remove(motionState);
        }
        if (_shapeRequests.size() > 0) {
            ShapeRequest shapeRequest(entity);
            ShapeRequests::iterator  requestItr = _shapeRequests.find(shapeRequest);
            if (requestItr != _shapeRequests.end()) {
                _shapeRequests.erase(requestItr);
            }
        }
    }
    _entitiesToRemoveFromPhysics.clear();

    // entities to add
    buildMotionStatesForEntitiesThatNeedThem();

    // motionStates with changed entities: delete, add, or change
    for (auto& object : _incomingChanges) {
        uint32_t unhandledFlags = object->getIncomingDirtyFlags();
        // an entity made a vehicle, or unmade (PHYSICS.md round 6)
        if (object->_entity->isVehicle()) {
            _vehicleSimulation->attach(object);
        } else if (_vehicleSimulation->has(object)) {
            _vehicleSimulation->detach(object);
        }

        uint32_t handledFlags = EASY_DIRTY_PHYSICS_FLAGS;
        bool isInPhysicsSimulation = object->isInPhysicsSimulation();
        bool shouldBeInPhysicsSimulation = object->shouldBeInPhysicsSimulation();
        if (!shouldBeInPhysicsSimulation && isInPhysicsSimulation) {
            transaction.objectsToRemove.push_back(object);
            continue;
        }

        bool needsNewShape = object->needsNewShape();
        if (needsNewShape) {
            ShapeType shapeType = object->getShapeType();
            if (shapeType == SHAPE_TYPE_STATIC_MESH) {
                ShapeRequest shapeRequest(object->_entity);
                ShapeRequests::iterator  requestItr = _shapeRequests.find(shapeRequest);
                if (requestItr == _shapeRequests.end()) {
                    ShapeInfo shapeInfo;
                    computeEntityShape(object->_entity, shapeInfo);
                    uint32_t requestCount = ObjectMotionState::getShapeManager()->getWorkRequestCount();
                    const JPH::Shape* shape = ObjectMotionState::getShapeManager()->getShape(shapeInfo);
                    if (shape) {
                        object->setShape(shape);
                        handledFlags |= Simulation::DIRTY_SHAPE;
                        needsNewShape = false;
                    } else if (requestCount != ObjectMotionState::getShapeManager()->getWorkRequestCount()) {
                        // shape doesn't exist but a new worker has been spawned to build it --> add to shapeRequests and wait
                        shapeRequest.shapeHash = shapeInfo.getHash();
                        _shapeRequests.insert(shapeRequest);
                    } else {
                        // failed to build shape --> will not be added/updated
                        handledFlags |= Simulation::DIRTY_SHAPE;
                    }
                } else {
                    // continue waiting for shape request
                }
            } else {
                ShapeInfo shapeInfo;
                computeEntityShape(object->_entity, shapeInfo);
                const JPH::Shape* shape = ObjectMotionState::getShapeManager()->getShape(shapeInfo);
                if (shape) {
                    object->setShape(shape);
                    handledFlags |= Simulation::DIRTY_SHAPE;
                    needsNewShape = false;
                } else {
                    // failed to build shape --> will not be added
                }
            }
        }
        if (!isInPhysicsSimulation) {
            if (needsNewShape) {
                // skip it
                continue;
            } else {
                transaction.objectsToAdd.push_back(object);
                handledFlags = DIRTY_PHYSICS_FLAGS;
                unhandledFlags = 0;
            }
        }

        if (unhandledFlags & EASY_DIRTY_PHYSICS_FLAGS) {
            object->handleEasyChanges(unhandledFlags);
        }
        bool changedFrame = object->getWorld() && object->getWorld()->getFrameID() != object->getFrameID();
        if (changedFrame || (unhandledFlags & (Simulation::DIRTY_MOTION_TYPE | Simulation::DIRTY_COLLISION_GROUP | (handledFlags & Simulation::DIRTY_SHAPE)))) {
            // a new body: different type, shape, collision filter, or (reparented) a different frame's world
            transaction.objectsToReinsert.push_back(object);
            handledFlags |= HARD_DIRTY_PHYSICS_FLAGS;
        } else if (unhandledFlags & Simulation::DIRTY_PHYSICS_ACTIVATION && object->getBody() && object->getBody()->IsStatic()) {
            transaction.activeStaticObjects.push_back(object);
        }
        object->clearIncomingDirtyFlags(handledFlags);
    }
    _incomingChanges.clear();
}

// A ball rolling off a trolley's edge on a ship's deck (the trolley a frame on the ship, a frame) has nothing under it
// in the trolley's world: it belongs to the ship now, where the deck is. The frame that holds the entity's position
// wins (the deepest of them, PhysicsEngine::frameContaining), and the entity leaves a frame at its volume's face, with
// no margin, so a ball dropping off a floor's edge lands on the floor below at the right height. Only the owner moves
// an entity between frames (serverless: everyone is); the move travels as the parent and the local pose in its next
// update. A frame's own hull, a car on a track and anything static or kinematic stay where their parent puts them.
void PhysicalEntitySimulation::updateFrameMembership(const VectorOfMotionStates& motionStates) {
    if (!_physicsEngine) {
        return;
    }
    bool serverless = getEntityTree() && getEntityTree()->isServerlessMode();
    for (auto stateItr : motionStates) {
        ObjectMotionState* state = &(*stateItr);
        if (!state || state->getType() != MOTIONSTATE_TYPE_ENTITY || !state->getBody() || !state->getBody()->IsDynamic()) {
            continue;
        }
        EntityMotionState* entityState = static_cast<EntityMotionState*>(state);
        EntityItemPointer entity = entityState->getEntity();
        if (!entity || entity->isFrame() || entityState->isTrackDriven() || !entity->getTrackID().isNull()
                || !(serverless || entityState->isLocallyOwned())) {
            continue;
        }
        QUuid currentFrame = entityState->getFrameID();
        if (currentFrame.isNull() && !entity->getParentID().isNull()) {
            continue;   // a child of something that is not a frame (a hand, a seat): a script's business
        }
        glm::vec3 worldPosition = entity->getWorldPosition();
        QUuid targetFrame = _physicsEngine->frameContaining(worldPosition, currentFrame, false);
        if (targetFrame == currentFrame) {
            continue;
        }
        // the entity's motion in the domain, from the frame it is leaving
        glm::quat worldRotation = entity->getWorldOrientation();
        glm::vec3 worldVelocity = entity->getWorldVelocity();
        glm::vec3 worldAngularVelocity = entity->getWorldAngularVelocity();
        glm::vec3 framePosition, frameLinearVelocity, frameAngularVelocity;
        glm::quat frameRotation;
        if (!currentFrame.isNull() &&
                _physicsEngine->getFrameTransform(currentFrame, framePosition, frameRotation, frameLinearVelocity, frameAngularVelocity)) {
            worldVelocity = frameLinearVelocity + glm::cross(frameAngularVelocity, worldPosition - framePosition) +
                frameRotation * entity->getLocalVelocity();
            worldAngularVelocity = frameAngularVelocity + frameRotation * entity->getLocalAngularVelocity();
        }
        Transform local = entity->getLocalTransform();
        if (targetFrame.isNull()) {
            entity->setParentID(QUuid());
            local.setTranslation(worldPosition);
            local.setRotation(worldRotation);
            entity->setLocalTransformAndVelocities(local, worldVelocity, worldAngularVelocity);
        } else {
            if (!_physicsEngine->getFrameTransform(targetFrame, framePosition, frameRotation, frameLinearVelocity, frameAngularVelocity)) {
                continue;
            }
            glm::quat toFrame = glm::inverse(frameRotation);
            glm::vec3 offset = worldPosition - framePosition;
            entity->setParentID(targetFrame);
            local.setTranslation(toFrame * offset);
            local.setRotation(glm::normalize(toFrame * worldRotation));
            entity->setLocalTransformAndVelocities(local, toFrame * (worldVelocity - frameLinearVelocity - glm::cross(frameAngularVelocity, offset)),
                                                   toFrame * (worldAngularVelocity - frameAngularVelocity));
        }
        qCDebug(physics) << "entity" << entity->getName() << entity->getID() << "crossed from frame" << currentFrame << "to" << targetFrame;
        // the body goes to the other world with the next transaction (buildPhysicsTransaction: the frame changed)
        entity->markDirtyFlags(Simulation::DIRTY_TRANSFORM | Simulation::DIRTY_VELOCITIES);
        _incomingChanges.insert(entityState);
        _entitiesToSort.insert(entity);
        entityState->setParentChanged();
    }
}

void PhysicalEntitySimulation::handleProcessedPhysicsTransaction(PhysicsEngine::Transaction& transaction) {
    // things on objectsToRemove are ready for delete
    for (auto object : transaction.objectsToRemove) {
        EntityMotionState* entityState = static_cast<EntityMotionState*>(object);
        _vehicleSimulation->detach(entityState);
        removeOwnershipData(entityState);
        _physicalObjects.remove(object);
        delete object;
    }
    transaction.clear();
}

void PhysicalEntitySimulation::handleDeactivatedMotionStates(const VectorOfMotionStates& motionStates) {
    bool serverlessMode = getEntityTree()->isServerlessMode();
    for (auto stateItr : motionStates) {
        ObjectMotionState* state = &(*stateItr);
        assert(state);
        if (state->getType() == MOTIONSTATE_TYPE_ENTITY) {
            EntityMotionState* entityState = static_cast<EntityMotionState*>(state);
            EntityItemPointer entity = entityState->getEntity();
            _entitiesToSort.insert(entity);
            if (!serverlessMode) {
                if (entity->isAvatarEntity()) {
                    switch (entityState->getOwnershipState()) {
                        case EntityMotionState::OwnershipState::PendingBid:
                            _bids.removeFirst(entityState);
                            entityState->clearOwnershipState();
                            break;
                        case EntityMotionState::OwnershipState::LocallyOwned:
                            _owned.removeFirst(entityState);
                            entityState->clearOwnershipState();
                            break;
                        default:
                            break;
                    }
                } else {
                    entityState->handleDeactivation();
                }
            }
        }
    }
}

void PhysicalEntitySimulation::handleChangedMotionStates(const VectorOfMotionStates& motionStates) {
    PROFILE_RANGE_EX(simulation_physics, "ChangedEntities", 0x00000000, (uint64_t)motionStates.size());
    QMutexLocker lock(&_mutex);

    for (auto stateItr : motionStates) {
        ObjectMotionState* state = &(*stateItr);
        assert(state);
        if (state->getType() == MOTIONSTATE_TYPE_ENTITY) {
            EntityMotionState* entityState = static_cast<EntityMotionState*>(state);
            _entitiesToSort.insert(entityState->getEntity());
            if (entityState->getOwnershipState() == EntityMotionState::OwnershipState::NotLocallyOwned) {
                // NOTE: entityState->getOwnershipState() reflects what ownership list (_bids or _owned) it is in
                // and is distinct from entityState->isLocallyOwned() which checks the simulation ownership
                // properties of the corresponding EntityItem.  It is possible for the two states to be out
                // of sync.  In fact, we're trying to put them back into sync here.
                if (entityState->isLocallyOwned()) {
                    addOwnership(entityState);
                } else if (entityState->shouldSendBid()) {
                    addOwnershipBid(entityState);
                } else {
                    entityState->getEntity()->updateQueryAACube();
                }
            }
        }
    }

    // A car on a track needs a driver even while it stands still (a train at a station is not an active body, so it
    // is not among the changed states above): whoever is near and finds it unowned bids for it (PHYSICS.md 4.8).
    // Likewise a vehicle the physics server holds while it sleeps (a car parked on its pad): it goes on the owned list,
    // whose keep-alive holds it, rather than waiting to move (round 7).
    for (ObjectMotionState* state : _physicalObjects) {
        if (state->getType() == MOTIONSTATE_TYPE_ENTITY) {
            EntityMotionState* entityState = static_cast<EntityMotionState*>(state);
            if (!entityState->getBody() || entityState->getOwnershipState() != EntityMotionState::OwnershipState::NotLocallyOwned) {
                continue;
            }
            if (entityState->isTrackDriven() && !entityState->isLocallyOwned() && entityState->shouldSendBid()) {
                addOwnershipBid(entityState);
            } else if (entityState->serverTakes() && entityState->isLocallyOwned()) {
                addOwnership(entityState);
            }
        }
    }

    updateFrameMembership(motionStates);

    // the vehicles' wheels follow the wheels, and a driver's inputs go to the physics server (PHYSICS.md round 6)
    _vehicleSimulation->setServerless(getEntityTree()->isServerlessMode());
    _vehicleSimulation->updateVisuals();

    uint32_t numSubsteps = _physicsEngine->getNumSubsteps();
    if (_lastStepSendPackets != numSubsteps) {
        _lastStepSendPackets = numSubsteps;
        _vehicleSimulation->sendInputs();
        _vehicleSimulation->sendStream();           // the physics server's states to its subscribers (round 9)
        _vehicleSimulation->sendStateRequests();    // a client's subscription

        if (Physics::getSessionUUID().isNull()) {
            // usually don't get here, but if so clear all ownership
            clearOwnershipData();
        }
        // send updates before bids, because this simplifies the logic thasuccessful bids will immediately send an update when added to the 'owned' list
        sendOwnedUpdates(numSubsteps);
        sendOwnershipBids(numSubsteps);
    }
}

void PhysicalEntitySimulation::addOwnershipBid(EntityMotionState* motionState) {
    if (getEntityTree()->isServerlessMode()) {
        return;
    }
    motionState->initForBid();
    motionState->sendBid(_entityPacketSender, _physicsEngine->getNumSubsteps());
    _bids.push_back(motionState);
    _nextBidExpiry = glm::min(_nextBidExpiry, motionState->getNextBidExpiry());
}

void PhysicalEntitySimulation::addOwnership(EntityMotionState* motionState) {
    if (getEntityTree()->isServerlessMode()) {
        return;
    }
    motionState->initForOwned();
    _owned.push_back(motionState);
}

void PhysicalEntitySimulation::sendOwnershipBids(uint32_t numSubsteps) {
    uint64_t now = usecTimestampNow();
    if (now > _nextBidExpiry) {
        PROFILE_RANGE_EX(simulation_physics, "Bid", 0x00000000, (uint64_t)_bids.size());
        _nextBidExpiry = std::numeric_limits<uint64_t>::max();
        uint32_t i = 0;
        while (i < _bids.size()) {
            bool removeBid = false;
            if (_bids[i]->isLocallyOwned()) {
                // when an object transitions from 'bid' to 'owned' we are changing the "mode" of data stored
                // in the EntityMotionState::_serverFoo variables (please see comments in EntityMotionState.h)
                // therefore we need to immediately send an update so that the values stored are what we're
                // "telling" the server rather than what we've been "hearing" from the server.
                _bids[i]->workerBidPriority();
                _bids[i]->sendUpdate(_entityPacketSender, numSubsteps);

                addOwnership(_bids[i]);
                removeBid = true;
            } else if (!_bids[i]->shouldSendBid()) {
                removeBid = true;
                _bids[i]->clearOwnershipState();
            }
            if (removeBid) {
                _bids.remove(i);
            } else {
                if (now > _bids[i]->getNextBidExpiry()) {
                    _bids[i]->sendBid(_entityPacketSender, numSubsteps);
                }
                // every bid kept is looked at again by its expiry, resent or not: one whose expiry was pushed back (an
                // owner's change does that) was forgotten, and when it then won, the win was never taken up, so the
                // physics server held a car it never updated and lost it ten seconds later (round 7)
                _nextBidExpiry = glm::min(_nextBidExpiry, _bids[i]->getNextBidExpiry());
                ++i;
            }
        }
    }
}

void PhysicalEntitySimulation::sendOwnedUpdates(uint32_t numSubsteps) {
    if (getEntityTree()->isServerlessMode()) {
        return;
    }
    PROFILE_RANGE_EX(simulation_physics, "Update", 0x00000000, (uint64_t)_owned.size());
    uint32_t i = 0;
    while (i < _owned.size()) {
        // if (_owned[i]->isStaticPriority()) continue;
        if (!_owned[i]->isLocallyOwned()) {
            if (_owned[i]->shouldSendBid()) {
                addOwnershipBid(_owned[i]);
            } else {
                _owned[i]->clearOwnershipState();
            }
            _owned.remove(i);
        } else {
            if (_owned[i]->shouldSendUpdate(numSubsteps)) {
                _owned[i]->sendUpdate(_entityPacketSender, numSubsteps);
            }
            ++i;
        }
    }
}

void PhysicalEntitySimulation::handleCollisionEvents(const CollisionEvents& collisionEvents) {
    for (auto collision : collisionEvents) {
        // NOTE: The collision event is always aligned such that idA is never NULL.
        // however idB may be NULL.
        if (!collision.idB.isNull()) {
            emit entityCollisionWithEntity(collision.idA, collision.idB, collision);
        }
    }
}

void PhysicalEntitySimulation::addDynamic(EntityDynamicPointer dynamic) {
    if (_physicsEngine) {
        // FIXME put fine grain locking into _physicsEngine
        {
            QMutexLocker lock(&_mutex);
            const QUuid& dynamicID = dynamic->getID();
            if (_physicsEngine->getDynamicByID(dynamicID)) {
                qCDebug(physics) << "warning -- PhysicalEntitySimulation::addDynamic -- adding an "
                    "dynamic that was already in _physicsEngine";
            }
        }
        QMutexLocker lock(&_dynamicsMutex);
        _dynamicsToAdd += dynamic;
    }
}

void PhysicalEntitySimulation::removeDynamic(const QUuid dynamicID) {
    QMutexLocker lock(&_dynamicsMutex);
    _dynamicsToRemove += dynamicID;
}

void PhysicalEntitySimulation::applyDynamicChanges() {
    QList<EntityDynamicPointer> dynamicsFailedToAdd;
    if (_physicsEngine) {
        QMutexLocker lock(&_dynamicsMutex);
        foreach(QUuid dynamicToRemove, _dynamicsToRemove) {
            _physicsEngine->removeDynamic(dynamicToRemove);
        }
        foreach (EntityDynamicPointer dynamicToAdd, _dynamicsToAdd) {
            if (!_dynamicsToRemove.contains(dynamicToAdd->getID())) {
                if (!_physicsEngine->addDynamic(dynamicToAdd)) {
                    dynamicsFailedToAdd += dynamicToAdd;
                }
            }
        }
        _dynamicsToAdd.clear();
        _dynamicsToRemove.clear();
    }

    // put back the ones that couldn't yet be added
    foreach (EntityDynamicPointer dynamicFailedToAdd, dynamicsFailedToAdd) {
        addDynamic(dynamicFailedToAdd);
    }
}
