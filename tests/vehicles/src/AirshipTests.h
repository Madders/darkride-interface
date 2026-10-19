//
//  AirshipTests.h
//  tests/vehicles/src
//
//  The airship model (PHYSICS.md 4.8, round 7): the atmosphere, buoyancy against weight, the fins' signs and the
//  weathercock, the pendulum, drag, and ballast and venting.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AirshipTests_h
#define hifi_AirshipTests_h

#include <QtTest/QtTest>

class AirshipTests : public QObject {
    Q_OBJECT
private slots:
    void atmosphere();
    void buoyancyAgainstWeight();
    void rudderAndElevator();
    void weathercock();
    void pendulum();
    void drag();
    void ballastAndVenting();
    void inputPacket();
};

#endif // hifi_AirshipTests_h
