//
//  EntityListMessage.h
//  libraries/entities/src
//
//  The entity server's list of every entity in the domain, for build mode's Hierarchy (NATIVE-EDITOR.md 8):
//  a request and a reply carried by the existing MessagesData packet, sent point to point between a client and
//  the entity server rather than through the messages mixer, so no new packet type and no protocol change.
//  An older server has no listener for MessagesData and does not answer.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityListMessage_h
#define hifi_EntityListMessage_h

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QVector>

#include <AACube.h>

#include "EntityTypes.h"

namespace EntityListMessage {
    extern const QString CHANNEL;

    // one entity as the server holds it: enough for a Hierarchy row and for flying the camera to it
    struct Record {
        QUuid id;
        QUuid parentID;     // an entity, an avatar, or none
        EntityTypes::EntityType type { EntityTypes::Unknown };
        bool visible { true };
        bool locked { false };
        quint64 created { 0 };  // usecs
        AACube cube;        // the query cube: the entity with everything it draws, in world space
        QString name;
    };

    QByteArray encodeRequest(quint32 requestNumber);
    bool decodeRequest(const QByteArray& data, quint32& requestNumber);

    QByteArray encodeReply(quint32 requestNumber, const QVector<Record>& records);
    // the reply's request number alone, read before anything is unpacked
    bool readReplyNumber(const QByteArray& data, quint32& requestNumber);
    bool decodeReply(const QByteArray& data, quint32& requestNumber, QVector<Record>& records);
}

#endif // hifi_EntityListMessage_h
