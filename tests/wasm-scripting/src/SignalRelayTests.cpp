//
//  SignalRelayTests.cpp
//  tests/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SignalRelayTests.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <SignalRelay.h>

#include "TestObjects.h"

QTEST_MAIN(SignalRelayTests)

namespace {

struct Received {
    int connection;
    QVariantList arguments;
};

SignalRelay::Handler collect(QList<Received>& into) {
    return [&into](int connection, const QVariantList& arguments) { into.append(Received { connection, arguments }); };
}

} // namespace

void SignalRelayTests::testConnectByNameAndSignature() {
    Probe probe;
    SignalRelay relay;
    QString error;
    int byName = relay.connect(&probe, "countChanged", [](int, const QVariantList&) {}, error);
    QVERIFY2(byName > 0, qPrintable(error));
    int bySignature = relay.connect(&probe, "countChanged(int)", [](int, const QVariantList&) {}, error);
    QVERIFY2(bySignature > 0, qPrintable(error));
    QVERIFY(bySignature != byName);
    QCOMPARE(relay.connectionCount(), 2);
    QCOMPARE(relay.sender(byName), &probe);
    QCOMPARE(relay.signal(byName).name(), QByteArray("countChanged"));
    QCOMPARE(relay.signal(bySignature).methodSignature(), QByteArray("countChanged(int)"));
    // the spaces of a hand-written signature are normalised
    int spaced = relay.connect(&probe, "named( QString , bool )", [](int, const QVariantList&) {}, error);
    QVERIFY2(spaced > 0, qPrintable(error));

    QCOMPARE(relay.connect(&probe, "noSuchSignal", [](int, const QVariantList&) {}, error), 0);
    QVERIFY(error.contains("noSuchSignal"));
    QCOMPARE(relay.connect(&probe, "countChanged(QString)", [](int, const QVariantList&) {}, error), 0);
    // a method that is not a signal is not connectable
    QCOMPARE(relay.connect(&probe, "reset", [](int, const QVariantList&) {}, error), 0);
    QCOMPARE(relay.connect(nullptr, "countChanged", [](int, const QVariantList&) {}, error), 0);
    QCOMPARE(relay.connectionCount(), 3);
}

void SignalRelayTests::testDeliversArguments() {
    Probe probe;
    SignalRelay relay;
    QList<Received> received;
    QString error;
    int count = relay.connect(&probe, "countChanged", collect(received), error);
    int position = relay.connect(&probe, "positionChanged", collect(received), error);
    int named = relay.connect(&probe, "named", collect(received), error);
    int bare = relay.connect(&probe, "bare", collect(received), error);
    int carried = relay.connect(&probe, "carried", collect(received), error);
    int object = relay.connect(&probe, "objectSent", collect(received), error);
    QVERIFY(count && position && named && bare && carried && object);

    probe.setCount(5);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received[0].connection, count);
    QCOMPARE(received[0].arguments, QVariantList { 5 });
    QCOMPARE(received[0].arguments[0].userType(), int(QMetaType::Int));

    probe.setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    QCOMPARE(received.size(), 2);
    QCOMPARE(received[1].connection, position);
    QCOMPARE(received[1].arguments.size(), 1);
    QCOMPARE(received[1].arguments[0].userType(), qMetaTypeId<glm::vec3>());
    QCOMPARE(received[1].arguments[0].value<glm::vec3>(), glm::vec3(1.0f, 2.0f, 3.0f));

    probe.send("hello", true);
    QCOMPARE(received.size(), 3);
    QCOMPARE(received[2].connection, named);
    QCOMPARE(received[2].arguments, (QVariantList { "hello", true }));

    probe.ping();
    QCOMPARE(received.size(), 4);
    QCOMPARE(received[3].connection, bare);
    QVERIFY(received[3].arguments.isEmpty());

    probe.carry(QVariant(2.5));
    QCOMPARE(received.size(), 5);
    QCOMPARE(received[4].connection, carried);
    QCOMPARE(received[4].arguments, QVariantList { 2.5 });
    QCOMPARE(received[4].arguments[0].userType(), int(QMetaType::Double));

    Other other;
    probe.sendObject(&other);
    QCOMPARE(received.size(), 6);
    QCOMPARE(received[5].connection, object);
    QCOMPARE(received[5].arguments[0].value<QObject*>(), &other);

    // two subscriptions to one signal both fire, in subscription order
    int countAgain = relay.connect(&probe, "countChanged", collect(received), error);
    probe.setCount(6);
    QCOMPARE(received.size(), 8);
    QCOMPARE(received[6].connection, count);
    QCOMPARE(received[7].connection, countAgain);
}

void SignalRelayTests::testDisconnect() {
    Probe probe;
    SignalRelay relay;
    QList<Received> received;
    QString error;
    int first = relay.connect(&probe, "countChanged", collect(received), error);
    int second = relay.connect(&probe, "countChanged", collect(received), error);
    probe.setCount(1);
    QCOMPARE(received.size(), 2);
    QVERIFY(relay.disconnect(first));
    QVERIFY(!relay.disconnect(first));
    QVERIFY(!relay.isConnected(first));
    QVERIFY(relay.isConnected(second));
    probe.setCount(2);
    QCOMPARE(received.size(), 3);
    QCOMPARE(received[2].connection, second);
    relay.disconnectAll();
    QCOMPARE(relay.connectionCount(), 0);
    probe.setCount(3);
    QCOMPARE(received.size(), 3);
    QVERIFY(!relay.disconnect(12345));
}

void SignalRelayTests::testHandlerMayDisconnectItself() {
    Probe probe;
    SignalRelay relay;
    QString error;
    int calls = 0;
    int once = relay.connect(&probe, "countChanged", [&](int connection, const QVariantList&) {
        ++calls;
        relay.disconnect(connection);
    }, error);
    QVERIFY(once > 0);
    probe.setCount(1);
    probe.setCount(2);
    QCOMPARE(calls, 1);
    QCOMPARE(relay.connectionCount(), 0);
}

void SignalRelayTests::testQueuedAcrossThreads() {
    // the sender lives on another thread; the handler must run on this one, with the arguments copied
    QThread thread;
    thread.start();
    auto probe = new Probe();
    probe->moveToThread(&thread);
    SignalRelay relay;
    QList<Received> received;
    QThread* handlerThread = nullptr;
    QString error;
    int count = relay.connect(probe, "countChanged", [&](int connection, const QVariantList& arguments) {
        handlerThread = QThread::currentThread();
        received.append(Received { connection, arguments });
    }, error);
    int position = relay.connect(probe, "positionChanged", [&](int connection, const QVariantList& arguments) {
        received.append(Received { connection, arguments });
    }, error);
    QVERIFY(count && position);

    QVERIFY(QMetaObject::invokeMethod(probe, "setCount", Qt::QueuedConnection, Q_ARG(int, 9)));
    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(handlerThread, QThread::currentThread());
    QCOMPARE(received[0].connection, count);
    QCOMPARE(received[0].arguments, QVariantList { 9 });

    QVERIFY(QMetaObject::invokeMethod(probe, "setPosition", Qt::QueuedConnection, Q_ARG(glm::vec3, glm::vec3(4.0f))));
    QTRY_COMPARE(received.size(), 2);
    QCOMPARE(received[1].arguments[0].value<glm::vec3>(), glm::vec3(4.0f));

    QMetaObject::invokeMethod(probe, "deleteLater", Qt::QueuedConnection);
    thread.quit();
    thread.wait();
}

void SignalRelayTests::testSenderDeleted() {
    auto probe = new Probe();
    SignalRelay relay;
    QList<Received> received;
    QString error;
    int count = relay.connect(probe, "countChanged", collect(received), error);
    QVERIFY(count > 0);
    delete probe;
    // the subscription is still listed, its sender gone; ending it is fine
    QVERIFY(relay.isConnected(count));
    QCOMPARE(relay.sender(count), nullptr);
    QVERIFY(relay.disconnect(count));
    QCOMPARE(relay.connectionCount(), 0);
}

void SignalRelayTests::testRelayDeleted() {
    Probe probe;
    QList<Received> received;
    QString error;
    {
        SignalRelay relay;
        QVERIFY(relay.connect(&probe, "countChanged", collect(received), error) > 0);
        probe.setCount(1);
        QCOMPARE(received.size(), 1);
    }
    // the relay's destructor dropped the connection: no call, no crash
    probe.setCount(2);
    QCOMPARE(received.size(), 1);
}

// TestObjects.h has no source file of its own, so its moc output is pulled in here.
#include "moc_TestObjects.cpp"
