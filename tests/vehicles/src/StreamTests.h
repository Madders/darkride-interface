//
//  StreamTests.h
//  tests/vehicles/src
//
//  The physics server's state stream (PHYSICS.md round 9): the state and subscription packets round-trip, and the
//  snapshot buffer interpolates between two states, says when it cannot (a late packet), keeps its states in order and
//  sets its delay from the arrivals' jitter.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_StreamTests_h
#define hifi_StreamTests_h

#include <QtTest/QtTest>

class StreamTests : public QObject {
    Q_OBJECT
private slots:
    void statePacketRoundTrip();
    void requestPacketRoundTrip();
    void malformedPackets();
    void interpolation();
    void lateAndEarly();
    void orderAndDuplicates();
    void delayFromJitter();
};

#endif // hifi_StreamTests_h
