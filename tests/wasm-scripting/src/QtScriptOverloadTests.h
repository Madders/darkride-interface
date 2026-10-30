//
//  QtScriptOverloadTests.h
//  tests/wasm-scripting/src
//
//  How the QtScript engine chooses between overloads, for the methods the WebAssembly engine gets beside a
//  QScriptValue one: an overload that takes a ScriptCallback is never chosen by QtScript (no script value
//  converts to one), so it may keep the method's name; an overload taking only plain values (a QVariant, a
//  number) makes QtScript's call ambiguous, so such a twin needs a name of its own.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_QtScriptOverloadTests_h
#define hifi_QtScriptOverloadTests_h

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtScript/QScriptValue>
#include <QtTest/QtTest>

#include <ScriptCallback.h>

/// Methods shaped as the scripting interfaces have them, with the overloads the WebAssembly engine adds.
class OverloadProbe : public QObject {
    Q_OBJECT
public:
    QString called;

    // as Assets.getAsset: options, scope and callback, the callback optional
    Q_INVOKABLE void fetch(QScriptValue options, QScriptValue scope, QScriptValue callback = QScriptValue()) {
        Q_UNUSED(options); Q_UNUSED(scope); Q_UNUSED(callback);
        called = "QScriptValue";
    }
    Q_INVOKABLE void fetch(const QVariant& options, const ScriptCallback& callback) {
        Q_UNUSED(options); Q_UNUSED(callback);
        called = "ScriptCallback";
    }

    // as Assets.uploadData: a plain value and a callback
    Q_INVOKABLE void upload(QString data, QScriptValue callback) {
        Q_UNUSED(data); Q_UNUSED(callback);
        called = "QScriptValue";
    }
    Q_INVOKABLE void upload(const QString& data, const ScriptCallback& callback) {
        Q_UNUSED(data); Q_UNUSED(callback);
        called = "ScriptCallback";
    }

    // results as QVariants, as Model's and Window's methods return them
    Q_INVOKABLE QVariant nothing() { return QVariant::fromValue(nullptr); }
    Q_INVOKABLE QVariant noObject() { return QVariant::fromValue<QObject*>(nullptr); }
    Q_INVOKABLE QVariant invalid() { return QVariant(); }
    Q_INVOKABLE QVariant text() { return QVariant(QString("chosen")); }
    Q_INVOKABLE QVariant flag() { return QVariant(true); }

    // as the controller builders' from(): a twin with no callback in it
    Q_INVOKABLE void pick(QScriptValue value) {
        Q_UNUSED(value);
        called = "QScriptValue";
    }
    Q_INVOKABLE void pick(const QVariant& value) {
        Q_UNUSED(value);
        called = "QVariant";
    }
};

/// Which members the QtScript engine shows a script, by access: the WebAssembly engine shows the same.
class AccessProbe : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE int publicInvokable() { return 1; }
public slots:
    int publicSlot() { return 2; }
protected:
    Q_INVOKABLE int protectedInvokable() { return 3; }
protected slots:
    int protectedSlot() { return 4; }
private:
    Q_INVOKABLE int privateInvokable() { return 5; }
private slots:
    int privateSlot() { return 6; }
};

/// Enum results: Qt's own (Q_ENUM) and one Qt doesn't know (a class that is not a QObject, no Q_ENUM), as
/// RefreshRateManager's were when Performance.getRefreshRateRegime returned one.
class EnumProbe : public QObject {
    Q_OBJECT
public:
    enum Mode { Idle, Stopped = 10 };
    Q_ENUM(Mode)
    struct Plain {
        enum Level { Low, High = 7 };
    };
    Q_INVOKABLE Mode currentMode() const { return Stopped; }
    Q_INVOKABLE Plain::Level plainLevel() const { return Plain::High; }
};

class QtScriptOverloadTests : public QObject {
    Q_OBJECT
private slots:
    void testCallbackOverloadIsNeverChosen();
    void testPlainValueTwinIsAmbiguous();
    void testMembersByAccess();
    void testVariantResults();
    void testEnumResults();
};

#endif // hifi_QtScriptOverloadTests_h
