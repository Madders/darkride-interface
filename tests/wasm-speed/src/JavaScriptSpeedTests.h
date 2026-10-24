//
//  JavaScriptSpeedTests.h
//  tests/wasm-speed/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_JavaScriptSpeedTests_h
#define hifi_JavaScriptSpeedTests_h

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>
#include <QtTest/QtTest>

#include <glm/glm.hpp>

#include <RegisteredMetaTypes.h>

/// The object each engine's copy of the benchmark answers to: `run` starts one test in that engine, `report` takes
/// the value the test's loop produced, and the rest are the cheapest calls and properties a script can reach.
class SpeedProbe : public QObject {
    Q_OBJECT
    Q_PROPERTY(glm::vec3 position READ position WRITE setPosition)
public:
    QVariantMap checks; // test name -> the value its loop produced

    Q_INVOKABLE void noop() {}
    Q_INVOKABLE int add(int a, int b) { return a + b; }
    Q_INVOKABLE void report(const QString& name, const QVariant& check) { checks[name] = check; }

    glm::vec3 position() const { return _position; }
    void setPosition(const glm::vec3& position) { _position = position; }

signals:
    void run(const QString& name, int iterations);

private:
    glm::vec3 _position { 1.0f, 2.0f, 3.0f };
};

/// What the JavaScript guest's runtime asks of `Script` when it starts a program.
class SpeedScript : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName CONSTANT)
public:
    QString source;
    QStringList printed;

    QString fileName() const { return "file:///speed/bench.js"; }

    Q_INVOKABLE QVariantMap scriptSource() const {
        QVariantMap program;
        program["fileName"] = fileName();
        program["source"] = source;
        return program;
    }
    Q_INVOKABLE QVariantMap scriptGlobalValues() const { return QVariantMap(); }
    Q_INVOKABLE QString resolvePathFrom(const QString& path, const QString& parent) const {
        return QUrl(parent.isEmpty() ? fileName() : parent).resolved(QUrl(path)).toString();
    }
    Q_INVOKABLE void print(const QString& message) { printed << message; }

signals:
    void update(float deltaTime);
    void scriptEnding();
};

class JavaScriptSpeedTests : public QObject {
    Q_OBJECT
private slots:
    void testSpeed();
};

#endif // hifi_JavaScriptSpeedTests_h
