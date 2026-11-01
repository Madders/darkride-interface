//
//  SignalRelayTests.h
//  tests/wasm-scripting/src
//
//  The relay: subscriptions by name and by signature, arguments as QVariants, disconnects, delivery on the
//  relay's thread from another thread, and senders or relays that go away.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SignalRelayTests_h
#define hifi_SignalRelayTests_h

#include <QtTest/QtTest>

class SignalRelayTests : public QObject {
    Q_OBJECT
private slots:
    void testConnectByNameAndSignature();
    void testDeliversArguments();
    void testDisconnect();
    void testHandlerMayDisconnectItself();
    void testQueuedAcrossThreads();
    void testSenderDeleted();
    void testRelayDeleted();
    void testSlotsAreReused();
    void testLateQueuedEmissionDropped();
    void testTooManyAtOnce();
};

#endif // hifi_SignalRelayTests_h
