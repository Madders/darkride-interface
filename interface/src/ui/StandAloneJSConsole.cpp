//
//  StandAloneJSConsole.cpp
//
//
//  Created by Clement on 1/17/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "StandAloneJSConsole.h"

#include <QtCore/QPointer>
#include <QtGui/QCloseEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDockWidget>

#include <Application.h>
#include <MainWindow.h>
#include <gl/GLHelpers.h>

#include "DockTitleBar.h"
#include "Panels.h"
#include "WindowLayout.h"

namespace {
    // what was typed into the console stops when its panel is closed (its close box, View > Panels, the shortcut),
    // not when it's only hidden for a moment (floating or docking it) or moved out of sight (tabbed behind another)
    class ConsoleDock : public QDockWidget {
    public:
        explicit ConsoleDock(QWidget* parent) : QDockWidget(CONSOLE_TITLE, parent) {}

        // the console itself, made the first time the panel shows: it starts a script engine of its own and reads
        // the autocomplete's doclets, which a start where nobody opens it shouldn't pay for (scripting's review)
        JSConsole* console() {
            if (!_console) {
                _console = new JSConsole(this);
                setWidget(_console);
            }
            return _console;
        }

    protected:
        void showEvent(QShowEvent* event) override {
            console();
            QDockWidget::showEvent(event);
        }
        void closeEvent(QCloseEvent* event) override {
            QDockWidget::closeEvent(event);
            if (event->isAccepted() && _console) {
                _console->stopTyped();
            }
        }

    private:
        QPointer<JSConsole> _console;
    };
}

QDockWidget* StandAloneJSConsole::dock() {
    if (_dock) {
        return _dock;
    }
    MainWindow* window = qApp->getWindow();
    if (!window || qApp->getLoginDialogPoppedUp()) {
        return nullptr;
    }
    gl::ViewChangeGuard guard;
    auto made = new ConsoleDock(window);
    made->setObjectName("ConsoleDock");
    made->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable);
    DockTitleBar::attach(made);
    // where the window's saved layout has it, asked before it's added anywhere; else floating along the bottom of the
    // window, a terminal's shape
    made->hide();
    if (!WindowLayout::instance()->restoreDock(made)) {
        window->addDockWidget(Qt::BottomDockWidgetArea, made);
        made->setFloating(true);
        Panels::placeFloating(made, window, QSize(CONSOLE_WIDTH, CONSOLE_HEIGHT));
    }
    WindowLayout::instance()->watch(made);
    _dock = made;
    return made;
}

JSConsole* StandAloneJSConsole::console() {
    auto made = static_cast<ConsoleDock*>(dock());
    return made ? made->console() : nullptr;
}

void StandAloneJSConsole::toggleConsole() {
    DependencyManager::get<Panels>()->toggle("Console");
}

bool StandAloneJSConsole::getActive() {
    QWidget* focus = QApplication::focusWidget();
    return _dock && focus && _dock->isAncestorOf(focus) && focus->isActiveWindow();
}
