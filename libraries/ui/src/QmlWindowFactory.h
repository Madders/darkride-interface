//
//  QmlWindowFactory.h
//  libraries/ui/src
//
//  The stand-in for the OverlayWindow and OverlayWebWindow constructors on the WebAssembly engine: a QObject
//  with a create method, registered under the constructor's name through FunctionTwins, so that the engine
//  gives a module (and the JavaScript shim, which wraps it back into a constructor) a way to open the QML and
//  web windows a QtScript script opened with `new OverlayWindow(...)`. The windows it makes live until the
//  factory is told to dispose of them or the script's engine is gone.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_QmlWindowFactory_h
#define hifi_QmlWindowFactory_h

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

class QmlWindowFactory : public QObject {
    Q_OBJECT
public:
    enum Kind { Window, WebWindow };

    QmlWindowFactory(Kind kind, bool restricted, QObject* parent = nullptr) : QObject(parent), _kind(kind), _restricted(restricted) {}

    /// A window from the properties `new OverlayWindow(properties)` takes (title, source, width, height,
    /// visible, frameless, enabled); null when the window cannot be made.
    Q_INVOKABLE QObject* create(const QVariantMap& properties);
    /// Deletes a window this factory made.
    Q_INVOKABLE void dispose(QObject* window);

private:
    Kind _kind;
    bool _restricted;
};

#endif // hifi_QmlWindowFactory_h
