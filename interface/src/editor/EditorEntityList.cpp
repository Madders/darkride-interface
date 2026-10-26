//
//  EditorEntityList.cpp
//  interface/src/editor
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EditorEntityList.h"

#include <DependencyManager.h>
#include <MessagesClient.h>
#include <NodeList.h>

namespace {
    const int REPLY_TIMEOUT_MS = 10000;
}

EditorEntityList::EditorEntityList(QObject* parent) : QObject(parent) {
    _timeout.setSingleShot(true);
    _timeout.setInterval(REPLY_TIMEOUT_MS);
    connect(&_timeout, &QTimer::timeout, this, [this] {
        qDebug() << "The entity server did not send its entity list";
        setState(State::Unanswered);
    });
    // MessagesClient takes every MessagesData packet, whoever sent it, the entity server's replies among them
    connect(DependencyManager::get<MessagesClient>().data(), &MessagesClient::dataReceived,
            this, &EditorEntityList::onData, Qt::QueuedConnection);
}

void EditorEntityList::request() {
    auto nodeList = DependencyManager::get<NodeList>();
    SharedNodePointer server = nodeList->soloNodeOfType(NodeType::EntityServer);
    if (!server || !server->getActiveSocket()) {
        _timeout.stop();
        setState(State::Unanswered);
        return;
    }
    _serverID = server->getUUID();
    QByteArray data = EntityListMessage::encodeRequest(++_requestNumber);
    nodeList->sendPacketList(MessagesClient::encodeMessagesDataPacket(EntityListMessage::CHANNEL, data,
                                                                      nodeList->getSessionUUID()), *server);
    _timeout.start();
    setState(State::Waiting);
}

void EditorEntityList::onData(QString channel, QByteArray data, QUuid senderID, bool localOnly) {
    if (localOnly || channel != EntityListMessage::CHANNEL || senderID != _serverID) {
        return;
    }
    quint32 number = 0;
    QVector<EntityListMessage::Record> records;
    if (!EntityListMessage::decodeReply(data, number, records)) {
        return;
    }
    // an answer to an older request is superseded by the one on its way
    if (number != _requestNumber) {
        return;
    }
    _timeout.stop();
    setState(State::Answered);
    emit received(records);
}

void EditorEntityList::setState(State state) {
    if (state != _state) {
        _state = state;
        emit stateChanged();
    }
}
