//
//  AvatarDataTests.h
//  tests/avatars/src
//
//  Avatar data read from a packet: the face's coefficients, and what follows them.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_AvatarDataTests_h
#define hifi_AvatarDataTests_h

#pragma once

#include <QtTest/QtTest>

class AvatarDataTests : public QObject {
    Q_OBJECT
private slots:
    void faceCoefficientsTest();
    void moreFaceCoefficientsThanTheHeadHoldsTest();
    void noFaceCoefficientsTest();
};

#endif // hifi_AvatarDataTests_h
