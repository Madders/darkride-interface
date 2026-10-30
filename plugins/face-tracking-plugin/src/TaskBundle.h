//
//  TaskBundle.h
//  plugins/face-tracking-plugin/src
//
//  The files in a MediaPipe .task bundle: a zip whose entries are stored uncompressed.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QString>

class TaskBundle {
public:
    // false, with error() saying why, unless data is a zip of stored (uncompressed) entries
    bool open(const QByteArray& data);

    // a copy of the named entry (its own allocation, so aligned as TensorFlow Lite wants), empty if there is none
    QByteArray file(const QString& name) const;

    const QString& error() const { return _error; }

private:
    struct Entry {
        int offset;
        int size;
    };

    QByteArray _data;
    QHash<QString, Entry> _entries;
    QString _error;
};
