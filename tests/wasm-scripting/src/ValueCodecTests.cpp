//
//  ValueCodecTests.cpp
//  tests/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ValueCodecTests.h"

#include <limits>

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtGui/QColor>

#include <ObjectTable.h>
#include <ValueCodec.h>

#include "TestObjects.h"

QTEST_MAIN(ValueCodecTests)

namespace {

struct Opaque {
    int value { 0 };
};

struct Unknown {
    int value { 0 };
};

QVariant roundTrip(const ValueCodec& codec, const QVariant& value, int targetType = QMetaType::UnknownType) {
    QVariant out;
    QString error;
    if (!codec.decode(codec.encode(value), targetType, out, error)) {
        qWarning() << "decode failed:" << error;
    }
    return out;
}

} // namespace

Q_DECLARE_METATYPE(Opaque)
Q_DECLARE_METATYPE(Unknown)

namespace {

// An enum and an object pointer type the codec handles by itself, until a registered conversion says otherwise.
enum class Shade { Light = 1, Dark = 2 };

} // namespace

Q_DECLARE_METATYPE(Shade)
Q_DECLARE_METATYPE(QPointer<Probe>)
Q_DECLARE_METATYPE(QSharedPointer<Probe>)

void ValueCodecTests::testPrimitives() {
    ValueCodec codec;
    QCOMPARE(roundTrip(codec, QVariant(true)), QVariant(true));
    QCOMPARE(roundTrip(codec, QVariant(false)), QVariant(false));
    QCOMPARE(roundTrip(codec, QVariant(42)), QVariant(42));
    QCOMPARE(roundTrip(codec, QVariant(-7)), QVariant(-7));
    QCOMPARE(roundTrip(codec, QVariant(3000000000u)).toLongLong(), 3000000000LL);
    QCOMPARE(roundTrip(codec, QVariant(qlonglong(1) << 40)), QVariant(qlonglong(1) << 40));
    QCOMPARE(roundTrip(codec, QVariant(1.5)), QVariant(1.5));
    QCOMPARE(roundTrip(codec, QVariant(0.25f)), QVariant(0.25));
    QCOMPARE(roundTrip(codec, QVariant(QString("héllo"))), QVariant(QString("héllo")));
    QCOMPARE(roundTrip(codec, QVariant(QString())), QVariant(QString()));
    QCOMPARE(roundTrip(codec, QVariant(QByteArray("\x00\x01\xff", 3))), QVariant(QByteArray("\x00\x01\xff", 3)));
    // an invalid QVariant is undefined, a null pointer is null, and they come back apart
    QCOMPARE(codec.toCbor(QVariant()), QCborValue(QCborValue::Undefined));
    QCOMPARE(codec.toCbor(QVariant::fromValue(nullptr)), QCborValue(nullptr));
    QVERIFY(!roundTrip(codec, QVariant()).isValid());
    QCOMPARE(roundTrip(codec, QVariant::fromValue(nullptr)).userType(), int(QMetaType::Nullptr));
    // a large unsigned value survives as a double
    QCOMPARE(roundTrip(codec, QVariant(qulonglong(1) << 63)).toDouble(), double(qulonglong(1) << 63));
}

void ValueCodecTests::testBytesAreCanonical() {
    ValueCodec codec;
    QVariantMap map;
    map["x"] = 1;
    QCOMPARE(codec.encode(map), QByteArray("\xa1\x61x\x01", 4));
    QCOMPARE(codec.encode(QVariant(QString("ab"))), QByteArray("\x62" "ab", 3));
    QCOMPARE(codec.encode(QVariant(true)), QByteArray("\xf5", 1));
    QCOMPARE(codec.encode(QVariant(-1)), QByteArray("\x20", 1));
}

void ValueCodecTests::testUuidAndUrl() {
    ValueCodec codec;
    QUuid uuid = QUuid::createUuid();
    QCOMPARE(codec.toCbor(QVariant(uuid)), QCborValue(uuid.toString()));
    QVERIFY(codec.toCbor(QVariant(uuid)).toString().startsWith('{'));
    QCOMPARE(codec.toCbor(QVariant(QUuid())), QCborValue(nullptr));
    // typed decoding takes the text with or without braces, and null as the null uuid
    QCOMPARE(codec.fromCbor(QCborValue(uuid.toString()), QMetaType::QUuid).toUuid(), uuid);
    QCOMPARE(codec.fromCbor(QCborValue(uuid.toString(QUuid::WithoutBraces)), QMetaType::QUuid).toUuid(), uuid);
    QCOMPARE(codec.fromCbor(QCborValue(nullptr), QMetaType::QUuid).toUuid(), QUuid());
    bool ok = true;
    QCOMPARE(codec.fromCbor(QCborValue(12), QMetaType::QUuid, &ok).toUuid(), QUuid());
    QVERIFY(!ok);
    // untyped: a string stays a string, which is what a script compares against
    QCOMPARE(roundTrip(codec, QVariant(uuid)).userType(), int(QMetaType::QString));

    QUrl url("https://example.com/a path?q=1");
    QCOMPARE(codec.toCbor(QVariant(url)), QCborValue(url.toString()));
    QCOMPARE(codec.fromCbor(QCborValue(url.toString()), QMetaType::QUrl).toUrl(), url);
}

void ValueCodecTests::testVectorTypes() {
    ValueCodec codec;
    glm::vec3 v3(1.5f, -2.0f, 3.25f);
    QCborValue cbor = codec.toCbor(QVariant::fromValue(v3));
    QVERIFY(cbor.isMap());
    QCOMPARE(cbor.toMap().size(), qsizetype(3));
    QCOMPARE(cbor.toMap().value("x").toDouble(), 1.5);
    QCOMPARE(cbor.toMap().value("y").toDouble(), -2.0);
    QCOMPARE(cbor.toMap().value("z").toDouble(), 3.25);
    QCOMPARE(roundTrip(codec, QVariant::fromValue(v3), qMetaTypeId<glm::vec3>()).value<glm::vec3>(), v3);

    glm::quat q = glm::normalize(glm::quat(0.9f, 0.1f, 0.2f, 0.3f));
    QCborMap quatMap = codec.toCbor(QVariant::fromValue(q)).toMap();
    QCOMPARE(quatMap.size(), qsizetype(4));
    QCOMPARE(float(quatMap.value("w").toDouble()), q.w);
    glm::quat back = roundTrip(codec, QVariant::fromValue(q), qMetaTypeId<glm::quat>()).value<glm::quat>();
    QVERIFY(glm::length(back - q) < 1e-6f);

    glm::vec2 v2(0.5f, 0.75f);
    QCOMPARE(codec.toCbor(QVariant::fromValue(v2)).toMap().size(), qsizetype(2));
    QCOMPARE(roundTrip(codec, QVariant::fromValue(v2), qMetaTypeId<glm::vec2>()).value<glm::vec2>(), v2);

    glm::vec4 v4(1.0f, 2.0f, 3.0f, 4.0f);
    QCOMPARE(codec.toCbor(QVariant::fromValue(v4)).toMap().value("w").toDouble(), 4.0);
    QCOMPARE(roundTrip(codec, QVariant::fromValue(v4), qMetaTypeId<glm::vec4>()).value<glm::vec4>(), v4);

    glm::u8vec3 color(10, 20, 30);
    QCborMap colorMap = codec.toCbor(QVariant::fromValue(color)).toMap();
    QCOMPARE(colorMap.value("red").toInteger(), qint64(10));
    QCOMPARE(colorMap.value("green").toInteger(), qint64(20));
    QCOMPARE(colorMap.value("blue").toInteger(), qint64(30));
    QCOMPARE(roundTrip(codec, QVariant::fromValue(color), qMetaTypeId<glm::u8vec3>()).value<glm::u8vec3>(), color);

    glm::mat4 m(1.0f);
    m[3][0] = 5.0f;  // a translation
    m[1][2] = 0.5f;
    QCborMap matMap = codec.toCbor(QVariant::fromValue(m)).toMap();
    QCOMPARE(matMap.size(), qsizetype(16));
    QCOMPARE(matMap.value("r0c3").toDouble(), 5.0);  // row 0, column 3: the x translation
    QCOMPARE(matMap.value("r2c1").toDouble(), 0.5);
    QCOMPARE(roundTrip(codec, QVariant::fromValue(m), qMetaTypeId<glm::mat4>()).value<glm::mat4>(), m);
    // the column-major array form is accepted too
    QCborArray flat;
    for (int i = 0; i < 16; ++i) {
        flat.append(double(m[i / 4][i % 4]));
    }
    QCOMPARE(codec.fromCbor(flat, qMetaTypeId<glm::mat4>()).value<glm::mat4>(), m);
}

void ValueCodecTests::testVectorCoercions() {
    ValueCodec codec;
    const int vec3Type = qMetaTypeId<glm::vec3>();
    // a number fills every component, as vec3FromScriptValue does
    QCOMPARE(codec.fromCbor(QCborValue(2.0), vec3Type).value<glm::vec3>(), glm::vec3(2.0f));
    // an array of three
    QCOMPARE(codec.fromCbor(QCborArray { 1, 2, 3 }, vec3Type).value<glm::vec3>(), glm::vec3(1.0f, 2.0f, 3.0f));
    // r, g, b and red, green, blue name the same components
    QCborMap rgb;
    rgb.insert(QLatin1String("r"), 1);
    rgb.insert(QLatin1String("g"), 2);
    rgb.insert(QLatin1String("b"), 3);
    QCOMPARE(codec.fromCbor(rgb, vec3Type).value<glm::vec3>(), glm::vec3(1.0f, 2.0f, 3.0f));
    QCborMap red;
    red.insert(QLatin1String("red"), 255);
    red.insert(QLatin1String("green"), 0);
    red.insert(QLatin1String("blue"), 128);
    QCOMPARE(codec.fromCbor(red, vec3Type).value<glm::vec3>(), glm::vec3(255.0f, 0.0f, 128.0f));
    // a colour name
    QCOMPARE(codec.fromCbor(QCborValue("red"), vec3Type).value<glm::vec3>(), glm::vec3(255.0f, 0.0f, 0.0f));
    bool ok = true;
    codec.fromCbor(QCborValue("no such colour"), vec3Type, &ok);
    QVERIFY(!ok);
    ok = true;
    codec.fromCbor(QCborArray { 1, 2 }, vec3Type, &ok);
    QVERIFY(!ok);
    // missing components read as zero, as undefined.toFloat() does today
    QCborMap partial;
    partial.insert(QLatin1String("x"), 4);
    QCOMPARE(codec.fromCbor(partial, vec3Type).value<glm::vec3>(), glm::vec3(4.0f, 0.0f, 0.0f));

    // a quaternion is normalised; an empty one is the identity
    QCborMap longQuat;
    longQuat.insert(QLatin1String("x"), 0);
    longQuat.insert(QLatin1String("y"), 0);
    longQuat.insert(QLatin1String("z"), 0);
    longQuat.insert(QLatin1String("w"), 2);
    QCOMPARE(codec.fromCbor(longQuat, qMetaTypeId<glm::quat>()).value<glm::quat>(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    QCOMPARE(codec.fromCbor(QCborMap(), qMetaTypeId<glm::quat>()).value<glm::quat>(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // vec2 accepts u, v
    QCborMap uv;
    uv.insert(QLatin1String("u"), 0.25);
    uv.insert(QLatin1String("v"), 0.5);
    QCOMPARE(codec.fromCbor(uv, qMetaTypeId<glm::vec2>()).value<glm::vec2>(), glm::vec2(0.25f, 0.5f));
}

void ValueCodecTests::testColorsAndGeometry() {
    ValueCodec codec;
    QColor color(10, 20, 30, 40);
    QCborMap map = codec.toCbor(QVariant(color)).toMap();
    QCOMPARE(map.value("red").toInteger(), qint64(10));
    QCOMPARE(map.value("alpha").toInteger(), qint64(40));
    QCOMPARE(codec.fromCbor(map, QMetaType::QColor).value<QColor>(), color);
    QCOMPARE(codec.fromCbor(QCborValue("blue"), QMetaType::QColor).value<QColor>(), QColor(Qt::blue));
    QCborMap noAlpha;
    noAlpha.insert(QLatin1String("red"), 1);
    noAlpha.insert(QLatin1String("green"), 2);
    noAlpha.insert(QLatin1String("blue"), 3);
    QCOMPARE(codec.fromCbor(noAlpha, QMetaType::QColor).value<QColor>(), QColor(1, 2, 3, 255));

    QRectF rect(1.0, 2.0, 3.0, 4.0);
    QCOMPARE(roundTrip(codec, QVariant(rect), QMetaType::QRectF).toRectF(), rect);
    QCOMPARE(roundTrip(codec, QVariant(QRect(1, 2, 3, 4)), QMetaType::QRect).toRect(), QRect(1, 2, 3, 4));
    QCOMPARE(roundTrip(codec, QVariant(QSizeF(3.5, 4.5)), QMetaType::QSizeF).toSizeF(), QSizeF(3.5, 4.5));
    QCOMPARE(roundTrip(codec, QVariant(QPointF(1.5, 2.5)), QMetaType::QPointF).toPointF(), QPointF(1.5, 2.5));
    QCOMPARE(roundTrip(codec, QVariant(QVector3D(1, 2, 3)), QMetaType::QVector3D).value<QVector3D>(), QVector3D(1, 2, 3));

    AACube cube(glm::vec3(1.0f, 2.0f, 3.0f), 4.0f);
    QCborMap cubeMap = codec.toCbor(QVariant::fromValue(cube)).toMap();
    QCOMPARE(cubeMap.value("scale").toDouble(), 4.0);
    AACube cubeBack = roundTrip(codec, QVariant::fromValue(cube), qMetaTypeId<AACube>()).value<AACube>();
    QCOMPARE(cubeBack.getCorner(), cube.getCorner());
    QCOMPARE(cubeBack.getScale(), cube.getScale());

    PickRay ray(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    PickRay rayBack = roundTrip(codec, QVariant::fromValue(ray), qMetaTypeId<PickRay>()).value<PickRay>();
    QCOMPARE(rayBack.origin, ray.origin);
    QCOMPARE(rayBack.direction, ray.direction);
}

void ValueCodecTests::testContainers() {
    ValueCodec codec;
    QStringList strings { "a", "b", "c" };
    QCOMPARE(roundTrip(codec, QVariant(strings), QMetaType::QStringList).toStringList(), strings);
    // untyped, an array of strings is a QVariantList
    QCOMPARE(roundTrip(codec, QVariant(strings)).userType(), int(QMetaType::QVariantList));

    QVariantList list { 1, "two", 3.0, true, QVariant() };
    QVariant listBack = roundTrip(codec, list);
    QCOMPARE(listBack.toList().size(), 5);
    QCOMPARE(listBack.toList()[0], QVariant(1));
    QCOMPARE(listBack.toList()[1], QVariant("two"));
    QCOMPARE(listBack.toList()[2], QVariant(3.0));
    QCOMPARE(listBack.toList()[3], QVariant(true));
    QVERIFY(!listBack.toList()[4].isValid());

    QVariantMap nested;
    nested["position"] = QVariant::fromValue(glm::vec3(1.0f, 2.0f, 3.0f));
    nested["ids"] = QVariant::fromValue(QVector<QUuid> { QUuid::createUuid(), QUuid() });
    nested["floats"] = QVariant::fromValue(QVector<float> { 0.5f, 1.5f });
    nested["points"] = QVariant::fromValue(QVector<glm::vec3> { glm::vec3(1.0f), glm::vec3(2.0f) });
    QCborMap nestedMap = codec.toCbor(nested).toMap();
    QVERIFY(nestedMap.value("position").isMap());
    QCOMPARE(nestedMap.value("ids").toArray().size(), qsizetype(2));
    QVERIFY(nestedMap.value("ids").toArray()[0].isString());
    QVERIFY(nestedMap.value("ids").toArray()[1].isNull());
    QCOMPARE(nestedMap.value("floats").toArray()[1].toDouble(), 1.5);
    QCOMPARE(nestedMap.value("points").toArray()[1].toMap().value("x").toDouble(), 2.0);

    // typed container decoding
    QCborArray ids { nested["ids"].value<QVector<QUuid>>()[0].toString(), QCborValue(nullptr) };
    QVector<QUuid> idsBack = codec.fromCbor(ids, qMetaTypeId<QVector<QUuid>>()).value<QVector<QUuid>>();
    QCOMPARE(idsBack, nested["ids"].value<QVector<QUuid>>());
    QCOMPARE(codec.fromCbor(QCborArray { 1, 2.5, "3" }, qMetaTypeId<QVector<float>>()).value<QVector<float>>(),
             (QVector<float> { 1.0f, 2.5f, 3.0f }));
    QCOMPARE(codec.fromCbor(QCborArray { 1, 2 }, qMetaTypeId<QVector<int>>()).value<QVector<int>>(), (QVector<int> { 1, 2 }));
    QCOMPARE(codec.fromCbor(QCborArray { 7 }, qMetaTypeId<QVector<unsigned int>>()).value<QVector<unsigned int>>(),
             (QVector<unsigned int> { 7u }));
    QCOMPARE(codec.fromCbor(QCborArray { "x", 1 }, qMetaTypeId<QVector<QString>>()).value<QVector<QString>>(),
             (QVector<QString> { "x", "1" }));
    QCborArray points { QCborArray { 1, 2, 3 }, 4 };
    QCOMPARE(codec.fromCbor(points, qMetaTypeId<QVector<glm::vec3>>()).value<QVector<glm::vec3>>(),
             (QVector<glm::vec3> { glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f) }));
    QCOMPARE(codec.fromCbor(QCborArray { true, false }, qMetaTypeId<QVector<bool>>()).value<QVector<bool>>(),
             (QVector<bool> { true, false }));
    bool ok = true;
    QCOMPARE(codec.fromCbor(QCborValue("not an array"), qMetaTypeId<QVector<float>>(), &ok).value<QVector<float>>().size(), 0);
    QVERIFY(!ok);
    QVERIFY(ValueCodec::isBuiltinType(qMetaTypeId<QVector<QUuid>>()));
    QVERIFY(ValueCodec::isBuiltinType(qMetaTypeId<QList<QVariant>>()));

    // a map with non-string keys still decodes (keys become their text)
    QCborMap odd;
    odd.insert(1, "one");
    QCOMPARE(codec.fromCbor(odd).toMap().value("1"), QVariant("one"));
}

void ValueCodecTests::testTypedNumbersAndStrings() {
    ValueCodec codec;
    QCOMPARE(codec.fromCbor(QCborValue(2.7), QMetaType::Int).toInt(), 2);
    QCOMPARE(codec.fromCbor(QCborValue(-2.7), QMetaType::Int).toInt(), -2);
    QCOMPARE(codec.fromCbor(QCborValue("12"), QMetaType::Int).toInt(), 12);
    QCOMPARE(codec.fromCbor(QCborValue(true), QMetaType::Int).toInt(), 1);
    QCOMPARE(codec.fromCbor(QCborValue(nullptr), QMetaType::Int).toInt(), 0);
    QCOMPARE(codec.fromCbor(QCborValue(3), QMetaType::Float).userType(), int(QMetaType::Float));
    QCOMPARE(codec.fromCbor(QCborValue(3), QMetaType::Float).toFloat(), 3.0f);
    QCOMPARE(codec.fromCbor(QCborValue(3), QMetaType::Double).toDouble(), 3.0);
    QCOMPARE(codec.fromCbor(QCborValue(300), QMetaType::UChar).value<uchar>(), uchar(44));
    QCOMPARE(codec.fromCbor(QCborValue(qint64(1) << 40), QMetaType::LongLong).toLongLong(), qint64(1) << 40);
    QCOMPARE(codec.fromCbor(QCborValue(5), QMetaType::ULongLong).toULongLong(), qulonglong(5));
    bool ok = true;
    QCOMPARE(codec.fromCbor(QCborValue("abc"), QMetaType::Int, &ok).toInt(), 0);
    QVERIFY(!ok);
    ok = true;
    QCOMPARE(codec.fromCbor(QCborMap(), QMetaType::Int, &ok).toInt(), 0);
    QVERIFY(!ok);

    QCOMPARE(codec.fromCbor(QCborValue(12), QMetaType::QString).toString(), QString("12"));
    QCOMPARE(codec.fromCbor(QCborValue(1.25), QMetaType::QString).toString(), QString("1.25"));
    QCOMPARE(codec.fromCbor(QCborValue(0.1), QMetaType::QString).toString(), QString("0.1"));
    QCOMPARE(codec.fromCbor(QCborValue(true), QMetaType::QString).toString(), QString("true"));
    QCOMPARE(codec.fromCbor(QCborValue(nullptr), QMetaType::QString).toString(), QString());
    QCborMap object;
    object.insert(QLatin1String("a"), 1);
    QCOMPARE(codec.fromCbor(object, QMetaType::QString).toString(), QString("{\"a\":1}"));

    QCOMPARE(codec.fromCbor(QCborValue(0), QMetaType::Bool).toBool(), false);
    QCOMPARE(codec.fromCbor(QCborValue(2), QMetaType::Bool).toBool(), true);
    QCOMPARE(codec.fromCbor(QCborValue(""), QMetaType::Bool).toBool(), false);
    QCOMPARE(codec.fromCbor(QCborValue("x"), QMetaType::Bool).toBool(), true);
    QCOMPARE(codec.fromCbor(QCborValue(nullptr), QMetaType::Bool).toBool(), false);
    QCOMPARE(codec.fromCbor(QCborMap(), QMetaType::Bool).toBool(), true);

    QCOMPARE(codec.fromCbor(QCborValue("bytes"), QMetaType::QByteArray).toByteArray(), QByteArray("bytes"));
    QCOMPARE(codec.fromCbor(QCborValue(QByteArray("x")), QMetaType::QString).toString(), QString("x"));
}

void ValueCodecTests::testObjects() {
    ObjectTable table;
    ValueCodec codec(&table);
    auto probe = new Probe();
    QCborValue cbor = codec.toCbor(QVariant::fromValue<QObject*>(probe));
    QVERIFY(cbor.isMap());
    QCOMPARE(cbor.toMap().value(ValueCodec::CLASS_KEY).toString(), QString("Probe"));
    const qint64 handle = cbor.toMap().value(ValueCodec::OBJECT_KEY).toInteger();
    QVERIFY(handle > 0);
    QCOMPARE(table.object(ObjectTable::Handle(handle)), probe);
    // the same object keeps its handle; a typed pointer encodes the same way
    QCOMPARE(codec.toCbor(QVariant::fromValue(probe)).toMap().value(ValueCodec::OBJECT_KEY).toInteger(), handle);
    QCOMPARE(table.size(), 1);

    // back: untyped gives a QObject*, typed gives the subclass pointer, a wrong class gives null
    QVariant untyped = codec.fromCbor(cbor);
    QCOMPARE(untyped.userType(), int(QMetaType::QObjectStar));
    QCOMPARE(untyped.value<QObject*>(), probe);
    QVariant typed = codec.fromCbor(cbor, qMetaTypeId<Probe*>());
    QCOMPARE(typed.userType(), qMetaTypeId<Probe*>());
    QCOMPARE(typed.value<Probe*>(), probe);
    bool ok = true;
    QCOMPARE(codec.fromCbor(cbor, qMetaTypeId<Other*>(), &ok).value<Other*>(), nullptr);
    QVERIFY(!ok);
    ok = true;
    QCOMPARE(codec.fromCbor(QCborValue(nullptr), qMetaTypeId<Probe*>(), &ok).value<Probe*>(), nullptr);
    QVERIFY(ok);
    ok = true;
    QCborMap unknown;
    unknown.insert(ValueCodec::OBJECT_KEY, 999);
    QCOMPARE(codec.fromCbor(unknown, qMetaTypeId<Probe*>(), &ok).value<Probe*>(), nullptr);
    QVERIFY(!ok);

    // a null QObject* is null, not a handle
    QCOMPARE(codec.toCbor(QVariant::fromValue<QObject*>(nullptr)), QCborValue(nullptr));

    // a deleted object is a dead handle, not a crash
    delete probe;
    QCOMPARE(table.object(ObjectTable::Handle(handle)), nullptr);
    QCOMPARE(table.className(ObjectTable::Handle(handle)), QString("Probe"));
    QCOMPARE(codec.fromCbor(cbor).value<QObject*>(), nullptr);
    table.release(ObjectTable::Handle(handle));
    QCOMPARE(table.size(), 0);

    // a named global
    Probe global;
    table.registerGlobal("Probe", &global);
    QCOMPARE(table.object(table.global("Probe")), &global);
    QCOMPARE(table.globalNames(), QStringList { "Probe" });
    QCOMPARE(table.handleFor(&global), table.global("Probe"));

    // without a table, an object cannot cross
    ValueCodec bare;
    QCOMPARE(bare.toCbor(QVariant::fromValue<QObject*>(&global)), QCborValue(QCborValue::Undefined));
}

void ValueCodecTests::testCallbacks() {
    ValueCodec codec;
    ScriptCallback callback { 17 };
    QCborValue cbor = codec.toCbor(QVariant::fromValue(callback));
    QCOMPARE(cbor.toMap().value(ValueCodec::CALLBACK_KEY).toInteger(), qint64(17));
    QCOMPARE(codec.fromCbor(cbor).value<ScriptCallback>(), callback);
    QCOMPARE(codec.fromCbor(cbor, qMetaTypeId<ScriptCallback>()).value<ScriptCallback>(), callback);
    QVERIFY(!codec.fromCbor(QCborValue(nullptr), qMetaTypeId<ScriptCallback>()).value<ScriptCallback>().isValid());
    bool ok = true;
    codec.fromCbor(QCborValue(5), qMetaTypeId<ScriptCallback>(), &ok);
    QVERIFY(!ok);
}

void ValueCodecTests::testEnums() {
    ValueCodec codec;
    const int modeType = qMetaTypeId<Probe::Mode>();
    QCOMPARE(codec.toCbor(QVariant::fromValue(Probe::Stopped)), QCborValue(10));
    QCOMPARE(codec.fromCbor(QCborValue(10), modeType).value<Probe::Mode>(), Probe::Stopped);
    QCOMPARE(codec.fromCbor(QCborValue(10), modeType).userType(), modeType);
    QCOMPARE(codec.fromCbor(QCborValue("Running"), modeType).value<Probe::Mode>(), Probe::Running);
    QCOMPARE(codec.fromCbor(QCborValue("Probe::Running"), modeType).value<Probe::Mode>(), Probe::Running);
    bool ok = true;
    codec.fromCbor(QCborValue("Flying"), modeType, &ok);
    QVERIFY(!ok);
    QVERIFY(ValueCodec::isBuiltinType(modeType));
}

void ValueCodecTests::testRegistryAndFallback() {
    ValueCodec codec;
    const int opaqueType = qRegisterMetaType<Opaque>();
    const int unknownType = qRegisterMetaType<Unknown>();
    QVERIFY(!ValueCodec::isBuiltinType(opaqueType));

    // unknown everywhere: undefined, and decoding fails
    QCOMPARE(codec.toCbor(QVariant::fromValue(Opaque { 3 })), QCborValue(QCborValue::Undefined));
    bool ok = true;
    codec.fromCbor(QCborValue(3), opaqueType, &ok);
    QVERIFY(!ok);

    // registered: the pair is used
    ValueCodec::registerType(opaqueType,
        [](const ValueCodec&, const QVariant& value) { return QCborValue(value.value<Opaque>().value); },
        [](const ValueCodec&, const QCborValue& value, QVariant& out) {
            if (!value.isInteger()) {
                return false;
            }
            out = QVariant::fromValue(Opaque { int(value.toInteger()) });
            return true;
        });
    QVERIFY(ValueCodec::hasRegisteredType(opaqueType));
    QCOMPARE(codec.toCbor(QVariant::fromValue(Opaque { 3 })), QCborValue(3));
    ok = true;
    QCOMPARE(codec.fromCbor(QCborValue(4), opaqueType, &ok).value<Opaque>().value, 4);
    QVERIFY(ok);
    ok = true;
    codec.fromCbor(QCborValue("no"), opaqueType, &ok);
    QVERIFY(!ok);

    // the fallback sees what neither the codec nor the registry knows
    int encodeCalls = 0, decodeCalls = 0;
    codec.setFallback(
        [&](const QVariant& value, QCborValue& out) {
            ++encodeCalls;
            if (value.userType() != unknownType) {
                return false;
            }
            out = QCborValue(QString("unknown:%1").arg(value.value<Unknown>().value));
            return true;
        },
        [&](const QCborValue& value, int targetType, QVariant& out) {
            ++decodeCalls;
            if (targetType != unknownType || !value.isString()) {
                return false;
            }
            out = QVariant::fromValue(Unknown { value.toString().section(':', 1).toInt() });
            return true;
        });
    QCOMPARE(codec.toCbor(QVariant::fromValue(Unknown { 9 })), QCborValue("unknown:9"));
    QCOMPARE(codec.fromCbor(QCborValue("unknown:8"), unknownType).value<Unknown>().value, 8);
    QCOMPARE(encodeCalls, 1);
    QCOMPARE(decodeCalls, 1);
    // builtins and registered types never reach the fallback
    codec.toCbor(QVariant(1));
    codec.toCbor(QVariant::fromValue(Opaque { 1 }));
    codec.fromCbor(QCborValue(1), opaqueType);
    QCOMPARE(encodeCalls, 1);
    QCOMPARE(decodeCalls, 1);
}

void ValueCodecTests::testRegistryPrecedence() {
    ObjectTable table;
    ValueCodec codec(&table);
    const int shadeType = qRegisterMetaType<Shade>();
    QCOMPARE(codec.toCbor(QVariant::fromValue(Shade::Dark)), QCborValue(2));

    // a registered enum has the shape its conversion gives (as {action, actionName} for a controller action)
    ValueCodec::registerType(shadeType,
        [](const ValueCodec&, const QVariant& value) {
            QCborMap map;
            map.insert(QStringLiteral("shade"), int(value.value<Shade>()));
            map.insert(QStringLiteral("name"), value.value<Shade>() == Shade::Dark ? "Dark" : "Light");
            return QCborValue(map);
        },
        [](const ValueCodec&, const QCborValue& value, QVariant& out) {
            out = QVariant::fromValue(Shade(value.toMap().value(QStringLiteral("shade")).toInteger()));
            return true;
        });
    QCborValue cbor = codec.toCbor(QVariant::fromValue(Shade::Dark));
    QCOMPARE(cbor.toMap().value(QStringLiteral("name")).toString(), QString("Dark"));
    QCOMPARE(codec.fromCbor(cbor, shadeType).value<Shade>(), Shade::Dark);

    // a registered object pointer type: here owned by the table, as a conversion for a ScriptOwnership type does
    const int otherType = qRegisterMetaType<Other*>();
    ValueCodec::registerType(otherType,
        [](const ValueCodec& codec, const QVariant& value) { return codec.toCborOwned(value.value<Other*>()); },
        [](const ValueCodec& codec, const QCborValue& value, QVariant& out) {
            out = QVariant::fromValue(qobject_cast<Other*>(codec.fromCbor(value).value<QObject*>()));
            return true;
        });
    QPointer<Other> other = new Other();
    cbor = codec.toCbor(QVariant::fromValue(other.data()));
    const auto handle = ObjectTable::Handle(cbor.toMap().value(ValueCodec::OBJECT_KEY).toInteger());
    QVERIFY(table.owns(handle));
    QCOMPARE(codec.fromCbor(cbor, otherType).value<Other*>(), other.data());
    table.release(handle);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(other.isNull());

    // an unregistered object pointer is not owned
    Probe probe;
    cbor = codec.toCbor(QVariant::fromValue(&probe));
    QVERIFY(!table.owns(ObjectTable::Handle(cbor.toMap().value(ValueCodec::OBJECT_KEY).toInteger())));
}

void ValueCodecTests::testOwnedObjects() {
    QPointer<Probe> kept = new Probe();
    QPointer<Probe> owned = new Probe();
    QPointer<Probe> later = new Probe();
    {
        ObjectTable table;
        const ObjectTable::Handle keptHandle = table.handleFor(kept);
        const ObjectTable::Handle ownedHandle = table.adopt(owned);
        QVERIFY(!table.owns(keptHandle));
        QVERIFY(table.owns(ownedHandle));
        // adopting an object already in the table keeps its handle
        QCOMPARE(table.adopt(owned), ownedHandle);

        // releasing an owned handle deletes the object; releasing another only forgets it
        table.release(keptHandle);
        table.release(ownedHandle);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!kept.isNull());
        QVERIFY(owned.isNull());

        // releaseOwned deletes what is left of the owned objects and keeps the rest
        const ObjectTable::Handle laterHandle = table.adopt(later);
        const ObjectTable::Handle keptAgain = table.handleFor(kept);
        table.releaseOwned();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(later.isNull());
        QVERIFY(!table.contains(laterHandle));
        QCOMPARE(table.object(keptAgain), kept.data());

        // an owned object deleted on the host side is just a dead handle
        QPointer<Probe> gone = new Probe();
        const ObjectTable::Handle goneHandle = table.adopt(gone);
        delete gone.data();
        QCOMPARE(table.object(goneHandle), nullptr);
        table.release(goneHandle);

        // an owned object with work left lingers when released (an injector whose sound plays): it stays until it
        // sees to its own end, or until the owned objects are released
        QPointer<Probe> busy = new Probe();
        QPointer<Probe> idle = new Probe();
        bool idleIsBusy = false;
        const ObjectTable::Handle busyHandle = table.adopt(busy, [](QObject*) { return true; });
        const ObjectTable::Handle idleHandle = table.adopt(idle, [&idleIsBusy](QObject*) { return idleIsBusy; });
        table.release(busyHandle);
        table.release(idleHandle);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!busy.isNull());
        QVERIFY(!table.contains(busyHandle));
        QVERIFY(idle.isNull());
        // a sweep keeps what is still busy and deletes what is done
        QPointer<Probe> done = new Probe();
        bool doneIsBusy = true;
        table.release(table.adopt(done, [&doneIsBusy](QObject*) { return doneIsBusy; }));
        QCOMPARE(table.sweepLingering(), 2);
        doneIsBusy = false;
        QCOMPARE(table.sweepLingering(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(done.isNull());
        QVERIFY(!busy.isNull());
        // and the owned objects' release takes the rest
        table.releaseOwned();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(busy.isNull());

        // the table's end deletes the owned objects it still has
        later = new Probe();
        table.adopt(later);
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(later.isNull());
    QVERIFY(!kept.isNull());
    delete kept.data();
}

void ValueCodecTests::testSmartPointers() {
    ObjectTable table;
    ValueCodec codec(&table);
    qRegisterMetaType<QPointer<Probe>>();
    qRegisterMetaType<QSharedPointer<Probe>>();
    Probe probe;
    // a QPointer to an object (Desktop.createWindow's window) crosses as the object's handle
    QCborValue cbor = codec.toCbor(QVariant::fromValue(QPointer<Probe>(&probe)));
    QCOMPARE(cbor.toMap().value(ValueCodec::CLASS_KEY).toString(), QString("Probe"));
    QCOMPARE(table.object(ObjectTable::Handle(cbor.toMap().value(ValueCodec::OBJECT_KEY).toInteger())), &probe);
    QCOMPARE(codec.toCbor(QVariant::fromValue(QPointer<Probe>())), QCborValue(nullptr));
    // and a QSharedPointer the same way
    QSharedPointer<Probe> shared(new Probe());
    cbor = codec.toCbor(QVariant::fromValue(shared));
    QCOMPARE(table.object(ObjectTable::Handle(cbor.toMap().value(ValueCodec::OBJECT_KEY).toInteger())), shared.data());
}

void ValueCodecTests::testDecodeArguments() {
    ValueCodec codec;
    QCborMap position;
    position.insert(QLatin1String("x"), 1);
    position.insert(QLatin1String("y"), 2);
    position.insert(QLatin1String("z"), 3);
    QCborArray args { 7, "seven", position, "extra" };
    QVariantList out;
    QString error;
    QVERIFY2(codec.decodeArguments(args.toCborValue().toCbor(),
                                  { QMetaType::Int, QMetaType::QString, qMetaTypeId<glm::vec3>(), QMetaType::Bool, QMetaType::Int },
                                  out, error), qPrintable(error));
    QCOMPARE(out.size(), 5);
    QCOMPARE(out[0], QVariant(7));
    QCOMPARE(out[1], QVariant("seven"));
    QCOMPARE(out[2].value<glm::vec3>(), glm::vec3(1.0f, 2.0f, 3.0f));
    QCOMPARE(out[3], QVariant(true));   // "extra" as a bool: truthy
    QVERIFY(!out[4].isValid());         // missing: undefined
    // more arguments than parameters are kept as plain values
    QVERIFY(codec.decodeArguments(args.toCborValue().toCbor(), { QMetaType::Int }, out, error));
    QCOMPARE(out.size(), 4);
    QCOMPARE(out[3], QVariant("extra"));
    QVERIFY(!codec.decodeArguments(codec.encode(QVariant(1)), { QMetaType::Int }, out, error));
    QVERIFY(error.contains("array"));
}

void ValueCodecTests::testMalformedInput() {
    ValueCodec codec;
    QVariant out;
    QString error;
    QVERIFY(!codec.decode(QByteArray("\xa1\x61", 2), out, error));
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!codec.decode(QByteArray("\xff", 1), QMetaType::Int, out, error));
    QVERIFY(!error.isEmpty());
    QVERIFY(codec.decode(QByteArray(), out, error) == false || !out.isValid());
}

// encode() writes the common types directly; the bytes must be the ones toCbor(value).toCbor() gives.
void ValueCodecTests::testDirectEncodingMatchesTheTree() {
    ValueCodec codec;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const QVariantList values {
        QVariant(), QVariant::fromValue(nullptr), QVariant(true), QVariant(false),
        QVariant(0), QVariant(1), QVariant(23), QVariant(24), QVariant(255), QVariant(256), QVariant(65535), QVariant(65536),
        QVariant(-1), QVariant(-24), QVariant(-25), QVariant(-256), QVariant(-257), QVariant(std::numeric_limits<int>::min()),
        QVariant(qlonglong(1) << 40), QVariant(-(qlonglong(1) << 40)), QVariant(std::numeric_limits<qlonglong>::min()),
        QVariant(std::numeric_limits<qlonglong>::max()), QVariant(uint(4000000000u)), QVariant(qulonglong(1) << 63),
        QVariant(std::numeric_limits<qulonglong>::max()), QVariant(short(-7)), QVariant::fromValue<char>('A'),
        QVariant(0.0), QVariant(-0.0), QVariant(1.0), QVariant(1.5), QVariant(-3.25), QVariant(1e300), QVariant(nan),
        QVariant(std::numeric_limits<double>::infinity()), QVariant(1.0f / 3.0f), QVariant(float(2.5)),
        QVariant(QString()), QVariant(QString("x")), QVariant(QString("héllo ✓")), QVariant(QString(23, 'a')),
        QVariant(QString(24, 'b')), QVariant(QString(300, 'c')), QVariant(QString(70000, 'd')),
        QVariant(QUuid()), QVariant(QUuid("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}")),
        QVariant::fromValue(glm::vec3(1.0f, -2.5f, 1.0f / 3.0f)), QVariant::fromValue(glm::vec3(0.0f)),
        QVariant::fromValue(glm::quat(0.5f, -0.5f, 0.25f, 1.0f / 7.0f)), QVariant::fromValue(glm::vec2(3.0f, 0.1f)),
        QVariant::fromValue(glm::vec4(1.0f, 2.0f, 3.0f, 4.0f)),
    };
    for (const QVariant& value : values) {
        QByteArray direct = codec.encode(value);
        QByteArray tree = codec.toCbor(value).toCbor();
        QVERIFY2(direct == tree, qPrintable(QString("%1 (%2): %3, the tree gives %4").arg(value.toString(), value.typeName())
                                                .arg(QString::fromLatin1(direct.toHex()), QString::fromLatin1(tree.toHex()))));
    }
    // the rest still go through the tree
    QCOMPARE(codec.encode(QVariant(QVariantList { 1, "two" })), codec.toCbor(QVariant(QVariantList { 1, "two" })).toCbor());
}

// genericTypeOf gives the type of the value fromCbor makes, without making it.
void ValueCodecTests::testGenericTypes() {
    ObjectTable objects;
    ValueCodec codec(&objects);
    Probe probe;
    ObjectTable::Handle handle = objects.handleFor(&probe);
    QCborMap object;
    object.insert(QStringLiteral("@object"), qint64(handle));
    QCborMap callback;
    callback.insert(QStringLiteral("@callback"), 3);
    QCborMap plain;
    plain.insert(QStringLiteral("x"), 1);
    const QList<QCborValue> items {
        QCborValue(1), QCborValue(qint64(1) << 40), QCborValue(2.5), QCborValue(true), QCborValue(nullptr),
        QCborValue(QCborValue::Undefined), QCborValue(QStringLiteral("text")), QCborValue(QByteArray("bytes")),
        QCborValue(QCborArray { 1, 2 }), QCborValue(object), QCborValue(callback), QCborValue(plain),
    };
    for (const QCborValue& item : items) {
        QCOMPARE(codec.genericTypeOf(item), codec.fromCbor(item).userType());
    }
}

// TestObjects.h has no source file of its own, so its moc output is pulled in here.
#include "moc_TestObjects.cpp"
