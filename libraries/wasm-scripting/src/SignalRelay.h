//
//  SignalRelay.h
//  libraries/wasm-scripting/src
//
//  Subscribes to Qt signals on behalf of a module. A relay is a QObject with no slots of its own: each
//  subscription gets a slot index past QObject's, and qt_metacall turns the signal's arguments into QVariants
//  and hands them to the subscription's handler (the trick QSignalSpy and QtScript's connection manager use).
//  The connection type is automatic: a signal from an object on the relay's own thread arrives at once, one
//  from another thread is queued through the relay's event loop, so the handler always runs on the relay's
//  thread, which is the script engine's.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SignalRelay_h
#define hifi_SignalRelay_h

#include <functional>

#include <QtCore/QHash>
#include <QtCore/QMetaMethod>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVector>

class SignalRelay : public QObject {
    // No Q_OBJECT on purpose: the slots are dynamic (qt_metacall), there is nothing for moc to generate.
public:
    using Handler = std::function<void(int connection, const QVariantList& arguments)>;

    explicit SignalRelay(QObject* parent = nullptr);
    ~SignalRelay() override;

    /// Subscribes to `signal` of `sender`, given as a name ("positionChanged") or a signature
    /// ("positionChanged(glm::vec3)"); a name picks the first signal of that name. Returns the connection id,
    /// greater than zero, or zero with the error text.
    int connect(QObject* sender, const QString& signal, Handler handler, QString& error);
    /// Ends a subscription. False when the id is unknown.
    bool disconnect(int connection);
    void disconnectAll();

    int connectionCount() const { return _connections.size(); }
    bool isConnected(int connection) const { return _connections.contains(connection); }
    QObject* sender(int connection) const;
    QMetaMethod signal(int connection) const;

    int qt_metacall(QMetaObject::Call call, int id, void** arguments) override;

private:
    struct Connection {
        QPointer<QObject> sender;
        QMetaMethod signal;
        QVector<int> parameterTypes;
        Handler handler;
        int slot;  // the absolute method index this relay answers to
    };

    void deliver(const Connection& connection, void** arguments);

    QHash<int, Connection> _connections;  // by connection id
    QHash<int, int> _connectionBySlot;    // slot index (relative to QObject's method count) to connection id
    int _nextConnection { 1 };
    int _nextSlot { 0 };
};

#endif // hifi_SignalRelay_h
