//
//  PhysicsServer.h
//  assignment-client/src/physics
//
//  The physics server (PHYSICS.md 4.5, round 5): the domain's server-authoritative simulation. An assignment client with a
//  headless entity tree of the domain's Track entities and everything that rides one, the physics library's engine and
//  simulation (one Jolt world for the domain and one per frame, as a client has) stepped at 90 Hz on a timer, and no
//  renderer. It owns every car on a track at SERVER priority, drives the trains with the same follower every client
//  runs, and publishes each train's state on its Track entity through the entity server, as ordinary edits, when
//  something happens and once a second besides; clients run the state on in the same substeps and blend corrections.
//  If it dies, ownership of the cars expires as any owner's does and the clients drive them as in round 4.
//
//  Round 6 adds the driven vehicles: the server simulates every vehicle (an entity with a wheeled vehicle group) on the
//  Terrain entities and the collidable primitive shapes of the domain, applies the drivers' inputs (VehicleInput packets,
//  one input per substep) and publishes each vehicle's state in its ownership updates. Not yet: bodies for avatars,
//  model hulls on the server (a vehicle's chassis is a box here), a direct state stream to clients.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PhysicsServer_h
#define hifi_PhysicsServer_h

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <EntityEditPacketSender.h>
#include <PhysicalEntitySimulation.h>
#include <PhysicsEngine.h>
#include <ShapeManager.h>
#include <ThreadedAssignment.h>

#include "../entities/EntityTreeHeadlessViewer.h"

class PhysicsServer : public ThreadedAssignment {
    Q_OBJECT

public:
    PhysicsServer(ReceivedMessage& message);
    ~PhysicsServer();

    void aboutToFinish() override;

public slots:
    void run() override;
    void nodeActivated(SharedNodePointer activatedNode);
    void nodeKilled(SharedNodePointer killedNode);
    void sendStatsPacket() override;

private slots:
    void handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    // receiveTime: the socket's stamp on the datagram the message came in (a packet piggybacked on a stats message is
    // rebuilt without one, so the caller passes the stats message's)
    void measureStreamAge(ReceivedMessage& message, const SharedNodePointer& senderNode, qint64 receiveTime);
    void handleVehicleInput(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void step();

private:
    void publishTrainStates();
    void updateTerrains();
    QString describeEntityServerConnection();

    EntityEditPacketSender _entityEditSender;
    EntityTreeHeadlessViewer _entityViewer;
    ShapeManager _shapeManager;
    PhysicsEnginePointer _physicsEngine;
    PhysicalEntitySimulationPointer _entitySimulation;
    QTimer* _stepTimer { nullptr };
    quint64 _lastQueryTime { 0 };
    quint64 _lastTerrainTime { 0 };
    bool _running { false };

    // stats
    quint64 _stepsTaken { 0 };
    quint64 _statesPublished { 0 };
    // the entity server's stream as it arrives (the lag from the entity server to this server, PHYSICS.md round 7's
    // open end): the time in flight by the packet's own stamp, and the time it waited in this process to be read
    quint64 _streamPackets { 0 };
    qint64 _streamFlightMax { 0 };      // usec, over the log window
    qint64 _streamQueuedMax { 0 };
    qint64 _parseMaxUsec { 0 };         // the longest a packet took to read into the tree, over the log window
    qint64 _parseTotalUsec { 0 };
    quint64 _streamLateLogTime { 0 };
    quint64 _lastStatsTime { 0 };
    quint64 _lastLogTime { 0 };
    quint64 _stepsAtLastLog { 0 };
    quint64 _stepsAtLastStats { 0 };
    float _stepsPerSecond { 0.0f };
};

#endif // hifi_PhysicsServer_h
