//
//  WindowCursor.cpp
//  interface/src
//
//  The system cursor over the client's window, in step with the compositor's icon (see WindowCursor.h). The code
//  of Application's showCursor, updateSystemCursor and checkChangeCursor, with the window and the GL widget passed
//  to apply() instead of read as members.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "WindowCursor.h"

#include <QtCore/QMutexLocker>
#include <QtWidgets/QWidget>

void WindowCursor::show(const Cursor::Icon& icon) {
    QMutexLocker locker(&_lock);

    auto managedCursor = Cursor::Manager::instance().getCursor();
    auto curIcon = managedCursor->getIcon();
    if (curIcon != icon) {
        managedCursor->setIcon(icon);
        curIcon = icon;
    }
    _desiredShape = icon == Cursor::Icon::SYSTEM ? Qt::ArrowCursor : Qt::BlankCursor;
    _needsChange = true;
}

void WindowCursor::setSystemShape(Qt::CursorShape shape) {
    QMutexLocker locker(&_lock);

    if (_desiredShape != Qt::BlankCursor && _desiredShape != shape) {
        _desiredShape = shape;
        _needsChange = true;
    }
}

void WindowCursor::apply(QWidget* window, QWidget* glWidget) {
    QMutexLocker locker(&_lock);
    if (_needsChange) {
#ifdef Q_OS_MACOS
        Q_UNUSED(glWidget);
        auto cursorTarget = window; // macOS doesn't seem to provide for hiding the cursor only on the GL widget
#else
        Q_UNUSED(window);
        // On windows and linux, hiding the top level cursor also means it's invisible when hovering over the
        // window menu, which is a pain, so only hide it for the GL surface
        auto cursorTarget = glWidget;
#endif
        cursorTarget->setCursor(_desiredShape);

        _needsChange = false;
    }
}
