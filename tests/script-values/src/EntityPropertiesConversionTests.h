//
//  EntityPropertiesConversionTests.h
//  tests/script-values/src
//
//  EntityItemProperties' conversions compiled for plain values against the same source compiled for QtScript:
//  the same members, in the same order, with the same types and values, written for every entity type and
//  read from the same inputs, edge cases included.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityPropertiesConversionTests_h
#define hifi_EntityPropertiesConversionTests_h

#include <QtCore/QObject>
#include <QtTest/QtTest>

class EntityPropertiesConversionTests : public QObject {
    Q_OBJECT
private slots:
    void testEveryTypeWritten();
    void testValuesWritten();
    void testReadingEdgeCases();
    void testReadingWorldEntities();
    void testReadingVariantMaps();
    void testWorldFileText();
    void testAvatarEntityBlobs();
    void testRoundTrip();
    void testVehiclesAndTracks();
    void testWorldFiles();
    void benchmarkOverlayConversions_data();
    void benchmarkOverlayConversions();
};

#endif // hifi_EntityPropertiesConversionTests_h
