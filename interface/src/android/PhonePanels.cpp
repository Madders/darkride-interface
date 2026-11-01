//
//  PhonePanels.cpp
//  interface/src/android
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "PhonePanels.h"

#if defined(Q_OS_ANDROID)

#include <atomic>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariant>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQuick/QQuickItem>
#include <QtAndroidExtras/QAndroidJniObject>

#include <DependencyManager.h>
#include <OffscreenUi.h>
#include <VirtualPadManager.h>

#include "InterfaceLogging.h"

namespace {
    // the frame's +android_interface file: there is none for the desktop
    const QUrl HOST_SOURCE { "qrc:///qml/darkride/PanelHost.qml" };
    // Android's dp: the screen's pixels per 1/160 inch
    const qreal DP_PER_INCH = 160.0;

    QPointer<PhonePanels> thePanels;
    // whether a QML panel shows, for the phone's back, which asks on Android's UI thread
    std::atomic<bool> qmlPanelShowing { false };
}

PhonePanels::PhonePanels(QObject* parent) : QObject(parent) {
    thePanels = this;
}

PhonePanels::~PhonePanels() {
    // (Panels goes first: nothing is told)
    if (_host) {
        delete _host.data();
    }
}

bool PhonePanels::isOpen(const QString& name) const {
    return !name.isEmpty() && name == _showing;
}

void PhonePanels::openQml(const QString& name, const QUrl& source) {
    _wanted = name;
    _wantedSource = source;
    if (_host) {
        showWanted();
    } else {
        makeHost();
    }
}

void PhonePanels::makeHost() {
    auto offscreenUi = DependencyManager::get<OffscreenUi>();
    if (!offscreenUi || !offscreenUi->getRootItem()) {
        qCWarning(interfaceapp) << "Panels: no QML UI to show" << _wanted << "in yet";
        return;
    }
    QPointer<PhonePanels> self(this);
    offscreenUi->load(HOST_SOURCE, [self](QQmlContext*, QQuickItem* made) {
        if (!self || !made) {
            return;
        }
        if (self->_host) {
            made->deleteLater();   // a second tap while the first was being made
            return;
        }
        self->_host = made;
        // shown once it's complete: this is before its bindings are
        QMetaObject::invokeMethod(self.data(), [self] {
            if (self) {
                self->showWanted();
            }
        }, Qt::QueuedConnection);
    });
}

void PhonePanels::showWanted() {
    if (!_host || _wanted.isEmpty()) {
        return;
    }
    QScreen* screen = QGuiApplication::primaryScreen();
    const qreal density = screen ? screen->physicalDotsPerInch() / DP_PER_INCH : 2.0;
    _host->setProperty("density", density);
    const QString name = _wanted;
    const QVariant panelName(name);
    const QVariant source(_wantedSource.toString());
    QMetaObject::invokeMethod(_host.data(), "showPanel", Q_ARG(QVariant, panelName), Q_ARG(QVariant, source));
    _wanted.clear();
    setShowing(name);
}

void PhonePanels::close(const QString& name) {
    if (name == _wanted) {
        _wanted.clear();
    }
    if (name.isEmpty() || name != _showing) {
        return;
    }
    if (_host) {
        // a page that keeps something on while hidden hears it's closed, as in a dock (QmlDock's panelClosed)
        QQuickItem* item = page(name);
        if (item && item->metaObject()->indexOfMethod("panelClosed()") >= 0) {
            QMetaObject::invokeMethod(item, "panelClosed");
        }
        QMetaObject::invokeMethod(_host.data(), "hidePanel");
    }
    setShowing(QString());
}

void PhonePanels::setShowing(const QString& name) {
    QString was = _showing;
    if (was == name) {
        return;
    }
    _showing = name;
    qmlPanelShowing = !name.isEmpty();
    // the virtual pad under a panel would still be drawn over it, and the panel takes its touches
    auto& pad = VirtualPad::Manager::instance();
    if (was.isEmpty()) {
        _padWasHidden = pad.isHidden();
        pad.hide(true);
    } else if (name.isEmpty()) {
        pad.hide(_padWasHidden);
    }
    if (!was.isEmpty()) {
        emit openChanged(was, false);
    }
    if (!name.isEmpty()) {
        emit openChanged(name, true);
    }
}

QQuickItem* PhonePanels::page(const QString& name) const {
    if (!_host || name.isEmpty() || _host->property("panelName").toString() != name) {
        return nullptr;
    }
    return qobject_cast<QQuickItem*>(_host->property("page").value<QObject*>());
}

void PhonePanels::release() {
    if (!_showing.isEmpty()) {
        setShowing(QString());
    }
    if (_host) {
        delete _host.data();   // the frame, its loader and the page
    }
}

bool PhonePanels::back() {
    if (!qmlPanelShowing) {
        return false;
    }
    QMetaObject::invokeMethod(qApp, [] {
        if (thePanels) {
            thePanels->close(thePanels->_showing);
        }
    }, Qt::QueuedConnection);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeClosePanel(JNIEnv*, jobject) {
    return PhonePanels::back();
}

#endif
