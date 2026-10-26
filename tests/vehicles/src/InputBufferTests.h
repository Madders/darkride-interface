//
//  InputBufferTests.h
//  tests/vehicles/src
//
//  The physics server's time-synchronised input buffer and the driver's lead controller (PHYSICS.md round 10): inputs
//  are applied at the substep their tick names however they arrive (in order, out of order, in a burst, with gaps),
//  late and early ones are refused, a gap holds the last input, the margin reports the lead, the alignment is set
//  again after a restart, and the controller asks for more lead when the least margin falls short and for less when
//  it stays long.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_InputBufferTests_h
#define hifi_InputBufferTests_h

#include <QtTest/QtTest>

class InputBufferTests : public QObject {
    Q_OBJECT
private slots:
    void alignmentAndOrder();
    void outOfOrderAndBurst();
    void gapsHoldTheLast();
    void lateAndEarly();
    void margin();
    void realignment();
    void lateForLong();
    void leadAdaptation();
    void standingAndSittingAgain();
};

#endif // hifi_InputBufferTests_h
