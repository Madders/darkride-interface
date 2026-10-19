//
//  OctreeHeadlessViewer.cpp
//  libraries/octree/src
//
//  Created by Brad Hefta-Gaub on 2/26/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "OctreeHeadlessViewer.h"

#include <NodeList.h>
#include <OctreeLogging.h>

void OctreeHeadlessViewer::queryOctree() {
    char serverType = getMyNodeType();
    PacketType packetType = getMyQueryMessageType();

    if (_hasViewFrustum) {
        ConicalViewFrustums views { _viewFrustum };
        _octreeQuery.setConicalViews(views);
    } else {
        _octreeQuery.clearConicalViews();
    }

    auto nodeList = DependencyManager::get<NodeList>();

    auto node = nodeList->soloNodeOfType(serverType);
    if (node && node->getActiveSocket()) {
        if (node->getUUID() != _queriedServer) {
            // a new entity server: ask it to report its first full scene, after which its stream turns unreliable
            _queriedServer = node->getUUID();
            _initialSceneComplete = false;
            _firstQueryTime = usecTimestampNow();
            _octreeQuery.setReportInitialCompletion(true);
        }
        auto queryPacket = NLPacket::create(packetType);

        // encode the query data
        auto packetData = reinterpret_cast<unsigned char*>(queryPacket->getPayload());
        int packetSize = _octreeQuery.getBroadcastData(packetData);
        queryPacket->setPayloadSize(packetSize);

        // make sure we still have an active socket
        nodeList->sendUnreliablePacket(*queryPacket, *node);
    }
}


void OctreeHeadlessViewer::handleInitialResultsComplete(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode) {
    if (_initialSceneComplete || !senderNode || senderNode->getUUID() != _queriedServer) {
        return;
    }
    _initialSceneComplete = true;
    _octreeQuery.setReportInitialCompletion(false);
    qCDebug(octree) << "entity server" << senderNode->getUUID() << "reported its first full scene after"
        << (usecTimestampNow() - _firstQueryTime) / USECS_PER_MSEC << "ms:" << (_tree ? _tree->getOctreeElementsCount() : 0)
        << "tree elements; its stream is unreliable from here";

}

int OctreeHeadlessViewer::parseOctreeStats(QSharedPointer<ReceivedMessage> message, SharedNodePointer sourceNode) {

    OctreeSceneStats temp;
    int statsMessageLength = temp.unpackFromPacket(*message);

    // TODO: actually do something with these stats, like expose them to JS...

    return statsMessageLength;
}

void OctreeHeadlessViewer::trackIncomingOctreePacket(const QByteArray& packet,
                                const SharedNodePointer& sendingNode, bool wasStatsPacket) {

}
