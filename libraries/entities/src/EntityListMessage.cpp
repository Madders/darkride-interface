//
//  EntityListMessage.cpp
//  libraries/entities/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityListMessage.h"

#include <QtCore/QDataStream>

namespace EntityListMessage {
    const QString CHANNEL = QStringLiteral("darkride.entityList");

    namespace {
        const quint8 FORMAT_VERSION = 1;
        const quint8 KIND_REQUEST = 1;
        const quint8 KIND_REPLY = 2;
        const quint32 MAX_RECORDS = 1000000;
        const int MAX_NAME_BYTES = 65535;

        void setUp(QDataStream& stream) {
            stream.setByteOrder(QDataStream::LittleEndian);
            stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
        }

        // the header both kinds share; false when the data is not ours or is of another version
        bool readHeader(QDataStream& stream, quint8 expectedKind, quint32& requestNumber) {
            quint8 version = 0;
            quint8 kind = 0;
            stream >> version >> kind >> requestNumber;
            return stream.status() == QDataStream::Ok && version == FORMAT_VERSION && kind == expectedKind;
        }
    }

    QByteArray encodeRequest(quint32 requestNumber) {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        setUp(stream);
        stream << FORMAT_VERSION << KIND_REQUEST << requestNumber;
        return data;
    }

    bool decodeRequest(const QByteArray& data, quint32& requestNumber) {
        QDataStream stream(data);
        setUp(stream);
        return readHeader(stream, KIND_REQUEST, requestNumber);
    }

    QByteArray encodeReply(quint32 requestNumber, const QVector<Record>& records) {
        QByteArray body;
        {
            QDataStream stream(&body, QIODevice::WriteOnly);
            setUp(stream);
            stream << (quint32)records.size();
            for (const auto& record : records) {
                QByteArray name = record.name.toUtf8().left(MAX_NAME_BYTES);
                quint8 flags = (record.visible ? 1 : 0) | (record.locked ? 2 : 0);
                const glm::vec3& corner = record.cube.getCorner();
                stream.writeRawData(record.id.toRfc4122().constData(), 16);
                stream.writeRawData(record.parentID.toRfc4122().constData(), 16);
                stream << (quint8)record.type << flags << record.created
                       << corner.x << corner.y << corner.z << record.cube.getScale()
                       << (quint16)name.size();
                stream.writeRawData(name.constData(), name.size());
            }
        }
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        setUp(stream);
        stream << FORMAT_VERSION << KIND_REPLY << requestNumber;
        // ids don't compress; names, types and flags do
        QByteArray compressed = qCompress(body);
        stream.writeRawData(compressed.constData(), compressed.size());
        return data;
    }

    bool decodeReply(const QByteArray& data, quint32& requestNumber, QVector<Record>& records) {
        records.clear();
        QDataStream header(data);
        setUp(header);
        if (!readHeader(header, KIND_REPLY, requestNumber)) {
            return false;
        }
        const int HEADER_BYTES = 6;
        QByteArray body = qUncompress(data.mid(HEADER_BYTES));
        if (body.isEmpty()) {
            return false;
        }
        QDataStream stream(body);
        setUp(stream);
        quint32 count = 0;
        stream >> count;
        if (stream.status() != QDataStream::Ok || count > MAX_RECORDS) {
            return false;
        }
        records.reserve((int)count);
        char id[16];
        char parentID[16];
        for (quint32 i = 0; i < count; ++i) {
            Record record;
            quint8 type = 0;
            quint8 flags = 0;
            float x = 0.0f, y = 0.0f, z = 0.0f, scale = 0.0f;
            quint16 nameBytes = 0;
            if (stream.readRawData(id, 16) != 16 || stream.readRawData(parentID, 16) != 16) {
                return false;
            }
            stream >> type >> flags >> record.created >> x >> y >> z >> scale >> nameBytes;
            QByteArray name(nameBytes, Qt::Uninitialized);
            if (stream.status() != QDataStream::Ok || stream.readRawData(name.data(), nameBytes) != nameBytes) {
                return false;
            }
            record.id = QUuid::fromRfc4122(QByteArray(id, 16));
            record.parentID = QUuid::fromRfc4122(QByteArray(parentID, 16));
            record.type = (EntityTypes::EntityType)type;
            record.visible = (flags & 1) != 0;
            record.locked = (flags & 2) != 0;
            record.cube = AACube(glm::vec3(x, y, z), scale);
            record.name = QString::fromUtf8(name);
            records << record;
        }
        return true;
    }
}
