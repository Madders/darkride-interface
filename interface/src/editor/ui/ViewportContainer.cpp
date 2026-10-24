//
//  ViewportContainer.cpp
//  interface/src/editor/ui
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ViewportContainer.h"

#include <QtGui/QResizeEvent>

#include <gl/GLHelpers.h>

#ifdef Q_OS_MACOS
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace {
    const int SETTLE_MS = 250;   // a splitter drag has stopped when nothing moved for this long

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
    }
}
