//
//  PlainScriptValueTests.h
//  tests/script-values/src
//
//  PlainScriptValue against QtScript's QScriptValue on the same operations: the two must agree, value and type,
//  since one source of conversions compiles against both.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PlainScriptValueTests_h
#define hifi_PlainScriptValueTests_h

#include <QtCore/QObject>
#include <QtTest/QtTest>

class PlainScriptValueTests : public QObject {
    Q_OBJECT
private slots:
    void testNumbers();
    void testNumberText();
    void testStrings();
    void testObjects();
    void testArrays();
    void testSharing();
    void testCbor();
    void testFromVariant();
    void testJson();
};

#endif // hifi_PlainScriptValueTests_h
