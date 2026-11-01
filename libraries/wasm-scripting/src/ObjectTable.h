//
//  ObjectTable.h
//  libraries/wasm-scripting/src
//
//  The objects a script engine has handed to a module, by handle. A handle is a small integer the guest keeps;
//  the host keeps the QObject behind it with a QPointer, so an object deleted on the host side becomes a dead
//  handle (object() returns null) rather than a dangling pointer. The registered globals (Entities, MyAvatar,
//  Script...) are the named entries; every other entry is an object a method returned or a signal carried.
//
//  An entry may own its object: one made for the script alone (an audio injector, a sound, an avatar's data
//  that a method returns new each call), which the QtScript engine gives the script with ScriptOwnership.
//  An owned object is deleted (deleteLater) when its handle is released or the owned entries are released;
//  one that still has work to do when the script lets it go (an injector whose sound plays) may linger: the
//  engine sweeps the lingering ones now and then, deleting those whose work is done, and the rest go with the
//  owned entries.
//
//  Used from one thread, the engine's.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ObjectTable_h
#define hifi_ObjectTable_h

#include <cstdint>
#include <functional>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

class ObjectTable {
public:
    using Handle = int32_t;
    static constexpr Handle NULL_HANDLE = 0;

    ObjectTable() = default;
    ObjectTable(const ObjectTable&) = delete;
    ObjectTable& operator=(const ObjectTable&) = delete;
    ~ObjectTable();

    /// The handle of an object, allocated on first sight; NULL_HANDLE for null.
    Handle handleFor(QObject* object);
    /// Whether an owned object the script releases stays for now (asked again at each sweep).
    using Lingers = std::function<bool(QObject* object)>;
    /// The handle of an object the table is to own from now on (see above).
    Handle adopt(QObject* object, Lingers lingers = Lingers());
    bool owns(Handle handle) const;
    /// The object behind a handle; null when the handle is unknown, released, or the object was deleted.
    QObject* object(Handle handle) const;
    /// The class name the object had when it was entered (readable after the object is gone).
    QString className(Handle handle) const;
    bool contains(Handle handle) const;
    /// Forgets a handle. The object itself is not touched, unless the table owns it.
    void release(Handle handle);
    /// Deletes the objects the table owns and forgets their handles (the engine does this as its script stops,
    /// on its own thread).
    void releaseOwned();
    /// Deletes the lingering objects whose work is done; returns how many still linger.
    int sweepLingering();

    /// A named global, as registerGlobalObject makes one. The name is bound to the object's handle.
    void registerGlobal(const QString& name, QObject* object);
    Handle global(const QString& name) const;
    QStringList globalNames() const;
    /// Whether the handle is a registered global's (a module may not release those).
    bool isGlobal(Handle handle) const;

    int size() const { return _entries.size(); }

private:
    struct Entry {
        QPointer<QObject> object;
        QString className;
        bool owned { false };
        Lingers lingers;
    };
    QHash<Handle, Entry> _entries;
    QHash<QObject*, Handle> _handles;
    QHash<QString, Handle> _globals;
    struct Lingering {
        QPointer<QObject> object;
        Lingers lingers;
    };
    QList<Lingering> _lingering;  // owned objects released while they had work to do
    Handle _next { 1 };
};

#endif // hifi_ObjectTable_h
