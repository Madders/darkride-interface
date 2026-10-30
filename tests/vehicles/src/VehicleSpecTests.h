//
//  VehicleSpecTests.h
//  tests/vehicles/src
//
//  The wheeled vehicle's spec as parsed from its entity (PHYSICS.md round 6) and the driver's input packet.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_VehicleSpecTests_h
#define hifi_VehicleSpecTests_h

#include <QtTest/QtTest>

class VehicleSpecTests : public QObject {
    Q_OBJECT
private slots:
    void wheelsFromJSON();
    void axlesAndDrive();
    void seatsAndGears();
    void inputPacket();
    void steering();
};

#endif // hifi_VehicleSpecTests_h
