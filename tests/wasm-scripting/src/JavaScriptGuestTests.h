//
//  JavaScriptGuestTests.h
//  tests/wasm-scripting/src
//
//  The JavaScript guest (tools/wasm-js, darkride-js.wasm) driven through the host interface with a stand-in
//  for the engine's Script object: a script's calls, properties, signals, callbacks, timers, includes,
//  modules and errors, without a client. Skipped when the module has not been built.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_JavaScriptGuestTests_h
#define hifi_JavaScriptGuestTests_h

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtTest/QtTest>

#include <ValueCodec.h>

/// What the shim asks the engine's Script object for, answered from canned values.
class FakeScript : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName CONSTANT)
public:
    QString source;
    QString fileName() const { return "file:///test/main.js"; }
    QHash<QString, QString> files;              // url -> contents, for include and require
    QHash<int, ScriptCallback> timers;
    int nextTimer { 1 };
    QVariantMap dumped;
    QString dumpPath;

    Q_INVOKABLE QVariantMap scriptSource() const {
        QVariantMap program;
        program["fileName"] = fileName();
        program["source"] = source;
        return program;
    }
    Q_INVOKABLE QVariantMap scriptGlobalValues() const {
        QVariantMap values;
        QVariantMap pickType;
        pickType["Ray"] = 1;
        values["PickType"] = pickType;
        return values;
    }
    Q_INVOKABLE int setTimeout(const ScriptCallback& callback, int ms) { Q_UNUSED(ms); timers[nextTimer] = callback; return nextTimer++; }
    Q_INVOKABLE int setInterval(const ScriptCallback& callback, int ms) { return setTimeout(callback, ms); }
    Q_INVOKABLE void clearTimeout(int timer) { timers.remove(timer); }
    Q_INVOKABLE void clearInterval(int timer) { timers.remove(timer); }
    Q_INVOKABLE QString resolvePathFrom(const QString& path, const QString& parent) const {
        return QUrl(parent.isEmpty() ? fileName() : parent).resolved(QUrl(path)).toString();
    }
    Q_INVOKABLE QVariantMap resolveInclude(const QString& file, const QString& parent) const {
        QVariantMap result;
        result["url"] = resolvePathFrom(file, parent);
        return result;
    }
    Q_INVOKABLE QVariantMap resolveModule(const QString& id, const QString& relativeTo, const QString& parent) const {
        QVariantMap result;
        QString url = resolvePathFrom(id, relativeTo.isEmpty() ? parent : relativeTo);
        if (files.contains(url)) {
            result["url"] = url;
        } else {
            result["error"] = "Cannot find module '" + id + "'";
        }
        return result;
    }
    Q_INVOKABLE QVariantMap fetchScript(const QString& url, bool force) const {
        Q_UNUSED(force);
        QVariantMap result;
        result["url"] = url;
        result["success"] = files.contains(url);
        result["status"] = files.contains(url) ? "OK" : "Not found";
        if (files.contains(url)) {
            result["contents"] = files.value(url);
        }
        return result;
    }
    Q_INVOKABLE bool dumpApi(const QString& path, const QVariantMap& globals) { dumpPath = path; dumped = globals; return true; }
    Q_INVOKABLE void print(const QString& message) { printed << message; }
    QStringList printed;

signals:
    void update(float deltaTime);
    void scriptEnding();
};

/// A stand-in for the engine's Assets object, under its class name (the guest binds the scope forms of the
/// callbacks for that class): each call is recorded with its callback, for the test to call back.
class AssetScriptingInterface : public QObject {
    Q_OBJECT
public:
    struct Request {
        QString method;
        QVariantList values;
        ScriptCallback callback;
    };
    QList<Request> requests;

    Q_INVOKABLE QVariant getAsset(const QVariant& options, const ScriptCallback& callback) { return record("getAsset", { options }, callback); }
    Q_INVOKABLE QVariant getCacheStatus(const ScriptCallback& callback) { return record("getCacheStatus", {}, callback); }
    Q_INVOKABLE QVariant saveToCache(const QVariant& options, const ScriptCallback& callback) { return record("saveToCache", { options }, callback); }
    Q_INVOKABLE QVariant saveToCache(const QUrl& url, const QByteArray& data, const QVariantMap& metadata, const ScriptCallback& callback) {
        return record("saveToCache", { url, data, metadata }, callback);
    }
    Q_INVOKABLE QVariant uploadData(const QString& data, const ScriptCallback& callback) { return record("uploadData", { data }, callback); }

private:
    QVariant record(const QString& method, const QVariantList& values, const ScriptCallback& callback) {
        requests.append({ method, values, callback });
        return QVariant();
    }
};

class JavaScriptGuestTests : public QObject {
    Q_OBJECT
private slots:
    void testCallsPropertiesAndValues();
    void testSignalsCallbacksAndTimers();
    void testIncludeAndRequire();
    void testErrorsAreReportedNotFatal();
    void testApiDump();
    void testManyCalls();
    void testEntityScripts();
    void testObjectsAreReleased();
    void testScopedCallbacks();
    void testRunawayLoopIsInterrupted();
    void testRunawayEntityScriptIsNamed();
    void testErrorsAndPrintedNamesAsOnQtScript();
};

#endif // hifi_JavaScriptGuestTests_h
