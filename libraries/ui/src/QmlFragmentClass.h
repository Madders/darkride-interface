//
//  Created by Gabriel Calero & Cristian Duarte on Aug 25, 2017
//  Copyright 2017 High Fidelity, Inc.
//
//  The phone's bars (Android: the action bar, the audio bar, the stats bar), each a QML fragment over the 3D view.
//  Since 1.1.4 scripts run on the WebAssembly engine, so this is made through QmlWindowFactory (its Fragment kind,
//  registered as the script global QmlFragment on Android: QmlFragment.create({ qml: ... })), not a QtScript
//  constructor.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ui_QmlFragmentClass_h
#define hifi_ui_QmlFragmentClass_h

#include <mutex>

#include <QtCore/QHash>
#include <QtCore/QPointer>

#include "QmlWindowClass.h"

class QmlFragmentClass : public QmlWindowClass {
    Q_OBJECT

public:
    /// The fragment for properties.qml (a QML file under the app's qml folder), made on the main thread; the one
    /// already made for that file if there is one, as each bar is one on the screen whichever script asks for it.
    /// Null without a qml property.
    static QmlFragmentClass* createFragment(QVariantMap properties, bool restricted);

    QmlFragmentClass(bool restricted, QString id);
    ~QmlFragmentClass();

    /**jsdoc
     * Creates a new button, adds it to this and returns it.
     * @function QmlFragmentClass#addButton
     * @param properties {object} button properties
     * @returns {TabletButtonProxy}
     */
    Q_INVOKABLE QObject* addButton(const QVariant& properties);

    /*
     * TODO - not yet implemented
     */
    Q_INVOKABLE void removeButton(QObject* tabletButtonProxy);

protected:
    QString qmlSource() const override { return _qml; }

private:
    static std::mutex _mutex;
    static QHash<QString, QPointer<QmlFragmentClass>> _fragments;
    QString _qml;
};

#endif
