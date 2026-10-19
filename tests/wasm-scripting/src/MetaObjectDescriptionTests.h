//
//  MetaObjectDescriptionTests.h
//  tests/wasm-scripting/src
//
//  The description record: what it lists, what it leaves out, and that it survives the codec unchanged.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MetaObjectDescriptionTests_h
#define hifi_MetaObjectDescriptionTests_h

#include <QtTest/QtTest>

class MetaObjectDescriptionTests : public QObject {
    Q_OBJECT
private slots:
    void testMethods();
    void testSignals();
    void testProperties();
    void testEnums();
    void testOptions();
    void testRoundTripsThroughCodecAndJson();
};

#endif // hifi_MetaObjectDescriptionTests_h
