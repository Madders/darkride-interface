//
//  SingleInstance.cpp
//  interface/src
//
//  See SingleInstance.h. The socket's name is the user's (getInterfaceSharedMemoryName), so every build of the
//  client on a machine shares one running instance per user, as before.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "SingleInstance.h"

#include <QtCore/QDebug>
#include <QtNetwork/QLocalSocket>

#include <SharedUtil.h>

#include "InterfaceLogging.h"

SingleInstance::SingleInstance(bool allowMultiple, bool hasUrl, const QString& url, QObject* parent) :
    QObject(parent),
    _allowMultiple(allowMultiple),
    _server(this)
#ifdef Q_OS_WIN
    , _sharedMemory(getInterfaceSharedMemoryName())
#endif
{
    const QString& name = getInterfaceSharedMemoryName();
    bool instanceMightBeRunning = true;
#ifdef Q_OS_WIN
    // Try to create a shared memory block - if it can't be created, there is an instance of
    // interface already running. We only do this on Windows for now because of the potential
    // for crashed instances to leave behind shared memory instances on unix.
    instanceMightBeRunning = !_sharedMemory.create(1, QSharedMemory::ReadOnly);
#endif
    if (allowMultiple) {
        instanceMightBeRunning = false;
    }
    if (!instanceMightBeRunning) {
        return;
    }

    // Try to connect and send message to existing interface instance
    QLocalSocket socket;
    socket.connectToServer(name);

    static const int LOCAL_SERVER_TIMEOUT_MS = 500;

    // Try to connect - if we can't connect, interface has probably just gone down
    if (socket.waitForConnected(LOCAL_SERVER_TIMEOUT_MS)) {
        if (hasUrl) {
            qDebug() << "Writing URL to local socket";
            socket.write(url.toUtf8());
            if (!socket.waitForBytesWritten(5000)) {
                qDebug() << "Error writing URL to local socket";
            }
        }
        socket.close();
        qDebug() << "Interface instance appears to be running, exiting";
        _anotherIsRunning = true;
        return;
    }
#ifdef Q_OS_WIN
    _anotherIsRunning = true;   // the block exists, so a client does, whether or not its socket answered
#endif
}

void SingleInstance::listen() {
    if (_allowMultiple) {
        return;
    }
    const QString& name = getInterfaceSharedMemoryName();
    // We failed to connect to a local server, so we remove any existing servers.
    _server.removeServer(name);
    _server.listen(name);
    connect(&_server, &QLocalServer::newConnection, this, [this] {
        qCDebug(interfaceapp) << "Got connection on local server from additional instance - waiting for parameters";
        auto socket = _server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            auto message = socket->readAll();
            socket->deleteLater();
            qCDebug(interfaceapp) << "Read from connection: " << message;
            // If we received a message, try to open it as a URL
            if (message.length() > 0) {
                emit urlHandedOff(QString::fromUtf8(message));
            }
        });
        emit connected();
    }, Qt::DirectConnection);
}
