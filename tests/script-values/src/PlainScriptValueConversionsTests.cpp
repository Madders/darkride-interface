//
//  PlainScriptValueConversionsTests.cpp
//  tests/script-values/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PlainScriptValueConversionsTests.h"

#include <cmath>
#include <limits>

#include <QtCore/QCborValue>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>

#include <PlainScriptValue.h>
#include <PlainScriptValueConversions.h>
#include <RegisteredMetaTypes.h>

#include "SameVariant.h"

QTEST_MAIN(PlainScriptValueConversionsTests)

namespace {

// The same input on both sides: what a script would write, evaluated by QtScript, and the same JSON read as a
// plain value (through CBOR, as a module's values come).
struct Inputs {
    QScriptEngine engine;
    QScriptValue script;
    PlainScriptValue plain;

    explicit Inputs(const QString& json) {
        script = engine.evaluate("(" + json + ")");
        if (json == "undefined") {
            plain = PlainScriptValue::undefinedValue();
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(("[" + json + "]").toUtf8(), &error);
        Q_ASSERT(error.error == QJsonParseError::NoError);
        plain = PlainScriptValue::fromCbor(QCborValue::fromJsonValue(document.array().at(0)));
    }
};

QString vec3Text(const glm::vec3& v) {
    return QString("(%1, %2, %3)").arg(v.x).arg(v.y).arg(v.z);
}

#define COMPARE_VEC3(actual, expected, input) \
    QVERIFY2((actual) == (expected), qPrintable(QString(input) + ": " + vec3Text(actual) + " vs " + vec3Text(expected)))

}  // namespace

void PlainScriptValueConversionsTests::testWriting() {
    QScriptEngine engine;
    PlainScriptEngine plain;
    QString where;
    const glm::vec3 vec3(1.5f, -2.0f, 0.25f);
    const glm::u8vec3 color(255, 0, 128);
    const glm::quat quat(0.5f, 0.5f, 0.5f, 0.5f);
    const glm::quat broken(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f);
    const QRect rect(1, 2, 30, 40);
    const AACube cube(glm::vec3(-1.0f, 2.0f, 3.5f), 4.0f);
    const QVector<glm::vec3> vec3s { glm::vec3(1.0f), glm::vec3(0.0f, 0.5f, 1.0f) };
    const QVector<glm::quat> quats { glm::quat(), quat };
    const QVector<bool> bools { true, false, true };
    const QVector<float> floats { 0.5f, 1.0f, -3.25f };

    QVERIFY2(sameVariant(vec2ToScriptValue(&engine, glm::vec2(1.0f, 2.5f)).toVariant(),
                         vec2ToScriptValue(&plain, glm::vec2(1.0f, 2.5f)).toVariant(), where), qPrintable("vec2: " + where));
    QVERIFY2(sameVariant(vec3ToScriptValue(&engine, vec3).toVariant(), vec3ToScriptValue(&plain, vec3).toVariant(), where),
             qPrintable("vec3: " + where));
    QVERIFY2(sameVariant(vec3ColorToScriptValue(&engine, vec3).toVariant(), vec3ColorToScriptValue(&plain, vec3).toVariant(), where),
             qPrintable("vec3 colour: " + where));
    QVERIFY2(sameVariant(u8vec3ToScriptValue(&engine, color).toVariant(), u8vec3ToScriptValue(&plain, color).toVariant(), where),
             qPrintable("u8vec3: " + where));
    QVERIFY2(sameVariant(u8vec3ColorToScriptValue(&engine, color).toVariant(), u8vec3ColorToScriptValue(&plain, color).toVariant(), where),
             qPrintable("u8vec3 colour: " + where));
    QVERIFY2(sameVariant(quatToScriptValue(&engine, quat).toVariant(), quatToScriptValue(&plain, quat).toVariant(), where),
             qPrintable("quat: " + where));
    QVERIFY2(sameVariant(quatToScriptValue(&engine, broken).toVariant(), quatToScriptValue(&plain, broken).toVariant(), where),
             qPrintable("quat with NaN: " + where));
    QVERIFY(quatToScriptValue(&plain, broken).propertyNames().isEmpty());
    QVERIFY2(sameVariant(qRectToScriptValue(&engine, rect).toVariant(), qRectToScriptValue(&plain, rect).toVariant(), where),
             qPrintable("rect: " + where));
    QVERIFY2(sameVariant(aaCubeToScriptValue(&engine, cube).toVariant(), aaCubeToScriptValue(&plain, cube).toVariant(), where),
             qPrintable("cube: " + where));
    QVERIFY2(sameVariant(qVectorVec3ToScriptValue(&engine, vec3s).toVariant(), qVectorVec3ToScriptValue(&plain, vec3s).toVariant(), where),
             qPrintable("vec3 list: " + where));
    QVERIFY2(sameVariant(qVectorVec3ColorToScriptValue(&engine, vec3s).toVariant(), qVectorVec3ColorToScriptValue(&plain, vec3s).toVariant(), where),
             qPrintable("colour list: " + where));
    QVERIFY2(sameVariant(qVectorQuatToScriptValue(&engine, quats).toVariant(), qVectorQuatToScriptValue(&plain, quats).toVariant(), where),
             qPrintable("quat list: " + where));
    QVERIFY2(sameVariant(qVectorBoolToScriptValue(&engine, bools).toVariant(), qVectorBoolToScriptValue(&plain, bools).toVariant(), where),
             qPrintable("bool list: " + where));
    QVERIFY2(sameVariant(qVectorFloatToScriptValue(&engine, floats).toVariant(), qVectorFloatToScriptValue(&plain, floats).toVariant(), where),
             qPrintable("float list: " + where));
    // the members in the order a script sees them
    QCOMPARE(vec3ToScriptValue(&plain, vec3).propertyNames(), QStringList({ "x", "y", "z" }));
    QCOMPARE(u8vec3ColorToScriptValue(&plain, color).propertyNames(), QStringList({ "red", "green", "blue" }));
    QCOMPARE(quatToScriptValue(&plain, quat).propertyNames(), QStringList({ "x", "y", "z", "w" }));
    QCOMPARE(qRectToScriptValue(&plain, rect).propertyNames(), QStringList({ "x", "y", "width", "height" }));
    QCOMPARE(aaCubeToScriptValue(&plain, cube).propertyNames(), QStringList({ "x", "y", "z", "scale" }));
}

void PlainScriptValueConversionsTests::testReadingVectors() {
    const QStringList inputs {
        "{\"x\": 1, \"y\": 2, \"z\": 3}", "{\"x\": 1}", "{\"r\": 1, \"g\": 2, \"b\": 3}", "{\"red\": 10, \"green\": 20, \"blue\": 30}",
        "{\"x\": 1, \"r\": 7, \"y\": \"2.5\", \"z\": null}", "5", "-1.25", "[1, 2, 3]", "[1, 2]", "[]", "\"red\"", "\"#102030\"",
        "\"nonsense\"", "\"\"", "null", "true", "{}", "undefined", "{\"u\": 1, \"v\": 2}", "{\"x\": \"abc\", \"y\": true}"
    };
    for (const QString& input : inputs) {
        Inputs in(input);
        glm::vec3 fromScript(9.0f), fromPlain(9.0f);  // the same start: a conversion may leave a component as it was
        vec3FromScriptValue(in.script, fromScript);
        vec3FromScriptValue(in.plain, fromPlain);
        COMPARE_VEC3(fromPlain, fromScript, "vec3 from " + input);
        glm::vec2 vec2FromScript(9.0f), vec2FromPlain(9.0f);
        vec2FromScriptValue(in.script, vec2FromScript);
        vec2FromScriptValue(in.plain, vec2FromPlain);
        QVERIFY2(vec2FromPlain == vec2FromScript, qPrintable("vec2 from " + input));
        glm::quat quatFromScript, quatFromPlain;
        quatFromScriptValue(in.script, quatFromScript);
        quatFromScriptValue(in.plain, quatFromPlain);
        QVERIFY2(quatFromPlain == quatFromScript, qPrintable("quat from " + input));
    }
    // the cases that matter most, spelled out
    glm::vec3 one;
    vec3FromScriptValue(Inputs("{\"x\": 1}").plain, one);
    COMPARE_VEC3(one, glm::vec3(1.0f, 0.0f, 0.0f), "{x: 1}");
    glm::vec3 five;
    vec3FromScriptValue(Inputs("5").plain, five);
    COMPARE_VEC3(five, glm::vec3(5.0f), "5");
    glm::vec3 red;
    vec3FromScriptValue(Inputs("\"red\"").plain, red);
    COMPARE_VEC3(red, glm::vec3(255.0f, 0.0f, 0.0f), "red");
}

void PlainScriptValueConversionsTests::testReadingColors() {
    const QStringList inputs {
        "{\"red\": 255, \"green\": 128, \"blue\": 0}", "{\"x\": 1, \"y\": 2, \"z\": 3}", "{\"r\": 300, \"g\": -1, \"b\": 2.7}",
        "7", "[10, 20, 30]", "[10, 20]", "\"blue\"", "\"#ff8000\"", "\"nonsense\"", "null", "{}", "undefined"
    };
    for (const QString& input : inputs) {
        Inputs in(input);
        glm::u8vec3 fromScript(9), fromPlain(9);
        u8vec3FromScriptValue(in.script, fromScript);
        u8vec3FromScriptValue(in.plain, fromPlain);
        QVERIFY2(fromPlain == fromScript, qPrintable(QString("u8vec3 from %1: (%2, %3, %4) vs (%5, %6, %7)").arg(input)
                                                         .arg(fromPlain.x).arg(fromPlain.y).arg(fromPlain.z)
                                                         .arg(fromScript.x).arg(fromScript.y).arg(fromScript.z)));
    }
}

void PlainScriptValueConversionsTests::testReadingOthers() {
    const QStringList inputs {
        "{\"x\": 1, \"y\": 2, \"width\": 30, \"height\": 40, \"scale\": 2.5, \"z\": -3}", "{\"x\": \"4\", \"scale\": \"big\"}",
        "{}", "null", "5", "[1, 2, 3, 4]", "undefined"
    };
    for (const QString& input : inputs) {
        Inputs in(input);
        QRect rectFromScript(7, 7, 7, 7), rectFromPlain(7, 7, 7, 7);
        qRectFromScriptValue(in.script, rectFromScript);
        qRectFromScriptValue(in.plain, rectFromPlain);
        QVERIFY2(rectFromPlain == rectFromScript, qPrintable("rect from " + input));
        AACube cubeFromScript, cubeFromPlain;
        aaCubeFromScriptValue(in.script, cubeFromScript);
        aaCubeFromScriptValue(in.plain, cubeFromPlain);
        QVERIFY2(cubeFromPlain.getCorner() == cubeFromScript.getCorner() && cubeFromPlain.getScale() == cubeFromScript.getScale(),
                 qPrintable("cube from " + input));
    }
}

void PlainScriptValueConversionsTests::testReadingLists() {
    const QStringList inputs {
        "[{\"x\": 1, \"y\": 2, \"z\": 3}, 4, [5, 6, 7], \"red\", null]", "[1, \"a\", 2.5, null, true, \"3\"]",
        "[true, 0, \"x\", null, \"\", 1]", "[{\"x\": 0, \"y\": 0, \"z\": 0, \"w\": 1}, {}, {\"x\": 2, \"y\": 0, \"z\": 0, \"w\": 0}]",
        "[]", "{\"length\": 2, \"0\": 1, \"1\": 2}", "5", "\"text\"", "null", "undefined"
    };
    for (const QString& input : inputs) {
        Inputs in(input);
        QVERIFY2(qVectorVec3FromScriptValue(in.plain) == qVectorVec3FromScriptValue(in.script), qPrintable("vec3s from " + input));
        QVERIFY2(qVectorQuatFromScriptValue(in.plain) == qVectorQuatFromScriptValue(in.script), qPrintable("quats from " + input));
        QVERIFY2(qVectorBoolFromScriptValue(in.plain) == qVectorBoolFromScriptValue(in.script), qPrintable("bools from " + input));
        QVERIFY2(qVectorFloatFromScriptValue(in.plain) == qVectorFloatFromScriptValue(in.script), qPrintable("floats from " + input));
        QVector<float> floatsFromScript, floatsFromPlain;
        qVectorFloatFromScriptValue(in.script, floatsFromScript);
        qVectorFloatFromScriptValue(in.plain, floatsFromPlain);
        QVERIFY2(floatsFromPlain == floatsFromScript, qPrintable("floats (every element) from " + input));
    }
    // the selective reader keeps the numbers only, the other every element as a number
    QCOMPARE(qVectorFloatFromScriptValue(Inputs("[1, \"a\", 2.5, null, true, \"3\"]").plain), QVector<float>({ 1.0f, 2.5f }));
    QVector<float> every;
    qVectorFloatFromScriptValue(Inputs("[1, \"a\", 2.5, null, true, \"3\"]").plain, every);
    QCOMPARE(every, QVector<float>({ 1.0f, 0.0f, 2.5f, 0.0f, 1.0f, 3.0f }));
}
