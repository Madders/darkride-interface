//
//  PlainScriptValueConversionsTests.h
//  tests/script-values/src
//
//  The plain-value twins of RegisteredMetaTypes' QtScript conversions against the originals: the same members
//  written, the same values read, from the same inputs, edge cases included.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PlainScriptValueConversionsTests_h
#define hifi_PlainScriptValueConversionsTests_h

#include <QtCore/QObject>
#include <QtTest/QtTest>

class PlainScriptValueConversionsTests : public QObject {
    Q_OBJECT
private slots:
    void testWriting();
    void testReadingVectors();
    void testReadingColors();
    void testReadingOthers();
    void testReadingLists();
};

#endif // hifi_PlainScriptValueConversionsTests_h
