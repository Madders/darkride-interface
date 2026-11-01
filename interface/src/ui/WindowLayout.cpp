//
//  WindowLayout.cpp
//  interface/src/ui
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "WindowLayout.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtWidgets/QDockWidget>

#include <MainWindow.h>
#include <gl/GLHelpers.h>

#include "Application.h"
#include "DockTitleBar.h"
#include "InterfaceLogging.h"
#include "Panels.h"
#include "SystemToolbar.h"
#include "chat/ChatWindow.h"
#include "editor/Editor.h"

namespace {
    // long enough that a separator's drag or a window's resize makes one save, short enough that a quit right after a
    // change still finds it saved (nothing is saved while quitting)
    const int SAVE_DELAY_MS = 400;
}

WindowLayout* WindowLayout::instance() {
    static QPointer<WindowLayout> theInstance;
    if (!theInstance) {
        theInstance = new WindowLayout();
    }
    return theInstance;
}

WindowLayout::WindowLayout() : QObject(qApp) {
    _saveTimer.setSingleShot(true);
    _saveTimer.setInterval(SAVE_DELAY_MS);
    connect(&_saveTimer, &QTimer::timeout, this, &WindowLayout::save);
}

void WindowLayout::restore() {
    auto window = qApp->getWindow();
    if (_restored || !window) {
        return;
    }
    _restored = true;

    // chat's panel exists before the restore, so the layout finds it by its name ("ChatDock"); it is already in the
    // window (ChatWindow::dock), floating at its first place, which a first start keeps
    QDockWidget* chat = ChatWindow::dock();
    if (!qobject_cast<DockTitleBar*>(chat->titleBarWidget())) {
        chat->setTitleBarWidget(new DockTitleBar(chat));
    }
    watch(chat);

    QByteArray layout = _layoutSetting.get();
    const bool fromBuildMode = layout.isEmpty();
    if (fromBuildMode) {
        layout = Setting::Handle<QByteArray>("editor/layout3", QByteArray()).get();   // Build mode's own, to 10-25
    }
    if (!layout.isEmpty()) {
        gl::ViewChangeGuard guard;   // docked panels coming back resize the GL view
        if (!window->restoreState(layout)) {
            qCWarning(interfaceapp) << "The window's saved layout could not be read; the panels start where they start";
        }
    }
    // the QML panels' docks, which the layout shows where they were if they were open
    DependencyManager::get<Panels>()->restore();
    raiseTabs(_frontTabsSetting.get());
    // a layout from before normal play and Build mode had one each can show My Files, left open in Build mode
    Setting::Handle<bool> hasModes { "window/layoutHasModes", false };
    if (!hasModes.get()) {
        if (auto myFiles = window->findChild<QDockWidget*>("EditorAssets")) {
            myFiles->hide();
        }
        hasModes.set(true);
    }
    // the bar's side is its own setting's, which Settings may have changed since the layout was saved
    SystemToolbar::instance()->restorePlace();
    qCDebug(interfaceapp) << "The window's layout:"
        << (layout.isEmpty() ? "the first one" : fromBuildMode ? "Build mode's" : "the saved one")
        << "; chat" << (chat->isHidden() ? "closed" : "open")
        << (chat->isFloating() ? "floating" : "docked");
    emit restored();
}

bool WindowLayout::restoreDock(QDockWidget* dock) {
    auto window = qApp->getWindow();
    if (!window || !dock) {
        return false;
    }
    gl::ViewChangeGuard guard;
    return window->restoreDockWidget(dock);
}

void WindowLayout::watch(QDockWidget* dock) {
    if (!dock || dock->property("windowLayoutWatched").toBool()) {
        return;
    }
    dock->setProperty("windowLayoutWatched", true);
    connect(dock, &QDockWidget::topLevelChanged, this, &WindowLayout::saveSoon);
    connect(dock, &QDockWidget::dockLocationChanged, this, &WindowLayout::saveSoon);
    connect(dock, &QDockWidget::visibilityChanged, this, &WindowLayout::saveSoon);
    dock->installEventFilter(this);   // its size (a separator's drag) and, floating, its place
}

bool WindowLayout::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
        saveSoon();
    }
    return QObject::eventFilter(watched, event);
}

void WindowLayout::saveSoon() {
    if (_restored) {
        _saveTimer.start();
    }
}

void WindowLayout::save() {
    auto window = qApp->getWindow();
    // quitting hides and destroys panels on its way out, which is no layout to come back to
    if (!_restored || !window || qApp->isAboutToQuit()) {
        return;
    }
    (_buildMode ? _buildLayoutSetting : _layoutSetting).set(window->saveState());
    (_buildMode ? _buildFrontTabsSetting : _frontTabsSetting).set(frontTabs());
}

QStringList WindowLayout::frontTabs() const {
    QStringList names;
    auto window = qApp->getWindow();
    if (!window) {
        return names;
    }
    for (QDockWidget* dock : window->findChildren<QDockWidget*>()) {
        // in a tab group and in sight: Qt keeps the others visible too, moved out of sight
        if (!dock->isHidden() && !dock->isFloating() && !window->tabifiedDockWidgets(dock).isEmpty() &&
            !dock->visibleRegion().isEmpty()) {
            names << dock->objectName();
        }
    }
    return names;
}

void WindowLayout::raiseTabs(const QStringList& names) {
    auto window = qApp->getWindow();
    if (!window) {
        return;
    }
    for (const QString& name : names) {
        if (auto dock = window->findChild<QDockWidget*>(name)) {
            if (!dock->isHidden()) {
                dock->raise();   // a tabbed dock's raise puts its tab in front
            }
        }
    }
}

void WindowLayout::putBack(const QByteArray& layout, const QStringList& tabs) {
    auto window = qApp->getWindow();
    if (!window || layout.isEmpty()) {
        return;
    }
    {
        gl::ViewChangeGuard guard;   // docked panels moving resize the GL view
        if (!window->restoreState(layout)) {
            qCWarning(interfaceapp) << "A saved layout could not be read; the panels stay where they are";
        }
        raiseTabs(tabs);
    }
    SystemToolbar::instance()->restorePlace();   // the bar's side is its own setting's
}

QVariantMap WindowLayout::named() {
    QVariantMap named = _namedSetting.get();
    const QVariantMap build = _buildNamedSetting.get();
    if (!build.isEmpty()) {
        for (auto i = build.begin(); i != build.end(); ++i) {
            QVariantMap entry = i.value().toMap();
            entry["build"] = true;
            named[named.contains(i.key()) ? i.key() + " (Build mode)" : i.key()] = entry;
        }
        _namedSetting.set(named);
        _buildNamedSetting.set(QVariantMap());
    }
    return named;
}

QStringList WindowLayout::savedNames() {
    return named().keys();
}

void WindowLayout::saveAs(const QString& name) {
    auto window = qApp->getWindow();
    if (!window || name.trimmed().isEmpty()) {
        return;
    }
    QVariantMap list = named();
    list[name.trimmed()] = QVariantMap {
        { "state", QString::fromLatin1(window->saveState().toBase64()) },
        { "front", frontTabs() },
        { "build", _buildMode },
    };
    _namedSetting.set(list);
}

void WindowLayout::apply(const QString& name) {
    const QVariantMap entry = named().value(name).toMap();
    if (entry.isEmpty()) {
        return;
    }
    putBack(QByteArray::fromBase64(entry.value("state").toString().toLatin1()), entry.value("front").toStringList());
    DependencyManager::get<Editor>()->layoutApplied(entry.value("build").toBool());
    saveSoon();
}

void WindowLayout::forget(const QString& name) {
    QVariantMap list = named();
    list.remove(name);
    _namedSetting.set(list);
}

void WindowLayout::resetToDefault() {
    {
        gl::ViewChangeGuard guard;
        DependencyManager::get<Panels>()->resetDocks();
        // chat's as on a first start, and Build mode's panels (My Files with them) docked where they start
        DependencyManager::get<Editor>()->resetLayout();
    }
    saveSoon();
}

bool WindowLayout::enterBuildMode() {
    auto window = qApp->getWindow();
    if (!window || _buildMode) {
        return false;
    }
    _saveTimer.stop();
    if (_restored) {
        _layoutSetting.set(window->saveState());
        _frontTabsSetting.set(frontTabs());
    }
    _buildMode = true;
    const QByteArray layout = _buildLayoutSetting.get();
    if (layout.isEmpty()) {
        return false;
    }
    putBack(layout, _buildFrontTabsSetting.get());
    return true;
}

void WindowLayout::leaveBuildMode() {
    auto window = qApp->getWindow();
    if (!window || !_buildMode) {
        return;
    }
    _saveTimer.stop();
    if (_restored) {
        _buildLayoutSetting.set(window->saveState());
        _buildFrontTabsSetting.set(frontTabs());
    }
    _buildMode = false;
    putBack(_layoutSetting.get(), _frontTabsSetting.get());
}
