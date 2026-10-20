//
//  TrackTests.h
//  tests/vehicles/src
//
//  The track curve and the follower (PHYSICS.md 4.8): arc lengths, poses, banking, stations and the speed profile.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_TrackTests_h
#define hifi_TrackTests_h

#include <QtTest/QtTest>

class TrackTests : public QObject {
    Q_OBJECT

private slots:
    void straightTrack();
    void closedLoop();
    void banking();
    void shuttleSchedule();
    void loopStations();
    void carVelocities();
    void determinism();
    void stateHandover();
};

#endif // hifi_TrackTests_h
