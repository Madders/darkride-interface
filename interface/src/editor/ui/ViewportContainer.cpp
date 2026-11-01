//
//  ViewportContainer.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ViewportContainer.h"

#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

#include <gl/GLHelpers.h>

#include "../../InterfaceLogging.h"

#ifdef Q_OS_MACOS
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace {
    const int SETTLE_MS = 250;   // a splitter drag has stopped when nothing moved for this long
    const int RESCALE_MS = 100;  // Qt takes a pass or two of its event loop to settle a new scale

#ifdef Q_OS_MACOS
    // [CATransaction flush]: Core Animation commits the window's pending layer changes now, the GL view's new surface
    // among them, rather than at the end of the main thread's loop pass
    void flushCoreAnimation() {
        static Class transaction = objc_getClass("CATransaction");
        static SEL flush = sel_registerName("flush");
        if (transaction) {
            ((void (*)(id, SEL))objc_msgSend)((id)transaction, flush);
        }
    }
#endif
}

ViewportContainer::ViewportContainer(QWidget* viewport, QWidget* parent) : QWidget(parent), _viewport(viewport) {
    setObjectName("ViewportContainer");
    setStyleSheet("#ViewportContainer { background: #000000; }");
    _viewport->setParent(this);
    _viewport->move(0, 0);
    _settle.setSingleShot(true);
    _settle.setInterval(SETTLE_MS);
    connect(&_settle, &QTimer::timeout, this, &ViewportContainer::applySize);
    _rescale.setSingleShot(true);
    _rescale.setInterval(RESCALE_MS);
    connect(&_rescale, &QTimer::timeout, this, &ViewportContainer::reapplySize);
#ifdef Q_OS_MACOS
    // a change to the view is put in place before the presenting thread may draw again (see gl::ViewChangeGuard)
    gl::setViewChangeFlush(&flushCoreAnimation);
#endif
}

void ViewportContainer::setDeferResizes(bool defer) {
    _defer = defer;
    if (!defer) {
        _settle.stop();
        applySize();
    }
}

void ViewportContainer::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (_defer) {
        _settle.start();
    } else {
        applySize();
    }
}

void ViewportContainer::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
#ifndef Q_OS_MACOS
    // Windows and Linux, where Qt does the scaling (main.cpp); macOS scales the view's surface itself
    QWindow* handle = window()->windowHandle();
    if (handle && !_screenChanged) {
        _screenChanged = connect(handle, &QWindow::screenChanged, this, [this](QScreen*) {
            watchScreen();
            _rescale.start();
        });
        watchScreen();
    }
#endif
}

void ViewportContainer::watchScreen() {
    QObject::disconnect(_screenGeometry);
    QObject::disconnect(_screenDpi);
    QWindow* handle = window()->windowHandle();
    if (QScreen* screen = handle ? handle->screen() : nullptr) {
        // a new display scale (Windows' Settings > Display > Scale) changes the screen's size in points and its dpi
        _screenGeometry = connect(screen, &QScreen::geometryChanged, this, [this](const QRect&) { _rescale.start(); });
        _screenDpi = connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this](qreal) { _rescale.start(); });
    }
}

void ViewportContainer::setInset(const QMargins& inset) {
    setContentsMargins(inset);
    applySize();
}

void ViewportContainer::applySize() {
    const QRect area = contentsRect();
    if (_viewport->geometry() != area) {
        // never while a frame is drawn to it, nor just after (see gl::ViewChangeGuard): toggling build mode, whose
        // panels resize the view, crashed in Apple's GL driver, clearing the view while it was being resized
        gl::ViewChangeGuard guard;
        _viewport->setGeometry(area);
#ifdef Q_OS_MACOS
        // the Mac's GL surface can keep its old size after this one: the picture was drawn at the old shape into the
        // new one, squeezed sideways with Chat docked until the dock closed (Caity, beta zip, 10-29). Put it again once
        // the layout has settled, as after a change of scale
        _rescale.start();
#endif
    }
}

void ViewportContainer::reapplySize() {
    // After a change of scale Qt can size the view's native window at the old scale while the layout settles at the
    // new one, and the view's size in points may still come out as applySize put it, which then leaves it: the view's
    // window kept its old pixels and the picture was stretched into the new shape (squashed sideways with Chat docked,
    // whose width stays the same in points). Put it again, through a different size so Qt resizes the native window.
    // Only on a change of scale or screen: applySize's skip of an unchanged size stays for the GL resize crash
    const QRect area = contentsRect();
    if (area.height() < 2) {
        return;
    }
    qCInfo(interfaceapp) << "ViewportContainer: the view's size put again after a change of scale or screen:" << area.size()
                         << "points at" << devicePixelRatioF();
    gl::ViewChangeGuard guard;
    _viewport->setGeometry(area.adjusted(0, 0, 0, -1));
    _viewport->setGeometry(area);
}
