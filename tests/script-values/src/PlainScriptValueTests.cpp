//
//  PlainScriptValueTests.cpp
//  tests/script-values/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PlainScriptValueTests.h"

#include <cmath>
#include <limits>

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QPoint>
#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptValueIterator>

#include <PlainScriptValue.h>
#include <PlainScriptValueConversions.h>
#include <RegisteredMetaTypes.h>
#include <VariantMapToScriptValue.h>

#include "SameVariant.h"

QTEST_GUILESS_MAIN(PlainScriptValueTests)

namespace {

// A number set as a property on each engine's object and read back, the way the conversions make numbers.
QScriptValue scriptNumber(QScriptEngine& engine, double value) {
    QScriptValue object = engine.newObject();
    object.setProperty("v", QScriptValue(value));
    return object.property("v");
}

PlainScriptValue plainNumber(double value) {
    PlainScriptValue object = PlainScriptValue::newObject();
    object.setProperty("v", PlainScriptValue(value));
    return object.property("v");
}

const double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
const double INFINITY_VALUE = std::numeric_limits<double>::infinity();

}  // namespace

void PlainScriptValueTests::testNumbers() {
    QScriptEngine engine;
    const QList<double> numbers {
        0.0, -0.0, 1.0, -1.0, 1.5, -1.5, 0.1 + 0.2, 100.0, 2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0,
        4294967295.0, 4294967296.0, 123456789012.0, 1e21, 1e-7, 0.000001, 1e100, 5e-324, 1.7976931348623157e308,
        NAN_VALUE, INFINITY_VALUE, -INFINITY_VALUE
    };
    for (double number : numbers) {
        const QScriptValue script = scriptNumber(engine, number);
        const PlainScriptValue plain = plainNumber(number);
        const QString name = QString::number(number, 'g', 17);
        QString where;
        QVERIFY2(sameVariant(script.toVariant(), plain.toVariant(), where), qPrintable(name + ": " + where));
        QVERIFY2(script.toString() == plain.toString(),
                 qPrintable(name + ": text " + script.toString() + " vs " + plain.toString()));
        QCOMPARE(plain.toBool(), script.toBool());
        QCOMPARE(plain.toInt32(), script.toInt32());
        QCOMPARE(plain.toUInt32(), script.toUInt32());
        QCOMPARE(plain.toUInt16(), script.toUInt16());
        if (std::isnan(number)) {
            QVERIFY(std::isnan(plain.toNumber()) && std::isnan(script.toNumber()));
            QCOMPARE(plain.toInteger(), script.toInteger());
        } else {
            QCOMPARE(plain.toNumber(), script.toNumber());
            QCOMPARE(plain.toInteger(), script.toInteger());
        }
        QVERIFY(plain.isNumber() && !plain.isString() && !plain.isObject());
    }
    // the integer and unsigned constructors
    QCOMPARE(PlainScriptValue(7).toVariant().userType(), (int)QMetaType::Int);
    QCOMPARE(PlainScriptValue(7u).toVariant().userType(), (int)QMetaType::Int);
    QCOMPARE(PlainScriptValue(3000000000u).toVariant().userType(), (int)QMetaType::Double);
    QCOMPARE(PlainScriptValue(3000000000u).toVariant().toDouble(), 3000000000.0);
    QCOMPARE(PlainScriptValue(1.0f).toVariant().userType(), (int)QMetaType::Int);
    QCOMPARE(PlainScriptValue(0.5f).toVariant().userType(), (int)QMetaType::Double);
}

void PlainScriptValueTests::testNumberText() {
    // the specification's layout, by example
    QCOMPARE(PlainScriptValue::numberToString(1e21), QString("1e+21"));
    QCOMPARE(PlainScriptValue::numberToString(123456789012345680000.0), QString("123456789012345680000"));
    QCOMPARE(PlainScriptValue::numberToString(1e-7), QString("1e-7"));
    QCOMPARE(PlainScriptValue::numberToString(0.000001), QString("0.000001"));
    QCOMPARE(PlainScriptValue::numberToString(0.1 + 0.2), QString("0.30000000000000004"));
    QCOMPARE(PlainScriptValue::numberToString(-0.0), QString("0"));
    QCOMPARE(PlainScriptValue::numberToString(-1.5e-10), QString("-1.5e-10"));
    QCOMPARE(PlainScriptValue::numberToString(1.7976931348623157e308), QString("1.7976931348623157e+308"));
    QCOMPARE(PlainScriptValue::numberToString(5e-324), QString("5e-324"));
    QCOMPARE(PlainScriptValue::numberToString(4294967295.0), QString("4294967295"));
    QCOMPARE(PlainScriptValue::numberToString(1.0), QString("1"));
    QCOMPARE(PlainScriptValue::numberToString(12.5), QString("12.5"));
    // and against QtScript's own, over a spread of magnitudes
    QScriptEngine engine;
    for (int exponent = -12; exponent <= 24; ++exponent) {
        for (double mantissa : { 1.0, 1.5, 1.25, 9.999, 3.14159265358979, 7.0 }) {
            const double value = mantissa * std::pow(10.0, exponent);
            const QString expected = scriptNumber(engine, value).toString();
            const QString actual = PlainScriptValue::numberToString(value);
            QVERIFY2(actual == expected, qPrintable(QString("%1: %2 vs %3").arg(value, 0, 'g', 17).arg(actual).arg(expected)));
        }
    }
}

void PlainScriptValueTests::testStrings() {
    QScriptEngine engine;
    const QStringList texts { "", " ", "12", " 12 ", "1.5", "-1.5", "abc", "0x10", "1e3", "Infinity", "-0", "1.", ".5",
                              "12abc", "true", "{00000000-0000-0000-0000-000000000000}" };
    for (const QString& text : texts) {
        const QScriptValue script(text);
        const PlainScriptValue plain(text);
        const QString name = "'" + text + "'";
        QString where;
        QVERIFY2(sameVariant(script.toVariant(), plain.toVariant(), where), qPrintable(name + ": " + where));
        QCOMPARE(plain.toString(), script.toString());
        QCOMPARE(plain.toBool(), script.toBool());
        if (std::isnan(script.toNumber())) {
            QVERIFY2(std::isnan(plain.toNumber()), qPrintable(name + ": NaN expected"));
        } else {
            QVERIFY2(plain.toNumber() == script.toNumber(),
                     qPrintable(name + QString(": %1 vs %2").arg(plain.toNumber()).arg(script.toNumber())));
        }
        QCOMPARE(plain.toInt32(), script.toInt32());
        QCOMPARE(plain.toUInt32(), script.toUInt32());
        QVERIFY(plain.isString() && !plain.isNumber() && !plain.isObject());
    }
    // the other primitives and the absent value
    QVERIFY(!PlainScriptValue().isValid());
    QVERIFY(PlainScriptValue().toVariant().isNull() && !QScriptValue().toVariant().isValid());
    QCOMPARE(PlainScriptValue().toString(), QScriptValue().toString());
    QCOMPARE(PlainScriptValue().toNumber(), QScriptValue().toNumber());
    QCOMPARE(PlainScriptValue::undefinedValue().toString(), engine.undefinedValue().toString());
    QVERIFY(std::isnan(PlainScriptValue::undefinedValue().toNumber()) && std::isnan(engine.undefinedValue().toNumber()));
    QVERIFY(!PlainScriptValue::undefinedValue().toVariant().isValid() && !engine.undefinedValue().toVariant().isValid());
    QCOMPARE(PlainScriptValue::nullValue().toString(), engine.nullValue().toString());
    QCOMPARE(PlainScriptValue::nullValue().toNumber(), engine.nullValue().toNumber());
    QVERIFY(!PlainScriptValue::nullValue().toVariant().isValid() && !engine.nullValue().toVariant().isValid());
    QCOMPARE(PlainScriptValue(true).toString(), QScriptValue(true).toString());
    QCOMPARE(PlainScriptValue(false).toNumber(), QScriptValue(false).toNumber());
    QCOMPARE(PlainScriptValue(true).toVariant(), QScriptValue(true).toVariant());
    QCOMPARE(PlainScriptValue("text").toVariant(), QScriptValue("text").toVariant());
}

void PlainScriptValueTests::testObjects() {
    QScriptEngine engine;
    QScriptValue script = engine.newObject();
    PlainScriptValue plain = PlainScriptValue::newObject();
    // the same properties set in the same order, with every kind of value, a nested object and a replacement
    script.setProperty("name", "box");
    plain.setProperty("name", "box");
    script.setProperty("count", 3);
    plain.setProperty("count", 3);
    script.setProperty("scale", 0.5);
    plain.setProperty("scale", 0.5);
    script.setProperty("visible", true);
    plain.setProperty("visible", true);
    script.setProperty("nothing", engine.nullValue());
    plain.setProperty("nothing", PlainScriptValue::nullValue());
    script.setProperty("unset", engine.undefinedValue());
    plain.setProperty("unset", PlainScriptValue::undefinedValue());
    QScriptValue scriptInner = engine.newObject();
    PlainScriptValue plainInner = PlainScriptValue::newObject();
    scriptInner.setProperty("x", 1);
    plainInner.setProperty("x", 1);
    scriptInner.setProperty("y", 2.5);
    plainInner.setProperty("y", 2.5);
    script.setProperty("position", scriptInner);
    plain.setProperty("position", plainInner);
    script.setProperty("count", 4);  // set again: the value changes, the place stays
    plain.setProperty("count", 4);
    script.setProperty("gone", "for now");
    plain.setProperty("gone", "for now");
    script.setProperty("gone", QScriptValue());  // an invalid value removes the property
    plain.setProperty("gone", PlainScriptValue());

    QString where;
    QVERIFY2(sameVariant(script.toVariant(), plain.toVariant(), where), qPrintable(where));
    // the order of the properties, as an iterator and the plain names give it
    QStringList scriptNames;
    QScriptValueIterator it(script);
    while (it.hasNext()) {
        it.next();
        scriptNames.append(it.name());
    }
    QCOMPARE(plain.propertyNames(), scriptNames);
    QCOMPARE(plain.propertyNames(), QStringList({ "name", "count", "scale", "visible", "nothing", "unset", "position" }));
    // reading: present, absent, through a non-object, a nested property
    QVERIFY(plain.property("name").isValid() && script.property("name").isValid());
    QVERIFY(!plain.property("absent").isValid() && !script.property("absent").isValid());
    QVERIFY(!plain.property("gone").isValid() && !script.property("gone").isValid());
    QVERIFY(plain.property("nothing").isValid() && script.property("nothing").isValid());
    QVERIFY(plain.property("nothing").isNull() && script.property("nothing").isNull());
    QVERIFY(plain.property("unset").isValid() && script.property("unset").isValid());
    QVERIFY(plain.property("unset").isUndefined() && script.property("unset").isUndefined());
    QVERIFY(!plain.property("count").property("x").isValid() && !script.property("count").property("x").isValid());
    QVERIFY(!PlainScriptValue("text").property("length").isValid() && !QScriptValue("text").property("length").isValid());
    QCOMPARE(plain.property("position").property("y").toNumber(), script.property("position").property("y").toNumber());
    QCOMPARE(plain.property("count").toVariant(), script.property("count").toVariant());
    QVERIFY(plain.isObject() && !plain.isArray() && script.isObject() && !script.isArray());
    QCOMPARE(plain.toString(), script.toString());
    QCOMPARE(plain.toBool(), script.toBool());
    QVERIFY(std::isnan(plain.toNumber()) && std::isnan(script.toNumber()));
    // setting on a non-object does nothing
    PlainScriptValue number(5);
    number.setProperty("x", 1);
    QVERIFY(!number.property("x").isValid() && number.isNumber());
}

void PlainScriptValueTests::testArrays() {
    QScriptEngine engine;
    QScriptValue script = engine.newArray();
    PlainScriptValue plain = PlainScriptValue::newArray();
    script.setProperty(0, 1);
    plain.setProperty(0, 1);
    script.setProperty(1, "two");
    plain.setProperty(1, "two");
    script.setProperty(2, 2.5);
    plain.setProperty(2, 2.5);
    script.setProperty(4, true);  // index 3 is a hole
    plain.setProperty(4, true);
    QScriptValue scriptInner = engine.newObject();
    PlainScriptValue plainInner = PlainScriptValue::newObject();
    scriptInner.setProperty("x", 0);
    plainInner.setProperty("x", 0);
    script.setProperty(5, scriptInner);
    plain.setProperty(5, plainInner);

    QString where;
    QVERIFY2(sameVariant(script.toVariant(), plain.toVariant(), where), qPrintable(where));
    QCOMPARE(plain.property("length").toInteger(), script.property("length").toInteger());
    QCOMPARE(plain.property("length").toInteger(), 6.0);
    QVERIFY(!plain.property(3).isValid() && !script.property(3).isValid());
    QVERIFY(!plain.property(9).isValid() && !script.property(9).isValid());
    QVERIFY(!plain.property("3").isValid() && !script.property("3").isValid());
    QCOMPARE(plain.property("1").toString(), script.property("1").toString());
    QCOMPARE(plain.property(2).toNumber(), script.property(2).toNumber());
    QVERIFY(plain.isArray() && plain.isObject() && script.isArray() && script.isObject());
    QCOMPARE(plain.toString(), script.toString());
    QCOMPARE(plain.propertyNames(), QStringList({ "0", "1", "2", "4", "5" }));
    QVERIFY(plain.toVariant().userType() == QMetaType::QVariantList && script.toVariant().userType() == QMetaType::QVariantList);
    // the length set shorter drops the elements beyond it
    script.setProperty("length", 2);
    plain.setProperty("length", 2);
    QVERIFY2(sameVariant(script.toVariant(), plain.toVariant(), where), qPrintable(where));
    QCOMPARE(plain.property("length").toInteger(), 2.0);
    // a one-element array reads as its element's text, an empty one as nothing
    PlainScriptValue single = PlainScriptValue::newArray();
    single.setProperty(0, 7);
    QCOMPARE(single.toString(), QString("7"));
    QCOMPARE(single.toNumber(), 7.0);
    QCOMPARE(PlainScriptValue::newArray().toString(), QString());
    QCOMPARE(PlainScriptValue::newArray().toNumber(), 0.0);
}

void PlainScriptValueTests::testSharing() {
    // a property set through a copy is seen through the original, as with a QScriptValue
    PlainScriptValue object = PlainScriptValue::newObject();
    PlainScriptValue group = PlainScriptValue::newObject();
    object.setProperty("group", group);
    PlainScriptValue again = object.property("group");
    again.setProperty("inner", 1);
    QCOMPARE(object.property("group").property("inner").toInteger(), 1.0);
    QCOMPARE(group.property("inner").toInteger(), 1.0);
    object.setProperty("group", again);  // set again, as the group macros do: the same object, the same place
    QCOMPARE(object.propertyNames(), QStringList({ "group" }));
    QCOMPARE(object.property("group").propertyNames(), QStringList({ "inner" }));
}

void PlainScriptValueTests::testCbor() {
    PlainScriptValue object = PlainScriptValue::newObject();
    object.setProperty("z", 1);
    object.setProperty("a", 2.5);
    object.setProperty("m", "text");
    object.setProperty("t", true);
    object.setProperty("n", PlainScriptValue::nullValue());
    PlainScriptValue list = PlainScriptValue::newArray();
    list.setProperty(0, 1);
    list.setProperty(1, PlainScriptValue::newObject());
    object.setProperty("list", list);
    object.setProperty("big", 3000000000.0);

    // CBOR keeps the order the properties were set in; a variant map sorts them
    const QCborValue cbor = object.toCbor();
    QVERIFY(cbor.isMap());
    QStringList keys;
    for (const auto& pair : cbor.toMap()) {
        keys.append(pair.first.toString());
    }
    QCOMPARE(keys, QStringList({ "z", "a", "m", "t", "n", "list", "big" }));
    QCOMPARE(object.toVariant().toMap().keys(), QStringList({ "a", "big", "list", "m", "n", "t", "z" }));
    QVERIFY(cbor.toMap().value("z").isInteger());
    QVERIFY(cbor.toMap().value("a").isDouble());
    QVERIFY(cbor.toMap().value("big").isDouble());
    QVERIFY(cbor.toMap().value("n").isNull());

    // and reads back as it was, value, type and order
    const PlainScriptValue back = PlainScriptValue::fromCbor(cbor);
    QString where;
    QVERIFY2(sameVariant(object.toVariant(), back.toVariant(), where), qPrintable(where));
    QCOMPARE(back.propertyNames(), object.propertyNames());
    QVERIFY(back.property("n").isNull());
    QVERIFY(back.property("list").isArray());
    QCOMPARE(back.property("list").property("length").toInteger(), 2.0);

    // what a module may send: a tagged id, bytes, a large integer, an undefined member
    QCborMap sent;
    sent.insert(QString("id"), QCborValue(QUuid("{1a2b3c4d-0000-0000-0000-000000000001}")));
    sent.insert(QString("bytes"), QCborValue(QByteArray("abc")));
    sent.insert(QString("huge"), QCborValue(qint64(1) << 40));
    sent.insert(QString("none"), QCborValue(QCborValue::Undefined));
    sent.insert(QCborValue(7), QCborValue("seven"));
    const PlainScriptValue received = PlainScriptValue::fromCbor(sent);
    QCOMPARE(received.property("id").toString(), QString("{1a2b3c4d-0000-0000-0000-000000000001}"));
    QCOMPARE(received.property("bytes").toString(), QString("YWJj"));
    QCOMPARE(received.property("huge").toVariant().userType(), (int)QMetaType::Double);
    QCOMPARE(received.property("huge").toNumber(), 1099511627776.0);
    QVERIFY(received.property("none").isUndefined());
    QCOMPARE(received.property("7").toString(), QString("seven"));
    QCOMPARE(received.propertyNames(), QStringList({ "id", "bytes", "huge", "none", "7" }));
}

void PlainScriptValueTests::testFromVariant() {
    QScriptEngine engine;
    // Every kind of QVariant a world file, avatar-entity data or an overlay's properties hold, and some that
    // none should, through variantToScriptValue, which the QtScript callers outside scripting use, and through
    // fromVariant: the same variant back, the same members in the same order, the same members left out
    QVariantMap nested;
    nested.insert("x", 1.0);
    nested.insert("y", 2.5);
    nested.insert("z", -3);
    const QVariantList list { 1, 2.5, "three", true, QVariantMap(), QVariantList(), nested };
    const QVariantList holes { 1, QVariant(), 3, QVariant::fromValue(QPoint(1, 2)), QVariant() };
    QVariantMap map;
    map.insert("bool", true);
    map.insert("int", 7);
    map.insert("double", 2.5);
    map.insert("integral", 3.0);
    map.insert("negativeZero", -0.0);
    map.insert("huge", 1e21);
    map.insert("float", QVariant::fromValue(0.1f));
    map.insert("uint", QVariant::fromValue(3000000000u));
    map.insert("longLong", QVariant::fromValue(qint64(123456789012345)));
    map.insert("uLongLong", QVariant::fromValue(quint64(18446744073709551615ULL)));
    map.insert("string", "text");
    map.insert("url", QUrl("https://example.invalid/a?b=c"));
    map.insert("bytes", QByteArray("12.5"));
    map.insert("letters", QByteArray("abc"));
    map.insert("char", QChar('A'));
    map.insert("uuid", QUuid("{1a2b3c4d-0000-0000-0000-000000000001}"));
    map.insert("dateTime", QDateTime::currentDateTime());
    map.insert("point", QPoint(1, 2));
    map.insert("null", QVariant());
    map.insert("nested", nested);
    map.insert("list", list);
    map.insert("holes", holes);
    map.insert("empty", QVariantMap());

    QVariant variant(map);
    const QScriptValue script = variantToScriptValue(variant, engine);
    const PlainScriptValue plain = PlainScriptValue::fromVariant(map);
    QString where;
    QVERIFY2(sameVariant(script.toVariant(), plain.toVariant(), where), qPrintable(where));
    QStringList scriptNames;
    QScriptValueIterator it(script);
    while (it.hasNext()) {
        it.next();
        scriptNames.append(it.name());
    }
    QCOMPARE(plain.propertyNames(), scriptNames);
    for (const QString& name : map.keys()) {
        QVERIFY2(plain.property(name).isValid() == script.property(name).isValid(), qPrintable(name));
    }
    QVERIFY(!plain.property("null").isValid());
    QVERIFY(!plain.property("point").isValid());
    // a 64-bit integer goes through a float on both paths (variantToScriptValue's rule for any other type), and
    // QJsonDocument gives one for a whole number: an entity's creation time loses its low bits on a world load
    QCOMPARE(plain.property("longLong").toNumber(), (double)(float)123456789012345);
    const QVariantMap fromJson = QJsonDocument::fromJson("{\"created\": 1792340679910596}").toVariant().toMap();
    QCOMPARE(fromJson.value("created").userType(), (int)QMetaType::LongLong);
    QVariant createdVariant = fromJson.value("created");
    QCOMPARE(variantToScriptValue(createdVariant, engine).toNumber(), (double)(float)1792340679910596);
    QCOMPARE(PlainScriptValue::fromVariant(createdVariant).toNumber(), (double)(float)1792340679910596);
    QCOMPARE(plain.property("uint").toNumber(), 3000000000.0);
    QCOMPARE(plain.property("url").toString(), QString("https://example.invalid/a?b=c"));
    QVERIFY(plain.property("list").isArray());
    QCOMPARE(plain.property("list").property("length").toInteger(), script.property("list").property("length").toInteger());
    QCOMPARE(plain.property("holes").property("length").toInteger(), script.property("holes").property("length").toInteger());
    QCOMPARE(plain.property("holes").property("length").toInteger(), 3.0);  // the elements never set do not count
    QVERIFY(!plain.property("holes").property(1).isValid());

    // Overlays.cpp read a PolyLine's points and widths through QScriptEngine::newVariant: a variant object, not
    // an array, whose length is undefined, so both lists always read as empty. The plain path reads them.
    const QVariantList points { nested, nested };
    const QVariantList widths { 0.1, 0.2 };
    const QScriptValue wrappedPoints = engine.newVariant(QVariant(points));
    QVERIFY(wrappedPoints.isVariant());
    QVERIFY(!wrappedPoints.isArray());
    QVERIFY(!wrappedPoints.property("length").isValid());
    QCOMPARE(qVectorVec3FromScriptValue(wrappedPoints).size(), 0);
    QCOMPARE(qVectorFloatFromScriptValue(engine.newVariant(QVariant(widths))).size(), 0);
    QVariant pointsVariant(points);
    QCOMPARE(qVectorVec3FromScriptValue(variantToScriptValue(pointsVariant, engine)).size(), 2);
    QCOMPARE(qVectorVec3FromScriptValue(PlainScriptValue::fromVariant(points)).size(), 2);
    QCOMPARE(qVectorVec3FromScriptValue(PlainScriptValue::fromVariant(points)).at(1), glm::vec3(1.0f, 2.5f, -3.0f));
    QCOMPARE(qVectorFloatFromScriptValue(PlainScriptValue::fromVariant(widths)), QVector<float>({ 0.1f, 0.2f }));
}

namespace {

// The same value on QtScript, kind for kind and in the same order
QScriptValue mirror(QScriptEngine& engine, const PlainScriptValue& plain) {
    if (plain.isUndefined() || !plain.isValid()) {
        return QScriptValue(QScriptValue::UndefinedValue);
    }
    if (plain.isNull()) {
        return QScriptValue(QScriptValue::NullValue);
    }
    if (plain.isBool()) {
        return QScriptValue(plain.toBool());
    }
    if (plain.isNumber()) {
        return QScriptValue(plain.toNumber());
    }
    if (plain.isString()) {
        return QScriptValue(plain.toString());
    }
    if (plain.isArray()) {
        QScriptValue array = engine.newArray();
        const quint32 length = plain.property("length").toUInt32();
        for (quint32 i = 0; i < length; ++i) {
            if (plain.property(i).isValid()) {
                array.setProperty(i, mirror(engine, plain.property(i)));
            }
        }
        array.setProperty("length", QScriptValue(length));
        return array;
    }
    QScriptValue object = engine.newObject();
    for (const QString& name : plain.propertyNames()) {
        object.setProperty(name, mirror(engine, plain.property(name)));
    }
    return object;
}

QString stringify(QScriptEngine& engine, const QScriptValue& value, const QString& indent) {
    QScriptValue json = engine.globalObject().property("JSON");
    QScriptValue text = json.property("stringify").call(json, QScriptValueList() << value << QScriptValue(QScriptValue::NullValue) << QScriptValue(indent));
    return text.isUndefined() ? QString("undefined") : text.toString();
}

}  // namespace

void PlainScriptValueTests::testJson() {
    QScriptEngine engine;
    PlainScriptValue object = PlainScriptValue::newObject();
    object.setProperty("id", "{1a2b3c4d-0000-0000-0000-000000000001}");
    object.setProperty("type", "Box");
    object.setProperty("lastEdited", 1792340679910596.0);
    object.setProperty("name", "quotes \" and backslash \\ and slash / and tab \t newline \n cr \r bs \b ff \f bell \x07 esc \x1b del \x7f");
    object.setProperty("accents", QString::fromUtf8("caf\xc3\xa9 \xe2\x80\xa8 \xf0\x9f\x8c\xb4 \xe2\x82\xac"));
    object.setProperty("empty", "");
    PlainScriptValue position = PlainScriptValue::newObject();
    position.setProperty("x", -2.9272141456604004);
    position.setProperty("y", 0.20000000298023224);
    position.setProperty("z", -0.0000152587890625);
    object.setProperty("position", position);
    PlainScriptValue numbers = PlainScriptValue::newArray();
    const QList<double> values { 0.0, -0.0, 1.0, -1.0, 1.5, 100.0, 2147483648.0, 1e21, 1e-7, 0.000001, 1e100, 5e-324,
        std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };
    for (int i = 0; i < values.size(); ++i) {
        numbers.setProperty(i, values.at(i));
    }
    object.setProperty("numbers", numbers);
    PlainScriptValue mixed = PlainScriptValue::newArray();
    mixed.setProperty(0, true);
    mixed.setProperty(1, PlainScriptValue::nullValue());
    mixed.setProperty(2, PlainScriptValue::undefinedValue());
    mixed.setProperty(4, "after a hole");
    mixed.setProperty(5, PlainScriptValue::newObject());
    mixed.setProperty(6, PlainScriptValue::newArray());
    object.setProperty("mixed", mixed);
    object.setProperty("undefinedProperty", PlainScriptValue::undefinedValue());
    object.setProperty("nullProperty", PlainScriptValue::nullValue());
    object.setProperty("false", false);
    object.setProperty("emptyObject", PlainScriptValue::newObject());
    object.setProperty("emptyArray", PlainScriptValue::newArray());
    PlainScriptValue nested = PlainScriptValue::newObject();
    PlainScriptValue deeper = PlainScriptValue::newArray();
    deeper.setProperty(0, PlainScriptValue::newObject());
    deeper.property(0).setProperty("k", "v");
    nested.setProperty("list", deeper);
    nested.setProperty("gone", PlainScriptValue::undefinedValue());
    object.setProperty("nested", nested);
    object.setProperty("key with \"quotes\"", 1);

    const QScriptValue script = mirror(engine, object);
    for (const QString& indent : { QString(), QString("    "), QString("\t"), QString("  ") }) {
        const QString expected = stringify(engine, script, indent);
        const QString actual = object.toJson(indent);
        QVERIFY2(actual == expected, qPrintable("indent '" + indent + "':\n" + actual + "\nvs QtScript\n" + expected));
    }
    // the parts on their own
    QCOMPARE(PlainScriptValue("a\"b").toJson(), QString("\"a\\\"b\""));
    QCOMPARE(PlainScriptValue(1e21).toJson(), QString("1e+21"));
    QCOMPARE(PlainScriptValue(-0.0).toJson(), QString("0"));
    QCOMPARE(PlainScriptValue(std::numeric_limits<double>::quiet_NaN()).toJson(), QString("null"));
    QCOMPARE(PlainScriptValue::nullValue().toJson(), QString("null"));
    QCOMPARE(PlainScriptValue::undefinedValue().toJson(), QString());
    QCOMPARE(PlainScriptValue::newObject().toJson("    "), QString("{}"));
    QCOMPARE(PlainScriptValue::newArray().toJson("    "), QString("[]"));
    QCOMPARE(stringify(engine, mirror(engine, PlainScriptValue::undefinedValue()), QString()), QString("undefined"));
}
