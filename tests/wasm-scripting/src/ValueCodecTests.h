//
//  ValueCodecTests.h
//  tests/wasm-scripting/src
//
//  The CBOR codec: every shape a script sees today round-trips, decoding follows the parameter type with
//  QtScript's coercions, objects cross as handles (owned by the table when made for the script), functions as
//  callbacks, unknown types reach the registry (ahead of the codec's enums and object pointers) and the fallback.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ValueCodecTests_h
#define hifi_ValueCodecTests_h

#include <QtTest/QtTest>

class ValueCodecTests : public QObject {
    Q_OBJECT
private slots:
    void testPrimitives();
    void testBytesAreCanonical();
    void testUuidAndUrl();
    void testVectorTypes();
    void testVectorCoercions();
    void testColorsAndGeometry();
    void testContainers();
    void testTypedNumbersAndStrings();
    void testObjects();
    void testCallbacks();
    void testEnums();
    void testRegistryAndFallback();
    void testRegistryPrecedence();
    void testOwnedObjects();
    void testSmartPointers();
    void testDecodeArguments();
    void testMalformedInput();
};

#endif // hifi_ValueCodecTests_h
