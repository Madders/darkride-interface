//
//  Created by Gabriel Calero & Cristian Duarte on Aug 25, 2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "QmlFragmentClass.h"

#include <QtCore/QThread>
#include <QtCore/QCoreApplication>

#include <shared/QtHelpers.h>

std::mutex QmlFragmentClass::_mutex;
QHash<QString, QPointer<QmlFragmentClass>> QmlFragmentClass::_fragments;

QmlFragmentClass* QmlFragmentClass::createFragment(QVariantMap properties, bool restricted) {
    const QString qml = properties.value("qml").toString();
    if (qml.isEmpty()) {
        qWarning() << "QmlFragmentClass: no qml in" << properties;
        return nullptr;
    }
    {
        // (a closed one has no QML any more: made again)
        std::lock_guard<std::mutex> guard(_mutex);
        QPointer<QmlFragmentClass> made = _fragments.value(qml);
        if (made && made->_qmlWindow) {
            return made.data();
        }
    }

    // not under the lock: the main thread, which this waits for, takes it when a fragment goes
    QmlFragmentClass* fragment = new QmlFragmentClass(restricted, qml);
    if (QThread::currentThread() != qApp->thread()) {
        fragment->moveToThread(qApp->thread());
        BLOCKING_INVOKE_METHOD(fragment, "initQml", Q_ARG(QVariantMap, properties));
    } else {
        fragment->initQml(properties);
    }
    std::lock_guard<std::mutex> guard(_mutex);
    _fragments.insert(qml, fragment);
    return fragment;
}

QmlFragmentClass::QmlFragmentClass(bool restricted, QString id) : QmlWindowClass(restricted), _qml(id) { }

QmlFragmentClass::~QmlFragmentClass() {
    std::lock_guard<std::mutex> guard(_mutex);
    if (_fragments.value(_qml) == this) {
        _fragments.remove(_qml);
    }
}

QObject* QmlFragmentClass::addButton(const QVariant& properties) {
    QVariant resultVar;
    Qt::ConnectionType connectionType = Qt::AutoConnection;

    if (QThread::currentThread() != _qmlWindow->thread()) {
        connectionType = Qt::BlockingQueuedConnection;
    }
    bool hasResult = QMetaObject::invokeMethod(_qmlWindow, "addButton", connectionType,
                                               Q_RETURN_ARG(QVariant, resultVar), Q_ARG(QVariant, properties));
    if (!hasResult) {
        qWarning() << "QmlFragmentClass addButton has no result";
        return nullptr;
    }

    QObject* qmlButton = qvariant_cast<QObject*>(resultVar);
    if (!qmlButton) {
        qWarning() << "QmlFragmentClass addButton result not a QObject";
        return nullptr;
    }
    return qmlButton;
}

void QmlFragmentClass::removeButton(QObject* button) {
}
