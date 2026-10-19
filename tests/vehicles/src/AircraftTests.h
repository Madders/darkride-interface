//
//  AircraftTests.h
//  tests/vehicles/src
//
//  The tilt-thrust aircraft model (PHYSICS.md round 8).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AircraftTests_h
#define hifi_AircraftTests_h

#include <QtTest/QtTest>

class AircraftTests : public QObject {
    Q_OBJECT
private slots:
    void liftAndDragCoefficients();
    void hoverAgainstWeight();
    void levelFlight();
    void aileronsRoll();
    void elevatorPitches();
    void tiltTransition();
    void hoverControls();
};

#endif // hifi_AircraftTests_h
