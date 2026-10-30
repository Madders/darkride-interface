//
//  QtScriptOverloadTests.cpp
//  tests/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "QtScriptOverloadTests.h"

#include <QtScript/QScriptEngine>

QTEST_GUILESS_MAIN(QtScriptOverloadTests)

namespace {

QString callIn(QScriptEngine& engine, OverloadProbe& probe, const QString& program) {
    probe.called.clear();
    QScriptValue result = engine.evaluate(program);
    if (engine.hasUncaughtException()) {
        QString message = engine.uncaughtException().toString();
        engine.clearExceptions();
        return "throws " + message;
    }
    return probe.called;
}

} // namespace

void QtScriptOverloadTests::testCallbackOverloadIsNeverChosen() {
    qRegisterMetaType<ScriptCallback>();  // as the engine's codec registers it in a client
    QScriptEngine engine;
    OverloadProbe probe;
    engine.globalObject().setProperty("probe", engine.newQObject(&probe));
    const QStringList calls {
        "probe.fetch({ url: 'atp:/a.txt' }, function () {})",
        "probe.fetch('atp:/a.txt', function () {})",
        "probe.fetch('atp:/a.txt', { handler: function () {} }, 'handler')",
        "probe.fetch('atp:/a.txt', {}, function () {})",
        "probe.fetch({}, null)",
        "probe.fetch({}, undefined)",
        "probe.fetch({}, 'handler')",
        "probe.fetch({}, 7)",
        "probe.upload('text', function () {})",
        "probe.upload('text', null)",
        "probe.upload(12, function () {})",
    };
    for (const QString& call : calls) {
        QCOMPARE(callIn(engine, probe, call), QString("QScriptValue"));
    }
}

void QtScriptOverloadTests::testPlainValueTwinIsAmbiguous() {
    QScriptEngine engine;
    OverloadProbe probe;
    engine.globalObject().setProperty("probe", engine.newQObject(&probe));
    // why the controller builders' twins are named fromValue, toValue...: QtScript scores the two the same
    QVERIFY(callIn(engine, probe, "probe.pick({ x: 1 })").startsWith("throws"));
}

void QtScriptOverloadTests::testMembersByAccess() {
    QScriptEngine engine;
    AccessProbe probe;
    engine.globalObject().setProperty("probe", engine.newQObject(&probe));
    auto typeOf = [&](const QString& name) { return engine.evaluate("typeof probe." + name).toString(); };
    QCOMPARE(typeOf("publicInvokable"), QString("function"));
    QCOMPARE(typeOf("publicSlot"), QString("function"));
    QCOMPARE(typeOf("protectedInvokable"), QString("function"));
    QCOMPARE(typeOf("protectedSlot"), QString("function"));
    QCOMPARE(typeOf("privateInvokable"), QString("undefined"));
    QCOMPARE(typeOf("privateSlot"), QString("undefined"));
    QCOMPARE(engine.evaluate("probe.protectedInvokable()").toInt32(), 3);
}

void QtScriptOverloadTests::testVariantResults() {
    QScriptEngine engine;
    OverloadProbe probe;
    engine.globalObject().setProperty("probe", engine.newQObject(&probe));
    auto evaluate = [&](const QString& program) { return engine.evaluate(program).toString(); };
    // a QVariant holding nullptr reaches a script as an opaque variant, not null; a null QObject* is null
    QCOMPARE(evaluate("String(probe.nothing() === null)"), QString("false"));
    QCOMPARE(evaluate("String(probe.noObject() === null)"), QString("true"));
    QCOMPARE(evaluate("typeof probe.invalid()"), QString("undefined"));
    QCOMPARE(evaluate("typeof probe.text() + ':' + probe.text()"), QString("string:chosen"));
    QCOMPARE(evaluate("typeof probe.flag() + ':' + probe.flag()"), QString("boolean:true"));
}

void QtScriptOverloadTests::testEnumResults() {
    // QtScript, too, can't return an enum Qt doesn't know: it throws, and asks for the type to be registered
    QScriptEngine engine;
    EnumProbe probe;
    engine.globalObject().setProperty("probe", engine.newQObject(&probe));
    QCOMPARE(engine.evaluate("probe.currentMode()").toInt32(), 10);
    engine.evaluate("probe.plainLevel()");
    QVERIFY(engine.hasUncaughtException());
    QString message = engine.uncaughtException().toString();
    engine.clearExceptions();
    QVERIFY2(message.contains("unknown return type"), qPrintable(message));
}
