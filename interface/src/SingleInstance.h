//
//  SingleInstance.h
//  interface/src
//
//  One client per user unless asked otherwise: a second launch hands its address to the running client over a
//  local socket and exits, and the running client listens for such hand-offs and goes where it is sent. On
//  Windows a shared-memory block, held for the process's life, is the first sign of a running client; elsewhere
//  the socket is (a crashed client leaves no block behind there). This stood in main() and in two Application
//  slots (ARCHITECTURE.md §5.4 step 2).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_SingleInstance_h
#define hifi_SingleInstance_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QLocalServer>
#ifdef Q_OS_WIN
#include <QtCore/QSharedMemory>
#endif

class SingleInstance : public QObject {
    Q_OBJECT
public:
    // Before the application exists. Unless several clients are allowed, looks for a running one and, when it
    // answers, hands it `url` (if there is one): anotherIsRunning() is then true and the caller exits.
    SingleInstance(bool allowMultiple, bool hasUrl, const QString& url, QObject* parent = nullptr);

    bool anotherIsRunning() const { return _anotherIsRunning; }

    // Once the application exists: receive the hand-offs of later launches, replacing the socket a crashed client
    // left. Each one signals connected() at once and urlHandedOff() when its address has arrived. Not when several
    // clients are allowed: such a client (a walled-off test client) would otherwise take the socket from the user's
    // own running client and receive its later launches' addresses (2026-10-25).
    void listen();
    // Stops receiving (the application's event loop has ended).
    void close() { _server.close(); }

signals:
    void connected();
    void urlHandedOff(const QString& url);

private:
    bool _allowMultiple { false };
    bool _anotherIsRunning { false };
    QLocalServer _server;
#ifdef Q_OS_WIN
    QSharedMemory _sharedMemory;
#endif
};

#endif // hifi_SingleInstance_h
