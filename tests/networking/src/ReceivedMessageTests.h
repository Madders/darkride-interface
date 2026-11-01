//
//  ReceivedMessageTests.h
//  tests/networking/src
//
//  Reads from a received message keep inside it, whatever sizes the message itself names.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_ReceivedMessageTests_h
#define hifi_ReceivedMessageTests_h

#pragma once

#include <QtTest/QtTest>

class ReceivedMessageTests : public QObject {
    Q_OBJECT
private slots:
    void readStringTest();
    void readStringLongerThanMessageTest();
    void readPastEndTest();
    void readAfterSeekPastEndTest();
    void negativeSizesTest();
};

#endif // hifi_ReceivedMessageTests_h
