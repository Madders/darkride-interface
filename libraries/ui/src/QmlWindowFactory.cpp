//
//  QmlWindowFactory.cpp
//  libraries/ui/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "QmlWindowFactory.h"

#include <FunctionTwins.h>

#include "QmlWebWindowClass.h"
#include "QmlWindowClass.h"

QObject* QmlWindowFactory::create(const QVariantMap& properties) {
    QmlWindowClass* window = _kind == WebWindow ? QmlWebWindowClass::createWebWindow(properties, _restricted)
                                                : QmlWindowClass::createWindow(properties, _restricted);
    if (window) {
        // the window lives on the main thread; it goes when the script's engine goes, as before
        connect(this, &QObject::destroyed, window, &QObject::deleteLater);
    }
    return window;
}

void QmlWindowFactory::dispose(QObject* window) {
    if (window && qobject_cast<QmlWindowClass*>(window)) {
        window->deleteLater();
    }
}

// The twins of the four constructors the application registers, registered when this library loads.
static const bool twinsRegistered = [] {
    FunctionTwins::add(reinterpret_cast<const void*>(&QmlWindowClass::constructor),
                       [] { return new QmlWindowFactory(QmlWindowFactory::Window, false); });
    FunctionTwins::add(reinterpret_cast<const void*>(&QmlWindowClass::restricted_constructor),
                       [] { return new QmlWindowFactory(QmlWindowFactory::Window, true); });
    FunctionTwins::add(reinterpret_cast<const void*>(&QmlWebWindowClass::constructor),
                       [] { return new QmlWindowFactory(QmlWindowFactory::WebWindow, false); });
    FunctionTwins::add(reinterpret_cast<const void*>(&QmlWebWindowClass::restricted_constructor),
                       [] { return new QmlWindowFactory(QmlWindowFactory::WebWindow, true); });
    return true;
}();
