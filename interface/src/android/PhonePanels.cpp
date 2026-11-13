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

#include "AndroidChatPage.h"
#include "InterfaceLogging.h"
#include "ui/Panels.h"

namespace {
    // the frame's +android_interface file, by its own path: there is none for the desktop, and loaded by the plain
    // name through the file selector its "../panel" import was looked for beside qml/darkride, not inside it
    const QUrl HOST_SOURCE { "qrc:///qml/darkride/+android_interface/PanelHost.qml" };
    // Android's dp: the screen's pixels per 1/160 inch
    const qreal DP_PER_INCH = 160.0;
    const QString CHAT = "Chat";
    const QString RADIO = "Radio";
    // in the app's assets' html folder (its build.gradle puts the radio's page there)
    const QString RADIO_PAGE = "radio/radio.html";
    const char* WEB_PANEL_CLASS = "io/highfidelity/hifiinterface/web/WebPanel";

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
    return !name.isEmpty() && (name == _showing || name == _webShowing);
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

void PhonePanels::openChat(const QString& link) {
    AndroidChatPage::instance()->open(link);
    setWebShowing(CHAT);
}

void PhonePanels::openRadio() {
    QAndroidJniObject::callStaticMethod<void>(WEB_PANEL_CLASS, "openAppPage",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QAndroidJniObject::fromString(RADIO).object<jstring>(), QAndroidJniObject::fromString(RADIO).object<jstring>(),
        QAndroidJniObject::fromString(RADIO_PAGE).object<jstring>());
    setWebShowing(RADIO);
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
    if (!name.isEmpty() && name == _webShowing) {
        // closed once the app says so (webPanelClosed)
        if (name == CHAT) {
            AndroidChatPage::instance()->close();
        } else {
            QAndroidJniObject::callStaticMethod<void>(WEB_PANEL_CLASS, "close", "(Ljava/lang/String;)V",
                QAndroidJniObject::fromString(name).object<jstring>());
        }
        return;
    }
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
        _padWasHidden = pad.isHiddenByRequest();
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

void PhonePanels::setWebShowing(const QString& name) {
    QString was = _webShowing;
    if (was == name) {
        return;
    }
    _webShowing = name;
    if (!was.isEmpty()) {
        emit openChanged(was, false);
    }
    if (!name.isEmpty()) {
        emit openChanged(name, true);
    }
}

void PhonePanels::webPanelClosed(const QString& name) {
    QMetaObject::invokeMethod(qApp, [name] {
        if (name == CHAT) {
            AndroidChatPage::instance()->closed();
        }
        // (one replaced by another has said so already)
        if (thePanels && thePanels->_webShowing == name) {
            thePanels->setWebShowing(QString());
        }
    }, Qt::QueuedConnection);
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

bool PhonePanels::backKey() {
    // the Panels chooser (hifi/+android_interface/PanelChooser.qml, the bar's fragment), over the world
    auto offscreenUi = DependencyManager::get<OffscreenUi>();
    if (offscreenUi && offscreenUi->getRootItem()) {
        auto chooser = offscreenUi->getRootItem()->findChild<QQuickItem*>("panelChooser");
        if (chooser && chooser->isVisible()) {
            chooser->setVisible(false);
            return true;
        }
    }
    if (!thePanels) {
        return false;
    }
    // a web panel over a QML one goes first
    if (!thePanels->_webShowing.isEmpty()) {
        thePanels->close(thePanels->_webShowing);
        return true;
    }
    if (!thePanels->_showing.isEmpty()) {
        thePanels->close(thePanels->_showing);
        return true;
    }
    return false;
}

extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_web_WebPanel_nativeClosed(JNIEnv*, jclass, jstring name) {
    PhonePanels::webPanelClosed(QAndroidJniObject(name).toString());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeClosePanel(JNIEnv*, jobject) {
    return PhonePanels::back();
}

// the Places screen's Chat (MainActivity): chat over that screen, outside any world, as the Panels card opens it
extern "C" JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_MainActivity_nativeOpenChat(JNIEnv*, jobject) {
    QMetaObject::invokeMethod(qApp, [] {
        DependencyManager::get<Panels>()->open(CHAT);
    }, Qt::QueuedConnection);
}

#endif
