//
//  MetaObjectDescription.cpp
//  libraries/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MetaObjectDescription.h"

#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QVariantList>

namespace MetaObjectDescription {

namespace {

const QMetaObject* QOBJECT_META = &QObject::staticMetaObject;

QVariantList parameters(const QMetaMethod& method) {
    QVariantList list;
    const auto types = method.parameterTypes();
    const auto names = method.parameterNames();
    for (int p = 0; p < types.size(); ++p) {
        QVariantMap parameter;
        parameter["type"] = QString::fromUtf8(types[p]);
        parameter["name"] = QString::fromUtf8(names.value(p));
        list.append(parameter);
    }
    return list;
}

} // namespace

QVariantMap describe(const QObject* object, const Options& options) {
    return object ? describe(object->metaObject(), options) : QVariantMap();
}

QVariantMap describe(const QMetaObject* meta, const Options& options) {
    QVariantMap description;
    if (!meta) {
        return description;
    }
    description["class"] = QString::fromUtf8(meta->className());
    QVariantList methods, signalList, properties, enums;
    for (int i = 0; i < meta->methodCount(); ++i) {
        QMetaMethod method = meta->method(i);
        // the members QtScript shows a script: public and protected ones (AudioScriptingInterface's playSound is
        // a protected invokable), not private ones
        if (method.access() == QMetaMethod::Private || method.methodType() == QMetaMethod::Constructor) {
            continue;
        }
        const QMetaObject* declaring = method.enclosingMetaObject();
        if (declaring == QOBJECT_META && !options.includeQObjectMembers) {
            continue;
        }
        if (method.name() == "deleteLater" && !options.includeDeleteLater) {
            continue;
        }
        QVariantMap entry;
        entry["name"] = QString::fromUtf8(method.name());
        entry["signature"] = QString::fromUtf8(method.methodSignature());
        entry["index"] = i;
        entry["declaringClass"] = QString::fromUtf8(declaring->className());
        entry["parameters"] = parameters(method);
        if (method.methodType() == QMetaMethod::Signal) {
            signalList.append(entry);
        } else {
            entry["returns"] = QString::fromUtf8(method.typeName());
            entry["kind"] = method.methodType() == QMetaMethod::Slot ? "slot" : "invokable";
            methods.append(entry);
        }
    }
    for (int i = 0; i < meta->propertyCount(); ++i) {
        QMetaProperty property = meta->property(i);
        const QMetaObject* declaring = property.enclosingMetaObject();
        if (declaring == QOBJECT_META && !options.includeQObjectMembers) {
            continue;
        }
        QVariantMap entry;
        entry["name"] = QString::fromUtf8(property.name());
        entry["type"] = QString::fromUtf8(property.typeName());
        entry["index"] = i;
        entry["readable"] = property.isReadable();
        entry["writable"] = property.isWritable();
        entry["constant"] = property.isConstant();
        entry["declaringClass"] = QString::fromUtf8(declaring->className());
        if (property.hasNotifySignal()) {
            entry["notify"] = QString::fromUtf8(property.notifySignal().name());
        }
        if (property.isEnumType()) {
            entry["enum"] = QString::fromUtf8(property.enumerator().name());
        }
        properties.append(entry);
    }
    for (int i = 0; i < meta->enumeratorCount(); ++i) {
        QMetaEnum enumerator = meta->enumerator(i);
        QVariantMap entry;
        entry["name"] = QString::fromUtf8(enumerator.name());
        QVariantMap values;
        for (int k = 0; k < enumerator.keyCount(); ++k) {
            values[QString::fromUtf8(enumerator.key(k))] = enumerator.value(k);
        }
        entry["values"] = values;
        enums.append(entry);
    }
    description["methods"] = methods;
    description["signals"] = signalList;
    description["properties"] = properties;
    description["enums"] = enums;
    return description;
}

} // namespace MetaObjectDescription
