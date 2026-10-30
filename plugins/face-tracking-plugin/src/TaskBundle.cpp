//
//  TaskBundle.cpp
//  plugins/face-tracking-plugin/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "TaskBundle.h"

#include <QtCore/QtEndian>

namespace {

const quint32 END_OF_CENTRAL_DIRECTORY = 0x06054b50;
const quint32 CENTRAL_DIRECTORY_ENTRY = 0x02014b50;
const quint32 LOCAL_HEADER = 0x04034b50;
const int END_OF_CENTRAL_DIRECTORY_SIZE = 22;
const int CENTRAL_DIRECTORY_ENTRY_SIZE = 46;
const int LOCAL_HEADER_SIZE = 30;
const int MAX_COMMENT = 0xffff;

quint16 u16(const QByteArray& data, int at) {
    return qFromLittleEndian<quint16>(data.constData() + at);
}

quint32 u32(const QByteArray& data, int at) {
    return qFromLittleEndian<quint32>(data.constData() + at);
}

}  // namespace

bool TaskBundle::open(const QByteArray& data) {
    _data = data;
    _entries.clear();
    _error.clear();
    const int size = data.size();

    // the end of the central directory: the last record, before a comment of up to 64 KB
    int end = -1;
    for (int at = size - END_OF_CENTRAL_DIRECTORY_SIZE; at >= 0 && at >= size - END_OF_CENTRAL_DIRECTORY_SIZE - MAX_COMMENT;
         at--) {
        if (u32(data, at) == END_OF_CENTRAL_DIRECTORY) {
            end = at;
            break;
        }
    }
    if (end < 0) {
        _error = "not a zip";
        return false;
    }
    const int count = u16(data, end + 10);
    int at = (int)u32(data, end + 16);
    for (int i = 0; i < count; i++) {
        if (at < 0 || at + CENTRAL_DIRECTORY_ENTRY_SIZE > size || u32(data, at) != CENTRAL_DIRECTORY_ENTRY) {
            _error = "a damaged zip directory";
            return false;
        }
        const int method = u16(data, at + 10);
        const qint64 compressedSize = u32(data, at + 20);
        const qint64 uncompressedSize = u32(data, at + 24);
        const int nameLength = u16(data, at + 28);
        const int extraLength = u16(data, at + 30);
        const int commentLength = u16(data, at + 32);
        const qint64 localHeader = u32(data, at + 42);
        if (at + CENTRAL_DIRECTORY_ENTRY_SIZE + nameLength > size) {
            _error = "a damaged zip directory";
            return false;
        }
        const QString name = QString::fromUtf8(data.constData() + at + CENTRAL_DIRECTORY_ENTRY_SIZE, nameLength);
        at += CENTRAL_DIRECTORY_ENTRY_SIZE + nameLength + extraLength + commentLength;

        if (method != 0 || compressedSize != uncompressedSize) {
            _error = QString("%1 is compressed").arg(name);
            return false;
        }
        if (localHeader + LOCAL_HEADER_SIZE > size || u32(data, (int)localHeader) != LOCAL_HEADER) {
            _error = QString("%1 has no file header").arg(name);
            return false;
        }
        const qint64 offset = localHeader + LOCAL_HEADER_SIZE + u16(data, (int)localHeader + 26) +
                              u16(data, (int)localHeader + 28);
        if (offset + compressedSize > size) {
            _error = QString("%1 runs past the end").arg(name);
            return false;
        }
        _entries.insert(name, { (int)offset, (int)compressedSize });
    }
    return true;
}

QByteArray TaskBundle::file(const QString& name) const {
    auto it = _entries.find(name);
    if (it == _entries.end()) {
        return QByteArray();
    }
    return QByteArray(_data.constData() + it->offset, it->size);
}
