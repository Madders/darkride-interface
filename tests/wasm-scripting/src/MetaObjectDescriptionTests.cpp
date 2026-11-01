//
//  MetaObjectDescriptionTests.cpp
//  tests/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MetaObjectDescriptionTests.h"

#include <QtCore/QJsonDocument>

#include <MetaObjectDescription.h>
#include <ValueCodec.h>

#include "TestObjects.h"

QTEST_GUILESS_MAIN(MetaObjectDescriptionTests)

namespace {

QVariantList entries(const QVariantMap& description, const char* list, const QString& name) {
    QVariantList found;
    for (const QVariant& entry : description.value(list).toList()) {
        if (entry.toMap().value("name").toString() == name) {
            found.append(entry);
        }
    }
    return found;
}

QVariantMap entry(const QVariantMap& description, const char* list, const QString& name) {
    QVariantList found = entries(description, list, name);
    return found.isEmpty() ? QVariantMap() : found.first().toMap();
}

} // namespace

void MetaObjectDescriptionTests::testMethods() {
    QVariantMap description = MetaObjectDescription::describe(&Probe::staticMetaObject);
    QCOMPARE(description.value("class").toString(), QString("Probe"));

    // an overloaded invokable is listed once per signature
    QVariantList adds = entries(description, "methods", "add");
    QCOMPARE(adds.size(), 2);
    QCOMPARE(adds[0].toMap().value("signature").toString(), QString("add(int,int)"));
    QCOMPARE(adds[0].toMap().value("returns").toString(), QString("int"));
    QCOMPARE(adds[0].toMap().value("kind").toString(), QString("invokable"));
    QCOMPARE(adds[0].toMap().value("declaringClass").toString(), QString("Probe"));
    QVariantList parameters = adds[0].toMap().value("parameters").toList();
    QCOMPARE(parameters.size(), 2);
    QCOMPARE(parameters[0].toMap().value("name").toString(), QString("a"));
    QCOMPARE(parameters[0].toMap().value("type").toString(), QString("int"));
    QCOMPARE(adds[1].toMap().value("signature").toString(), QString("add(QString,QString)"));
    QCOMPARE(adds[1].toMap().value("returns").toString(), QString("QString"));
    // the index is the meta-object's
    int index = adds[0].toMap().value("index").toInt();
    QCOMPARE(Probe::staticMetaObject.method(index).methodSignature(), QByteArray("add(int,int)"));

    QVariantMap scale = entry(description, "methods", "scale");
    QCOMPARE(scale.value("returns").toString(), QString("glm::vec3"));
    QCOMPARE(scale.value("parameters").toList()[0].toMap().value("type").toString(), QString("glm::vec3"));
    QCOMPARE(scale.value("parameters").toList()[1].toMap().value("type").toString(), QString("float"));

    // a slot is a method of kind slot; a private invokable is not listed, a protected one is (as QtScript shows it);
    // a signal is not a method
    QCOMPARE(entry(description, "methods", "reset").value("kind").toString(), QString("slot"));
    QVERIFY(entry(description, "methods", "hidden").isEmpty());
    QCOMPARE(entry(description, "methods", "guarded").value("kind").toString(), QString("invokable"));
    QVERIFY(entry(description, "methods", "countChanged").isEmpty());
    // deleteLater is left out by default, QObject's other members are kept and marked
    QVERIFY(entry(description, "methods", "deleteLater").isEmpty());
    QVariantMap objectName = entry(description, "properties", "objectName");
    QCOMPARE(objectName.value("declaringClass").toString(), QString("QObject"));
}

void MetaObjectDescriptionTests::testSignals() {
    QVariantMap description = MetaObjectDescription::describe(&Probe::staticMetaObject);
    QVariantMap countChanged = entry(description, "signals", "countChanged");
    QCOMPARE(countChanged.value("signature").toString(), QString("countChanged(int)"));
    QCOMPARE(countChanged.value("parameters").toList().size(), 1);
    QCOMPARE(countChanged.value("parameters").toList()[0].toMap().value("name").toString(), QString("count"));
    QVERIFY(!countChanged.contains("returns"));
    QVERIFY(!countChanged.contains("kind"));
    QVariantMap named = entry(description, "signals", "named");
    QCOMPARE(named.value("parameters").toList()[1].toMap().value("type").toString(), QString("bool"));
    QCOMPARE(entry(description, "signals", "bare").value("parameters").toList().size(), 0);
    QCOMPARE(entry(description, "signals", "destroyed").value("declaringClass").toString(), QString("QObject"));
    int index = countChanged.value("index").toInt();
    QCOMPARE(Probe::staticMetaObject.method(index).name(), QByteArray("countChanged"));
}

void MetaObjectDescriptionTests::testProperties() {
    QVariantMap description = MetaObjectDescription::describe(&Probe::staticMetaObject);
    QVariantMap count = entry(description, "properties", "count");
    QCOMPARE(count.value("type").toString(), QString("int"));
    QCOMPARE(count.value("readable").toBool(), true);
    QCOMPARE(count.value("writable").toBool(), true);
    QCOMPARE(count.value("constant").toBool(), false);
    QCOMPARE(count.value("notify").toString(), QString("countChanged"));
    QVERIFY(!count.contains("enum"));
    QCOMPARE(Probe::staticMetaObject.property(count.value("index").toInt()).name(), "count");

    QVariantMap label = entry(description, "properties", "label");
    QCOMPARE(label.value("writable").toBool(), false);
    QCOMPARE(label.value("constant").toBool(), true);
    QVERIFY(!label.contains("notify"));

    QVariantMap mode = entry(description, "properties", "mode");
    QCOMPARE(mode.value("type").toString(), QString("Mode"));
    QCOMPARE(mode.value("enum").toString(), QString("Mode"));

    QCOMPARE(entry(description, "properties", "position").value("type").toString(), QString("glm::vec3"));
}

void MetaObjectDescriptionTests::testEnums() {
    QVariantMap description = MetaObjectDescription::describe(&Probe::staticMetaObject);
    QVariantList enums = description.value("enums").toList();
    QCOMPARE(enums.size(), 1);
    QCOMPARE(enums[0].toMap().value("name").toString(), QString("Mode"));
    QVariantMap values = enums[0].toMap().value("values").toMap();
    QCOMPARE(values.size(), 3);
    QCOMPARE(values.value("Idle").toInt(), 0);
    QCOMPARE(values.value("Running").toInt(), 1);
    QCOMPARE(values.value("Stopped").toInt(), 10);
}

void MetaObjectDescriptionTests::testOptions() {
    MetaObjectDescription::Options options;
    options.includeQObjectMembers = false;
    QVariantMap own = MetaObjectDescription::describe(&Probe::staticMetaObject, options);
    QVERIFY(entry(own, "properties", "objectName").isEmpty());
    QVERIFY(entry(own, "signals", "destroyed").isEmpty());
    QVERIFY(!entry(own, "signals", "countChanged").isEmpty());
    for (const QVariant& method : own.value("methods").toList()) {
        QCOMPARE(method.toMap().value("declaringClass").toString(), QString("Probe"));
    }

    options.includeDeleteLater = true;
    QVariantMap withDelete = MetaObjectDescription::describe(&Probe::staticMetaObject, options);
    // deleteLater is QObject's, so it needs both options
    QVERIFY(entry(withDelete, "methods", "deleteLater").isEmpty());
    options.includeQObjectMembers = true;
    withDelete = MetaObjectDescription::describe(&Probe::staticMetaObject, options);
    QCOMPARE(entry(withDelete, "methods", "deleteLater").value("kind").toString(), QString("slot"));

    // an object gives its meta-object's description; null gives an empty one
    Probe probe;
    QCOMPARE(MetaObjectDescription::describe(&probe), MetaObjectDescription::describe(&Probe::staticMetaObject));
    QVERIFY(MetaObjectDescription::describe(static_cast<const QObject*>(nullptr)).isEmpty());
    QVERIFY(MetaObjectDescription::describe(static_cast<const QMetaObject*>(nullptr)).isEmpty());
}

void MetaObjectDescriptionTests::testRoundTripsThroughCodecAndJson() {
    QVariantMap description = MetaObjectDescription::describe(&Probe::staticMetaObject);
    ValueCodec codec;
    QVariant back;
    QString error;
    QVERIFY2(codec.decode(codec.encode(description), back, error), qPrintable(error));
    QCOMPARE(back.toMap(), description);
    // and as JSON (Script.dumpApi): the same record, every value representable
    QJsonDocument json = QJsonDocument::fromVariant(description);
    QVERIFY(!json.isNull());
    QCOMPARE(json.object().value("class").toString(), QString("Probe"));
    QCOMPARE(QJsonDocument::fromJson(json.toJson()).toVariant().toMap(), description);
}

// TestObjects.h has no source file of its own, so its moc output is pulled in here.
#include "moc_TestObjects.cpp"
