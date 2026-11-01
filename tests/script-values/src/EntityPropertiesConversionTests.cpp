//
//  EntityPropertiesConversionTests.cpp
//  tests/script-values/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityPropertiesConversionTests.h"

#include <QtCore/QCborValue>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptValueIterator>

#include <EntityItemProperties.h>
#include <EntityTypes.h>
#include <Gzip.h>
#include <PlainScriptValue.h>
#include <SharedUtil.h>
#include <VariantMapToScriptValue.h>

#include "SameVariant.h"

QTEST_GUILESS_MAIN(EntityPropertiesConversionTests)

namespace {

// The names of an object's own properties in the order QtScript lists them
QStringList scriptNames(const QScriptValue& value) {
    QStringList names;
    QScriptValueIterator it(value);
    while (it.hasNext()) {
        it.next();
        names.append(it.name());
    }
    return names;
}

// Whether the two values have the same properties in the same order, nested values included (an array's
// elements by index; the values themselves are compared through their variants)
bool sameShape(const QScriptValue& script, const PlainScriptValue& plain, QString& where) {
    if (script.isArray()) {
        if (!plain.isArray()) {
            where = "an array on QtScript only";
            return false;
        }
        const int length = script.property("length").toInt32();
        if (plain.property("length").toInt32() != length) {
            where = QString("array length %1 vs %2").arg(length).arg(plain.property("length").toInt32());
            return false;
        }
        for (int i = 0; i < length; ++i) {
            if (!sameShape(script.property(i), plain.property(i), where)) {
                where = QString("[%1] ").arg(i) + where;
                return false;
            }
        }
        return true;
    }
    if (script.isObject()) {
        if (!plain.isObject() || plain.isArray()) {
            where = "an object on QtScript only";
            return false;
        }
        const QStringList a = scriptNames(script);
        const QStringList b = plain.propertyNames();
        if (a != b) {
            where = "order " + a.join(",") + " vs " + b.join(",");
            return false;
        }
        for (const QString& name : a) {
            if (!sameShape(script.property(name), plain.property(name), where)) {
                where = name + ": " + where;
                return false;
            }
        }
        return true;
    }
    return true;
}

// age and ageAsText come from the clock at the moment of the conversion, not from the properties
void stripClock(QVariantMap& map) {
    map.remove("age");
    map.remove("ageAsText");
}

// The properties written both ways: the same variant (type and value) and the same shape (order)
bool sameWriting(const EntityItemProperties& properties, bool skipDefaults, bool allowUnknownCreateTime, bool strictSemantics,
                 EntityPsuedoPropertyFlags flags, QString& where) {
    QScriptEngine engine;
    PlainScriptEngine plainEngine;
    const QScriptValue script = properties.copyToScriptValue(&engine, skipDefaults, allowUnknownCreateTime, strictSemantics, flags);
    const PlainScriptValue plain = properties.copyToScriptValue(&plainEngine, skipDefaults, allowUnknownCreateTime, strictSemantics, flags);
    QVariantMap a = script.toVariant().toMap();
    QVariantMap b = plain.toVariant().toMap();
    if (a.contains("age") != b.contains("age") || a.contains("ageAsText") != b.contains("ageAsText")) {
        where = "age present on one side only";
        return false;
    }
    stripClock(a);
    stripClock(b);
    if (!sameVariant(a, b, where)) {
        where = "value: " + where;
        return false;
    }
    if (!sameShape(script, plain, where)) {
        where = "shape: " + where;
        return false;
    }
    if (a.isEmpty()) {
        where = "nothing written";
        return false;
    }
    return true;
}

bool sameWritingEveryWay(const EntityItemProperties& properties, QString& where) {
    EntityPsuedoPropertyFlags pseudo;
    pseudo.set(EntityPsuedoPropertyFlag::FlagsActive);
    pseudo.set(EntityPsuedoPropertyFlag::ID);
    pseudo.set(EntityPsuedoPropertyFlag::Type);
    pseudo.set(EntityPsuedoPropertyFlag::BoundingBox);
    pseudo.set(EntityPsuedoPropertyFlag::RenderInfo);
    pseudo.set(EntityPsuedoPropertyFlag::FaceCamera);
    EntityItemProperties desired = properties;
    EntityPropertyFlags flags;
    flags << PROP_POSITION;
    flags << PROP_NAME;
    flags << PROP_COLOR;
    desired.setDesiredProperties(flags);
    struct Way {
        const char* name;
        const EntityItemProperties& properties;
        bool skipDefaults;
        bool strictSemantics;
        EntityPsuedoPropertyFlags flags;
    };
    const Way ways[] = {
        { "everything", properties, false, false, EntityPsuedoPropertyFlags() },
        { "non-defaults", properties, true, false, EntityPsuedoPropertyFlags() },
        { "strict", properties, false, true, EntityPsuedoPropertyFlags() },
        { "desired", desired, false, false, EntityPsuedoPropertyFlags() },
        { "desired, strict", desired, false, true, EntityPsuedoPropertyFlags() },
        { "pseudo-properties", properties, false, false, pseudo },
        { "desired with pseudo-properties", desired, false, false, pseudo },
    };
    for (const Way& way : ways) {
        if (!sameWriting(way.properties, way.skipDefaults, true, way.strictSemantics, way.flags, where)) {
            where = QString(way.name) + ": " + where;
            return false;
        }
    }
    return true;
}

// The same input read both ways, compared as QtScript writes the properties read
bool sameReading(const QString& json, bool honorReadOnly, QString& where) {
    QScriptEngine engine;
    const QScriptValue scriptInput = engine.evaluate("(" + json + ")");
    if (engine.hasUncaughtException()) {
        where = "the input does not evaluate: " + engine.uncaughtException().toString();
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        where = "the input is not JSON: " + error.errorString();
        return false;
    }
    const PlainScriptValue plainInput = PlainScriptValue::fromCbor(QCborValue::fromJsonValue(document.object()));

    EntityItemProperties fromScript;
    EntityItemProperties fromPlain;
    fromScript.copyFromScriptValue(scriptInput, honorReadOnly);
    fromPlain.copyFromScriptValue(plainInput, honorReadOnly);
    fromPlain.setLastEdited(fromScript.getLastEdited());  // the clock, not the conversion

    QVariantMap a = fromScript.copyToScriptValue(&engine, false, true).toVariant().toMap();
    QVariantMap b = fromPlain.copyToScriptValue(&engine, false, true).toVariant().toMap();
    stripClock(a);
    stripClock(b);
    if (!sameVariant(a, b, where)) {
        return false;
    }
    // and the plain reading agrees with itself through the plain writing
    PlainScriptEngine plainEngine;
    QVariantMap c = fromPlain.copyToScriptValue(&plainEngine, false, true).toVariant().toMap();
    stripClock(c);
    if (!sameVariant(a, c, where)) {
        where = "plain writing of the plain reading: " + where;
        return false;
    }
    return true;
}

// A map read as the callers outside scripting read one (a world file's entity, avatar-entity data, an avatar
// bookmark, an overlay's properties): through variantMapToScriptValue on QtScript and fromVariant on the plain
// path, compared as sameReading compares
bool sameVariantReading(const QVariantMap& map, bool honorReadOnly, QString& where) {
    QScriptEngine engine;
    QVariantMap input = map;
    const QScriptValue scriptInput = variantMapToScriptValue(input, engine);
    const PlainScriptValue plainInput = PlainScriptValue::fromVariant(map);

    EntityItemProperties fromScript;
    EntityItemProperties fromPlain;
    fromScript.copyFromScriptValue(scriptInput, honorReadOnly);
    fromPlain.copyFromScriptValue(plainInput, honorReadOnly);
    fromPlain.setLastEdited(fromScript.getLastEdited());

    QVariantMap a = fromScript.copyToScriptValue(&engine, false, true).toVariant().toMap();
    QVariantMap b = fromPlain.copyToScriptValue(&engine, false, true).toVariant().toMap();
    stripClock(a);
    stripClock(b);
    if (!sameVariant(a, b, where)) {
        return false;
    }
    PlainScriptEngine plainEngine;
    QVariantMap c = fromPlain.copyToScriptValue(&plainEngine, false, true).toVariant().toMap();
    stripClock(c);
    if (!sameVariant(a, c, where)) {
        where = "plain writing of the plain reading: " + where;
        return false;
    }
    return true;
}

EntityItemProperties sampleProperties(EntityTypes::EntityType type) {
    EntityItemProperties properties;
    properties.setType(type);
    properties.setCreated(usecTimestampNow() - USECS_PER_SECOND);
    properties.setPosition(glm::vec3(1.5f, -2.0f, 3.25f));
    properties.setRotation(glm::quat(0.5f, 0.5f, 0.5f, 0.5f));
    properties.setDimensions(glm::vec3(0.5f, 1.0f, 2.0f));
    properties.setColor(glm::u8vec3(255, 128, 0));
    properties.setName("sample");
    properties.setScript("https://example.invalid/script.js");
    properties.setScriptsEnabled(false);
    properties.setUserData("{\"grabbable\": true}");
    properties.setParentID(QUuid("{1a2b3c4d-0000-0000-0000-000000000001}"));
    properties.setLifetime(60.0f);
    properties.setCollisionless(true);
    properties.setDynamic(true);
    properties.setModelURL("https://example.invalid/model.glb");
    properties.setImageURL("https://example.invalid/image.png");
    properties.setShapeType(SHAPE_TYPE_BOX);
    properties.setBillboardMode(BillboardMode::YAW);
    properties.setActionData(QByteArray("action bytes"));
    properties.setLimitedRun(7);
    properties.getKeyLight().setColor(glm::u8vec3(10, 20, 30));
    properties.getKeyLight().setIntensity(0.75f);
    properties.getGrab().setGrabbable(false);
    properties.getAnimation().setURL("https://example.invalid/animation.fbx");
    properties.getAnimation().setFPS(24.0f);
    properties.getPulse().setMin(0.25f);
    return properties;
}

}  // namespace

void EntityPropertiesConversionTests::testEveryTypeWritten() {
    for (int type = EntityTypes::Box; type < EntityTypes::NUM_TYPES; ++type) {
        if (type == EntityTypes::RemovedPolyVox) {
            continue;
        }
        EntityItemProperties properties;
        properties.setType(static_cast<EntityTypes::EntityType>(type));
        properties.setCreated(usecTimestampNow());
        QString where;
        QVERIFY2(sameWritingEveryWay(properties, where),
                 qPrintable(EntityTypes::getEntityTypeName(static_cast<EntityTypes::EntityType>(type)) + ": " + where));
        // with no creation time, nothing is written unless allowed
        EntityItemProperties fresh;
        fresh.setType(static_cast<EntityTypes::EntityType>(type));
        QScriptEngine engine;
        PlainScriptEngine plainEngine;
        QVERIFY(fresh.copyToScriptValue(&engine, false).toVariant().toMap().isEmpty());
        QVERIFY(fresh.copyToScriptValue(&plainEngine, false).toVariant().toMap().isEmpty());
    }
}

void EntityPropertiesConversionTests::testValuesWritten() {
    for (EntityTypes::EntityType type : { EntityTypes::Model, EntityTypes::Image, EntityTypes::Zone, EntityTypes::Grid, EntityTypes::Text,
                                          EntityTypes::Web, EntityTypes::ParticleEffect, EntityTypes::PolyLine, EntityTypes::Light,
                                          EntityTypes::Material, EntityTypes::Shape, EntityTypes::Gizmo, EntityTypes::Terrain,
                                          EntityTypes::Track }) {
        QString where;
        QVERIFY2(sameWritingEveryWay(sampleProperties(type), where), qPrintable(EntityTypes::getEntityTypeName(type) + ": " + where));
    }
    // what a module sees: the keys in QtScript's order, not sorted
    PlainScriptEngine plainEngine;
    const QStringList names = sampleProperties(EntityTypes::Model).copyToScriptValue(&plainEngine, false).propertyNames();
    QVERIFY(names.indexOf("id") < names.indexOf("type") && names.indexOf("type") < names.indexOf("name"));
    QVERIFY(names.indexOf("name") < names.indexOf("position") && names.indexOf("position") < names.indexOf("modelURL"));
    QStringList sorted = names;
    sorted.sort();
    QVERIFY(names != sorted);
}

void EntityPropertiesConversionTests::testReadingEdgeCases() {
    const QStringList inputs {
        // vectors: partial, a number, a colour name, an array, aliases
        "{\"type\": \"Box\", \"position\": {\"x\": 1}}",
        "{\"type\": \"Box\", \"position\": 5}",
        "{\"type\": \"Box\", \"dimensions\": [1, 2, 3]}",
        "{\"type\": \"Box\", \"dimensions\": [1, 2]}",
        "{\"type\": \"Box\", \"color\": \"red\"}",
        "{\"type\": \"Box\", \"color\": {\"r\": 1, \"g\": 2, \"b\": 3}}",
        "{\"type\": \"Box\", \"color\": [10, 20, 30]}",
        "{\"type\": \"Box\", \"color\": 7}",
        "{\"type\": \"Zone\", \"keyLight\": {\"color\": \"blue\", \"direction\": {\"x\": \"NaN\", \"y\": 1, \"z\": 0}}}",
        // quaternions: whole, partial, with a NaN
        "{\"type\": \"Box\", \"rotation\": {\"x\": 0, \"y\": 0.7071, \"z\": 0, \"w\": 0.7071}}",
        "{\"type\": \"Box\", \"rotation\": {\"x\": 1, \"y\": 1}}",
        "{\"type\": \"Box\", \"rotation\": {\"x\": \"NaN\", \"y\": 0, \"z\": 0, \"w\": 1}}",
        // the legacy names
        "{\"type\": \"Model\", \"animationURL\": \"https://example.invalid/a.fbx\", \"animationFPS\": 12}",
        "{\"type\": \"Box\", \"ignoreForCollisions\": true, \"collisionsWillMove\": true}",
        "{\"type\": \"Image\", \"textures\": \"{\\\"tex.picture\\\": \\\"https://example.invalid/p.png\\\"}\"}",
        "{\"type\": \"Image\", \"textures\": \"{\\\"tex.picture\\\": \\\"https://example.invalid/p.png\\\"}\", \"imageURL\": \"https://example.invalid/q.png\"}",
        "{\"type\": \"Text\", \"faceCamera\": true}",
        "{\"type\": \"Text\", \"isFacingAvatar\": true}",
        "{\"type\": \"Text\", \"faceCamera\": true, \"isFacingAvatar\": true}",
        "{\"type\": \"Text\", \"faceCamera\": true, \"billboardMode\": \"full\"}",
        "{\"type\": \"PolyLine\", \"faceCamera\": true}",
        // enumerations by name, good and bad
        "{\"type\": \"Shape\", \"shapeType\": \"sphere\", \"shape\": \"Cylinder\", \"billboardMode\": \"yaw\"}",
        "{\"type\": \"Shape\", \"shapeType\": \"nonsense\", \"billboardMode\": 3}",
        "{\"type\": \"Zone\", \"keyLightMode\": \"enabled\", \"shapeType\": \"box\"}",
        // numbers as text, out of range, and the unsigned readers
        "{\"type\": \"Box\", \"parentJointIndex\": \"7\", \"limitedRun\": \"4294967295\", \"editionNumber\": \"12.5\"}",
        "{\"type\": \"Box\", \"parentJointIndex\": 70000, \"limitedRun\": -1, \"lifetime\": \"abc\", \"alpha\": \"0.5\"}",
        "{\"type\": \"Box\", \"created\": 1700000000000000, \"scriptTimestamp\": \"42\"}",
        // bytes as base64, an id with and without braces
        "{\"type\": \"Box\", \"actionData\": \"YWN0aW9u\", \"parentID\": \"{1a2b3c4d-0000-0000-0000-000000000001}\"}",
        "{\"type\": \"Box\", \"parentID\": \"1a2b3c4d-0000-0000-0000-000000000002\", \"owningAvatarID\": \"not an id\"}",
        // nulls, undefined through a missing value, booleans as other things
        "{\"type\": \"Box\", \"name\": null, \"position\": null, \"visible\": null, \"color\": null, \"userData\": null}",
        "{\"type\": \"Box\", \"visible\": \"false\", \"locked\": 0, \"dynamic\": \"yes\", \"collisionless\": 1}",
        // groups: a group as a whole, a group's value of the wrong kind
        "{\"type\": \"Zone\", \"haze\": {\"hazeRange\": 100, \"hazeColor\": {\"red\": 1, \"green\": 2, \"blue\": 3}}, \"bloom\": 5}",
        "{\"type\": \"Box\", \"grab\": {\"grabbable\": false, \"equippableLeftPosition\": {\"x\": 1, \"y\": 2, \"z\": 3}}, \"pulse\": {\"min\": 0.1, \"colorMode\": \"in\"}}",
        // the type missing, unknown, or changed
        "{\"position\": {\"x\": 1, \"y\": 2, \"z\": 3}}",
        "{\"type\": \"Nonsense\", \"name\": \"x\"}",
        "{\"type\": \"Model\", \"modelURL\": \"https://example.invalid/m.glb\", \"jointRotations\": [{\"x\": 0, \"y\": 0, \"z\": 0, \"w\": 1}, {}], \"jointRotationsSet\": [true, false]}",
        // lists of numbers and strings
        "{\"type\": \"PolyLine\", \"linePoints\": [{\"x\": 0, \"y\": 0, \"z\": 0}, {\"x\": 1, \"y\": 1, \"z\": 1}], \"strokeWidths\": [0.1, \"0.2\", null], \"strokeColors\": [{\"x\": 1, \"y\": 0, \"z\": 0}]}",
        "{\"type\": \"ParticleEffect\", \"emitRate\": 10, \"emitOrientation\": {\"x\": 0, \"y\": 0, \"z\": 0, \"w\": 1}, \"particleRadius\": \"0.5\"}",
        "{}"
    };
    for (const QString& input : inputs) {
        QString where;
        QVERIFY2(sameReading(input, true, where), qPrintable(input + ": " + where));
        QVERIFY2(sameReading(input, false, where), qPrintable(input + " (read-only ignored): " + where));
    }
}

void EntityPropertiesConversionTests::testReadingWorldEntities() {
    const QString path = QFileInfo(__FILE__).dir().filePath("../../../tests-manual/wasm-scripting/world.json");
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable("no test world at " + path));
    const QJsonArray entities = QJsonDocument::fromJson(file.readAll()).object().value("Entities").toArray();
    QVERIFY(!entities.isEmpty());
    for (const QJsonValue& entity : entities) {
        const QString json = QString::fromUtf8(QJsonDocument(entity.toObject()).toJson(QJsonDocument::Compact));
        QString where;
        QVERIFY2(sameReading(json, true, where), qPrintable(json.left(80) + ": " + where));
    }
}

void EntityPropertiesConversionTests::testReadingVariantMaps() {
    // what an avatar bookmark stores and reads back, and what the overlay conversion starts from: a full
    // property map as toVariant() gives it, for every entity type
    for (int type = EntityTypes::Box; type < EntityTypes::NUM_TYPES; ++type) {
        QScriptEngine engine;
        const QVariantMap written = sampleProperties((EntityTypes::EntityType)type).copyToScriptValue(&engine, false).toVariant().toMap();
        const QString name = EntityTypes::getEntityTypeName((EntityTypes::EntityType)type);
        QString where;
        QVERIFY2(sameVariantReading(written, false, where), qPrintable(name + ": " + where));
        QVERIFY2(sameVariantReading(written, true, where), qPrintable(name + " (read-only honoured): " + where));
    }

    // an overlay's properties as the Overlays API gets them from a script: ints and doubles, maps for the vectors
    // and the colour, lists, a null, text
    QVariantMap overlay;
    overlay["type"] = "Shape";
    overlay["shape"] = "Cube";
    overlay["name"] = "an overlay";
    overlay["position"] = QVariantMap({ { "x", 1 }, { "y", 2.5 }, { "z", -3 } });
    overlay["dimensions"] = QVariantMap({ { "x", 1 }, { "y", 1 }, { "z", 0.25 } });
    overlay["rotation"] = QVariantMap({ { "x", 0 }, { "y", 0.7071 }, { "z", 0 }, { "w", 0.7071 } });
    overlay["color"] = QVariantMap({ { "red", 255 }, { "green", 128 }, { "blue", 0 } });
    overlay["alpha"] = 0.5;
    overlay["visible"] = true;
    overlay["lifetime"] = -1;
    overlay["parentID"] = "{1a2b3c4d-0000-0000-0000-000000000001}";
    overlay["parentJointIndex"] = 65535;
    overlay["renderLayer"] = "hud";
    overlay["primitiveMode"] = "lines";
    overlay["ignorePickIntersection"] = false;
    overlay["linePoints"] = QVariantList({ QVariantMap({ { "x", 0 }, { "y", 0 }, { "z", 0 } }), QVariantMap({ { "x", 1 }, { "y", 1 }, { "z", 1 } }) });
    overlay["strokeWidths"] = QVariantList({ 0.1, 0.2 });
    overlay["textures"] = "{\"tex\": \"https://example.invalid/t.png\"}";
    overlay["userData"] = QVariant();
    overlay["url"] = QUrl("https://example.invalid/model.glb");
    QString where;
    QVERIFY2(sameVariantReading(overlay, true, where), qPrintable("overlay: " + where));
    overlay["type"] = "PolyLine";
    QVERIFY2(sameVariantReading(overlay, true, where), qPrintable("polyline overlay: " + where));

    // the world file's entities as EntityTree::readFromMap gets them: QJsonDocument's variants, every number a double
    const QString path = QFileInfo(__FILE__).dir().filePath("../../../tests-manual/wasm-scripting/world.json");
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable("no test world at " + path));
    const QVariantList entities = QJsonDocument::fromJson(file.readAll()).toVariant().toMap().value("Entities").toList();
    QVERIFY(!entities.isEmpty());
    for (const QVariant& entity : entities) {
        const QVariantMap map = entity.toMap();
        QVERIFY2(sameVariantReading(map, true, where), qPrintable(map.value("id").toString() + ": " + where));
        QVERIFY2(sameVariantReading(map, false, where), qPrintable(map.value("id").toString() + " (read-only ignored): " + where));
    }
}

namespace {

// An entity's text in the entity server's world file: JSON.stringify(properties, null, '    ') of what
// copyToScriptValue writes, which RecurseOctreeToJSONOperator had QtScript's own JSON.stringify produce; age
// and ageAsText come from the clock and are taken out on both sides
bool sameWorldFileText(const EntityItemProperties& properties, bool skipDefaults, QString& where) {
    QScriptEngine engine;
    PlainScriptEngine plainEngine;
    QScriptValue script = properties.copyToScriptValue(&engine, skipDefaults);
    PlainScriptValue plain = properties.copyToScriptValue(&plainEngine, skipDefaults);
    for (const QString& name : { QString("age"), QString("ageAsText") }) {
        script.setProperty(name, QScriptValue());
        plain.setProperty(name, PlainScriptValue());
    }
    QScriptValue json = engine.globalObject().property("JSON");
    const QString expected = json.property("stringify").call(json, QScriptValueList() << script << QScriptValue(QScriptValue::NullValue) << QScriptValue("    ")).toString();
    const QString actual = plain.toJson("    ");
    if (actual != expected) {
        int at = 0;
        while (at < actual.size() && at < expected.size() && actual.at(at) == expected.at(at)) {
            ++at;
        }
        where = QString("differs at %1: ...%2... vs ...%3...").arg(at).arg(actual.mid(qMax(0, at - 40), 80)).arg(expected.mid(qMax(0, at - 40), 80));
        return false;
    }
    if (expected.size() < 100) {
        where = "too short to be an entity: " + expected;
        return false;
    }
    return true;
}

}  // namespace

void EntityPropertiesConversionTests::testWorldFileText() {
    for (int type = EntityTypes::Box; type < EntityTypes::NUM_TYPES; ++type) {
        const QString name = EntityTypes::getEntityTypeName((EntityTypes::EntityType)type);
        QString where;
        QVERIFY2(sameWorldFileText(sampleProperties((EntityTypes::EntityType)type), true, where), qPrintable(name + ": " + where));
        QVERIFY2(sameWorldFileText(sampleProperties((EntityTypes::EntityType)type), false, where), qPrintable(name + " (defaults too): " + where));
    }
    // the test world's entities, as read for a load and written again as a live entity's are (one has a
    // creation time; properties without one write nothing)
    const QString path = QFileInfo(__FILE__).dir().filePath("../../../tests-manual/wasm-scripting/world.json");
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable("no test world at " + path));
    const QVariantList entities = QJsonDocument::fromJson(file.readAll()).toVariant().toMap().value("Entities").toList();
    QVERIFY(!entities.isEmpty());
    for (const QVariant& entity : entities) {
        EntityItemProperties properties;
        properties.copyFromScriptValue(PlainScriptValue::fromVariant(entity.toMap()), false);
        properties.setCreated(usecTimestampNow() - USECS_PER_SECOND);
        QString where;
        QVERIFY2(sameWorldFileText(properties, true, where), qPrintable(entity.toMap().value("id").toString() + ": " + where));
        QVERIFY2(sameWorldFileText(properties, false, where), qPrintable(entity.toMap().value("id").toString() + " (defaults too): " + where));
    }
}

void EntityPropertiesConversionTests::testAvatarEntityBlobs() {
    // propertiesToBlob and blobToProperties as they were, with a QScriptEngine, against the plain functions
    const QUuid myAvatarID("{aaaaaaaa-0000-0000-0000-00000000000a}");
    for (int type = EntityTypes::Box; type < EntityTypes::NUM_TYPES; ++type) {
        const QString name = EntityTypes::getEntityTypeName((EntityTypes::EntityType)type);
        EntityItemProperties properties = sampleProperties((EntityTypes::EntityType)type);
        properties.setParentID(myAvatarID);
        for (bool allProperties : { false, true }) {
            QScriptEngine engine;
            QVariant variantProperties = properties.copyToScriptValue(&engine, !allProperties).toVariant();
            QJsonObject jsonObject = QJsonDocument::fromVariant(variantProperties).object();
            if (jsonObject.contains("parentID") && QUuid(jsonObject["parentID"].toString()) == myAvatarID) {
                jsonObject["parentID"] = AVATAR_SELF_ID.toString();
            }
            jsonObject.remove("age");
            jsonObject.remove("ageAsText");
            const QByteArray expected = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);

            QByteArray blob;
            EntityItemProperties::propertiesToBlob(myAvatarID, properties, blob, allProperties);
            QJsonObject written = QJsonDocument::fromJson(blob).object();
            written.remove("age");
            written.remove("ageAsText");
            const QByteArray actual = QJsonDocument(written).toJson(QJsonDocument::Compact);
            QVERIFY2(actual == expected, qPrintable(name + ": " + actual.left(200) + " vs " + expected.left(200)));
            QVERIFY2(written["parentID"].toString() == AVATAR_SELF_ID.toString(), qPrintable(name + ": the avatar's own id is not replaced"));

            // and read back both ways
            EntityItemProperties fromScript;
            QVariantMap map = QJsonDocument::fromJson(blob).toVariant().toMap();
            fromScript.copyFromScriptValue(variantMapToScriptValue(map, engine), false);
            EntityItemProperties fromPlain;
            QVERIFY2(EntityItemProperties::blobToProperties(blob, fromPlain), qPrintable(name + ": the blob does not read"));
            QVariantMap a = fromScript.copyToScriptValue(&engine, false, true).toVariant().toMap();
            QVariantMap b = fromPlain.copyToScriptValue(&engine, false, true).toVariant().toMap();
            stripClock(a);
            stripClock(b);
            // lastEdited is the clock at the end of each reading (and setLastEdited clamps to the creation time,
            // which both paths read through a float, as variantToScriptValue reads a 64-bit integer: a known quirk)
            a.remove("lastEdited");
            b.remove("lastEdited");
            QString where;
            QVERIFY2(sameVariant(a, b, where), qPrintable(name + ": " + where));
        }
    }
    EntityItemProperties none;
    QVERIFY(!EntityItemProperties::blobToProperties(QByteArray(), none));
    QVERIFY(!EntityItemProperties::blobToProperties(QByteArray("[1, 2]"), none));
    QVERIFY(!EntityItemProperties::blobToProperties(QByteArray("{}"), none));
}

void EntityPropertiesConversionTests::testRoundTrip() {
    // what QtScript writes for an entity with values, read back both ways, as a script's full property map is
    for (EntityTypes::EntityType type : { EntityTypes::Model, EntityTypes::Image, EntityTypes::Zone, EntityTypes::Text, EntityTypes::Shape }) {
        QScriptEngine engine;
        const QScriptValue written = sampleProperties(type).copyToScriptValue(&engine, false);
        const QString json = QString::fromUtf8(QJsonDocument::fromVariant(written.toVariant()).toJson(QJsonDocument::Compact));
        QString where;
        QVERIFY2(sameReading(json, true, where), qPrintable(EntityTypes::getEntityTypeName(type) + ": " + where));
        QVERIFY2(sameReading(json, false, where), qPrintable(EntityTypes::getEntityTypeName(type) + " (read-only ignored): " + where));
    }
    // what a module writes, read the plain way, is what QtScript reads from the same object
    PlainScriptEngine plainEngine;
    const PlainScriptValue plain = sampleProperties(EntityTypes::Model).copyToScriptValue(&plainEngine, false);
    const PlainScriptValue back = PlainScriptValue::fromCbor(plain.toCbor());
    EntityItemProperties fromPlain;
    fromPlain.copyFromScriptValue(back, true);
    QScriptEngine engine;
    EntityItemProperties fromScript;
    fromScript.copyFromScriptValue(engine.toScriptValue(plain.toVariant()), true);
    fromPlain.setLastEdited(fromScript.getLastEdited());
    QVariantMap a = fromScript.copyToScriptValue(&engine, false, true).toVariant().toMap();
    QVariantMap b = fromPlain.copyToScriptValue(&engine, false, true).toVariant().toMap();
    stripClock(a);
    stripClock(b);
    QString where;
    QVERIFY2(sameVariant(a, b, where), qPrintable(where));
    QVERIFY(a.value("name").toString() == "sample" && a.value("modelURL").toString() == "https://example.invalid/model.glb");
    // scripts off (not the default) comes through both ways
    QVERIFY(a.contains("scriptsEnabled") && !a.value("scriptsEnabled").toBool() && !b.value("scriptsEnabled").toBool());
}

namespace {

// A car riding a track, with every vehicle property set to a value that is not its default (the wheels and seats as
// the lists WheeledVehicleSpec reads), and a Track with every track and train property set
const char* const VEHICLE_ON_A_TRACK = R"JSON({
    "type": "Box", "name": "a car on a track", "dynamic": true,
    "position": {"x": 1, "y": 2, "z": 3}, "dimensions": {"x": 2, "y": 1, "z": 4},
    "trackID": "{1a2b3c4d-0000-0000-0000-0000000000aa}", "trackOffset": 12.5,
    "vehicle": {
        "type": "car", "mass": 1200, "centerOfMass": {"x": 0, "y": -0.25, "z": 0.125},
        "wheels": "[{\"name\":\"frontLeft\",\"center\":[-0.8,-0.3,-1.3],\"radius\":0.35,\"width\":0.2,\"mass\":20,\"suspensionDistance\":0.3,\"targetPosition\":0.5,\"spring\":35000,\"damper\":4500,\"steer\":1,\"drive\":true,\"brake\":1,\"handBrake\":0,\"suspensionForcePoint\":false},{\"name\":\"rearRight\",\"center\":{\"x\":0.8,\"y\":-0.3,\"z\":1.3},\"radius\":0.35,\"steer\":0,\"drive\":true,\"handBrake\":1}]",
        "seats": "[{\"name\":\"driver\",\"position\":[-0.4,0.2,0],\"yaw\":0,\"driver\":true},{\"name\":\"passenger\",\"position\":{\"x\":0.4,\"y\":0.2,\"z\":0},\"yaw\":180}]",
        "maxSteerAngle": 35, "maxTorque": 650, "minRPM": 900, "maxRPM": 7000, "gearRatios": "[3.5,2.1,1.4,1.0,0.8]",
        "reverseGearRatio": 3.125, "finalDriveRatio": 3.875, "maxBrakeTorque": 1800, "maxHandBrakeTorque": 4200,
        "driveType": "all", "suspensionFrequency": 1.75, "suspensionDamping": 0.625, "wheel": "frontLeft", "axle": "z",
        "driver": "{1a2b3c4d-0000-0000-0000-0000000000bb}", "inputSequence": 42, "throttle": 0.5, "steering": -0.25,
        "brake": 0.125, "handBrake": 1, "engineRPM": 3200, "gear": 3, "stateTime": 123456789,
        "envelopeVolume": 5000, "centerOfBuoyancy": {"x": 0, "y": 1.5, "z": 0}, "gasDensity": 0.0625,
        "maxThrust": 20000, "maxVerticalThrust": 800, "thrustPoint": {"x": 0, "y": 0.25, "z": 0.5},
        "dragArea": {"x": 3, "y": 8, "z": 0.75}, "angularDragArea": {"x": 800, "y": 800, "z": 600}, "rudderArea": 2,
        "elevatorArea": 4, "finPoint": {"x": 0, "y": 0.5, "z": 4.5}, "maxFinAngle": 32, "ballastRate": 0.25,
        "ventRate": 0.375, "pitch": 0.125, "lift": 0.5, "trim": -0.0625, "ballast": 0.75, "fill": 0.875,
        "wingArea": 24, "wingSpan": 11, "wingPoint": {"x": 0, "y": 0.25, "z": 0.25}, "tilt": 1
    }
})JSON";

const char* const TRACK_WITH_A_TRAIN = R"JSON({
    "type": "Track", "name": "a track with a train",
    "position": {"x": 0, "y": 0, "z": 0}, "dimensions": {"x": 40, "y": 4, "z": 20},
    "trackPoints": [{"x": 0, "y": 0, "z": 0}, {"x": 10, "y": 1, "z": 0}, {"x": 20, "y": 0, "z": 5}, {"x": 10, "y": -1, "z": 10}],
    "trackBank": [0, 0.25, -0.125, 0], "trackDwell": [5, 0, 3, 0], "trackClosed": true,
    "trackSpeed": 7.5, "trackAcceleration": 1.25, "trainDistance": 33.5, "trainSpeed": 6.25, "trainReversed": true,
    "trainDwell": 2.5, "trainStation": 2, "trainTime": 987654321, "trackProfile": "none", "trackWidth": 0.75,
    "trackHeight": 0.5, "trackClearance": 1.25, "trackPylonSpacing": 12, "trackPylonFloor": -6
})JSON";

// The named properties of a writing, compared with another's
bool sameValues(const QVariantMap& expected, const QVariantMap& actual, const QStringList& names, QString& where) {
    for (const QString& name : names) {
        if (!expected.contains(name)) {
            where = name + " not written";
            return false;
        }
        if (!sameVariant(expected.value(name), actual.value(name), where)) {
            where = name + ": " + where;
            return false;
        }
    }
    return true;
}

}  // namespace

void EntityPropertiesConversionTests::testVehiclesAndTracks() {
    // what physics asked for (10-18): a round trip through copyToScriptValue and copyFromScriptValue, QtScript and
    // plain, identical, for an entity with every vehicle property set and a Track with its train properties
    struct Sample {
        const char* name;
        const char* json;
        QStringList properties;  // each set to a value that is not its default, each to come back
    };
    const Sample samples[] = {
        { "a car on a track", VEHICLE_ON_A_TRACK, { "trackID", "trackOffset", "vehicle" } },
        { "a track with a train", TRACK_WITH_A_TRAIN,
          { "trackPoints", "trackBank", "trackDwell", "trackClosed", "trackSpeed", "trackAcceleration", "trainDistance",
            "trainSpeed", "trainReversed", "trainDwell", "trainStation", "trainTime", "trackProfile", "trackWidth",
            "trackHeight", "trackClearance", "trackPylonSpacing", "trackPylonFloor" } },
    };
    const int vehicleProperties = EntityItemProperties::getPropertiesOfGroup("vehicle").size();
    QVERIFY(vehicleProperties > 0);
    for (const Sample& sample : samples) {
        const QString name(sample.name);
        QScriptEngine engine;
        PlainScriptEngine plainEngine;
        const QScriptValue input = engine.evaluate(QString("(") + sample.json + ")");
        QVERIFY2(!engine.hasUncaughtException(), qPrintable(name + ": " + engine.uncaughtException().toString()));
        EntityItemProperties properties;
        properties.copyFromScriptValue(input, false);
        properties.setCreated(usecTimestampNow() - USECS_PER_SECOND);

        // every property set: the writing without defaults carries each one, and all of the vehicle's
        const QVariantMap nonDefaults = properties.copyToScriptValue(&engine, true).toVariant().toMap();
        for (const QString& property : sample.properties) {
            QVERIFY2(nonDefaults.contains(property), qPrintable(name + ": " + property + " is at its default"));
        }
        if (sample.properties.contains("vehicle")) {
            const QVariantMap vehicle = nonDefaults.value("vehicle").toMap();
            QVERIFY2(vehicle.size() == vehicleProperties,
                     qPrintable(name + QString(": %1 of the %2 vehicle properties set").arg(vehicle.size()).arg(vehicleProperties)));
        }

        // written both ways, every way: the same
        QString where;
        QVERIFY2(sameWritingEveryWay(properties, where), qPrintable(name + ": " + where));

        // the round trip on each path: written and read back by QtScript, written and read back as plain values;
        // the same properties come back on both, and they are the values set
        const QScriptValue scriptWritten = properties.copyToScriptValue(&engine, false);
        const PlainScriptValue plainWritten = properties.copyToScriptValue(&plainEngine, false);
        for (bool honorReadOnly : { false, true }) {
            EntityItemProperties viaScript;
            EntityItemProperties viaPlain;
            viaScript.copyFromScriptValue(scriptWritten, honorReadOnly);
            viaPlain.copyFromScriptValue(plainWritten, honorReadOnly);
            viaPlain.setLastEdited(viaScript.getLastEdited());
            QVariantMap a = viaScript.copyToScriptValue(&engine, false, true).toVariant().toMap();
            QVariantMap b = viaPlain.copyToScriptValue(&engine, false, true).toVariant().toMap();
            stripClock(a);
            stripClock(b);
            const QString way = name + (honorReadOnly ? " (read-only honoured)" : "");
            QVERIFY2(sameVariant(a, b, where), qPrintable(way + ": " + where));
            QVERIFY2(sameValues(nonDefaults, a, sample.properties, where), qPrintable(way + ", the values back: " + where));
        }

        // the readers outside scripting (a world file's entity, avatar-entity data, a bookmark), a script's object
        // as text, and the world file's text
        const QVariantMap written = scriptWritten.toVariant().toMap();
        QVERIFY2(sameVariantReading(written, false, where), qPrintable(name + ", read as a world file reads it: " + where));
        QVERIFY2(sameVariantReading(written, true, where), qPrintable(name + ", read as a bookmark reads it: " + where));
        const QString json = QString::fromUtf8(QJsonDocument::fromVariant(written).toJson(QJsonDocument::Compact));
        QVERIFY2(sameReading(json, true, where), qPrintable(name + ", read as a script's object: " + where));
        QVERIFY2(sameWorldFileText(properties, true, where), qPrintable(name + ", the world file's text: " + where));
        QVERIFY2(sameWorldFileText(properties, false, where), qPrintable(name + ", the world file's text, defaults too: " + where));
    }
}

void EntityPropertiesConversionTests::testWorldFiles() {
    // whole worlds as an entity server keeps them (models.json.gz, or the JSON), named in SCRIPT_VALUES_TEST_WORLDS
    // (paths separated by ':'): every entity read as EntityTree::readFromMap reads one, and written as the world
    // file and an export write one, on QtScript and on the plain path, compared; skipped when the variable names none
    const QStringList paths = QString::fromLocal8Bit(qgetenv("SCRIPT_VALUES_TEST_WORLDS")).split(':', Qt::SkipEmptyParts);
    if (paths.isEmpty()) {
        QSKIP("SCRIPT_VALUES_TEST_WORLDS names no world files");
    }
    for (const QString& path : paths) {
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable("cannot open " + path));
        QByteArray data = file.readAll();
        if (path.endsWith(".gz")) {
            QByteArray unzipped;
            QVERIFY2(gunzip(data, unzipped), qPrintable("cannot unzip " + path));
            data = unzipped;
        }
        const QVariantList entities = QJsonDocument::fromJson(data).toVariant().toMap().value("Entities").toList();
        QVERIFY2(!entities.isEmpty(), qPrintable("no entities in " + path));
        QMap<QString, int> types;
        for (const QVariant& entity : entities) {
            const QVariantMap map = entity.toMap();
            const QString at = path + " " + map.value("id").toString() + ": ";
            QString where;
            // the load: read-only properties read too, as EntityItemPropertiesFromScriptValueIgnoreReadOnly does
            QVERIFY2(sameVariantReading(map, false, where), qPrintable(at + where));
            // the save: the properties read, written as the world file (without defaults) and as an export
            // (without defaults, or every property) write them
            EntityItemProperties properties;
            properties.copyFromScriptValue(PlainScriptValue::fromVariant(map), false);
            if (properties.getCreated() == UNKNOWN_CREATED_TIME) {
                properties.setCreated(usecTimestampNow() - USECS_PER_SECOND);
            }
            QVERIFY2(sameWorldFileText(properties, true, where), qPrintable(at + "the world file's text: " + where));
            QVERIFY2(sameWriting(properties, true, false, false, EntityPsuedoPropertyFlags(), where), qPrintable(at + "an export: " + where));
            QVERIFY2(sameWriting(properties, false, false, false, EntityPsuedoPropertyFlags(), where), qPrintable(at + "an export of every property: " + where));
            types[map.value("type").toString()]++;
        }
        QStringList counts;
        for (auto it = types.cbegin(); it != types.cend(); ++it) {
            counts << QString("%1 %2").arg(it.value()).arg(it.key());
        }
        qInfo().noquote() << path << "-" << entities.size() << "entities, the same on both paths:" << counts.join(", ");
    }
}

void EntityPropertiesConversionTests::benchmarkOverlayConversions_data() {
    QTest::addColumn<bool>("edit");
    QTest::addColumn<int>("way");
    QTest::newRow("an overlay edit, QtScript, an engine made per call") << true << 0;
    QTest::newRow("an overlay edit, QtScript, one engine kept") << true << 1;
    QTest::newRow("an overlay edit, plain values") << true << 2;
    QTest::newRow("an overlay read, QtScript, an engine made per call") << false << 0;
    QTest::newRow("an overlay read, QtScript, one engine kept") << false << 1;
    QTest::newRow("an overlay read, plain values") << false << 2;
}

void EntityPropertiesConversionTests::benchmarkOverlayConversions() {
    // the cost of what Overlays does for every addOverlay and editOverlay (an overlay's properties to an entity's)
    // and every getProperty (an entity's to an overlay's), in each of the three ways it has had: a QScriptEngine
    // made for each call, one kept per thread, and plain values with no engine
    QFETCH(bool, edit);
    QFETCH(int, way);
    QVariantMap overlay;
    overlay["type"] = "Shape";
    overlay["shape"] = "Cube";
    overlay["position"] = QVariantMap({ { "x", 1 }, { "y", 2.5 }, { "z", -3 } });
    overlay["rotation"] = QVariantMap({ { "x", 0 }, { "y", 0.7071 }, { "z", 0 }, { "w", 0.7071 } });
    overlay["dimensions"] = QVariantMap({ { "x", 0.1 }, { "y", 0.1 }, { "z", 0.1 } });
    overlay["color"] = QVariantMap({ { "red", 255 }, { "green", 128 }, { "blue", 0 } });
    overlay["alpha"] = 0.5;
    overlay["visible"] = true;
    overlay["ignorePickIntersection"] = true;
    overlay["parentID"] = "{1a2b3c4d-0000-0000-0000-000000000001}";
    const EntityItemProperties entity = sampleProperties(EntityTypes::Shape);
    QScriptEngine kept;
    int written = 0;
    QBENCHMARK {
        if (edit) {
            EntityItemProperties properties;
            if (way == 2) {
                properties.copyFromScriptValue(PlainScriptValue::fromVariant(overlay), true);
            } else {
                QScriptEngine perCall;
                QScriptEngine& engine = way == 1 ? kept : perCall;
                QVariantMap input = overlay;
                EntityItemPropertiesFromScriptValueHonorReadOnly(variantMapToScriptValue(input, engine), properties);
            }
            written += properties.getVisible() ? 1 : 0;
        } else {
            QVariantMap map;
            if (way == 2) {
                PlainScriptEngine plainEngine;
                map = entity.copyToScriptValue(&plainEngine, false).toVariant().toMap();
            } else {
                QScriptEngine perCall;
                QScriptEngine& engine = way == 1 ? kept : perCall;
                map = EntityItemPropertiesToScriptValue(&engine, entity).toVariant().toMap();
            }
            written += map.size();
        }
    }
    QVERIFY(written > 0);
}
