//
//  ObjectTable.cpp
//  libraries/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ObjectTable.h"

#include <QtCore/QObject>

ObjectTable::~ObjectTable() {
    releaseOwned();
}

ObjectTable::Handle ObjectTable::handleFor(QObject* object) {
    if (!object) {
        return NULL_HANDLE;
    }
    auto found = _handles.find(object);
    if (found != _handles.end()) {
        // the same address may belong to a new object after the old one died: check the entry is still alive
        auto entry = _entries.find(found.value());
        if (entry != _entries.end() && entry.value().object == object) {
            return found.value();
        }
        if (entry != _entries.end()) {
            _entries.erase(entry);
        }
        _handles.erase(found);
    }
    Handle handle = _next++;
    _entries.insert(handle, Entry { QPointer<QObject>(object), QString::fromUtf8(object->metaObject()->className()) });
    _handles.insert(object, handle);
    return handle;
}

ObjectTable::Handle ObjectTable::adopt(QObject* object, Lingers lingers) {
    Handle handle = handleFor(object);
    auto entry = _entries.find(handle);
    if (entry != _entries.end()) {
        entry.value().owned = true;
        entry.value().lingers = std::move(lingers);
    }
    return handle;
}

bool ObjectTable::owns(Handle handle) const {
    auto entry = _entries.find(handle);
    return entry != _entries.end() && entry.value().owned;
}

QObject* ObjectTable::object(Handle handle) const {
    auto entry = _entries.find(handle);
    return entry == _entries.end() ? nullptr : entry.value().object.data();
}

QString ObjectTable::className(Handle handle) const {
    auto entry = _entries.find(handle);
    return entry == _entries.end() ? QString() : entry.value().className;
}

bool ObjectTable::contains(Handle handle) const {
    return _entries.contains(handle);
}

void ObjectTable::release(Handle handle) {
    auto entry = _entries.find(handle);
    if (entry == _entries.end()) {
        return;
    }
    if (entry.value().object) {
        QObject* object = entry.value().object.data();
        _handles.remove(object);
        if (entry.value().owned) {
            if (entry.value().lingers && entry.value().lingers(object)) {
                _lingering.append(Lingering { QPointer<QObject>(object), entry.value().lingers });
            } else {
                object->deleteLater();
            }
        }
    }
    _entries.erase(entry);
    for (auto it = _globals.begin(); it != _globals.end();) {
        if (it.value() == handle) {
            it = _globals.erase(it);
        } else {
            ++it;
        }
    }
}

void ObjectTable::releaseOwned() {
    QList<Handle> owned;
    for (auto it = _entries.constBegin(); it != _entries.constEnd(); ++it) {
        if (it.value().owned) {
            owned.append(it.key());
        }
    }
    for (Handle handle : owned) {
        auto entry = _entries.find(handle);
        if (entry == _entries.end()) {
            continue;
        }
        if (entry.value().object) {
            _handles.remove(entry.value().object.data());
            entry.value().object->deleteLater();
        }
        _entries.erase(entry);
        for (auto it = _globals.begin(); it != _globals.end();) {
            if (it.value() == handle) {
                it = _globals.erase(it);
            } else {
                ++it;
            }
        }
    }
    // and those that lingered: their work ends with the script, as a QtScript engine's end deletes what it owns
    for (const Lingering& lingering : _lingering) {
        if (lingering.object) {
            lingering.object->deleteLater();
        }
    }
    _lingering.clear();
}

int ObjectTable::sweepLingering() {
    for (auto it = _lingering.begin(); it != _lingering.end();) {
        if (!it->object) {
            it = _lingering.erase(it);
        } else if (!it->lingers(it->object.data())) {
            it->object->deleteLater();
            it = _lingering.erase(it);
        } else {
            ++it;
        }
    }
    return _lingering.size();
}

void ObjectTable::registerGlobal(const QString& name, QObject* object) {
    _globals.insert(name, handleFor(object));
}

ObjectTable::Handle ObjectTable::global(const QString& name) const {
    return _globals.value(name, NULL_HANDLE);
}

QStringList ObjectTable::globalNames() const {
    QStringList names = _globals.keys();
    names.sort();
    return names;
}
