//
//  WindowLayout.h
//  interface/src/ui
//
//  The main window's saved layouts (QMainWindow::saveState): where the dock panels are, docked or floating, their
//  sizes, and whether each is open. Normal play and Build mode each have their own (Caity's "go step 2", 2026-10-26),
//  so each opens the way it was left: entering Build mode saves normal play's and puts Build mode's back, leaving does
//  the reverse. Normal play's is restored once, right after sign-in, so nothing moves under the sign-in screen; the
//  current one is saved a moment after any watched panel moves, resizes, floats, docks, opens or closes, and never
//  while quitting. A panel made later takes its saved place then (restoreDock). It took over from Build mode's own
//  layout ("editor/layout3"), which the first restore still reads. Chat docking, step 1 of
//  team/skunkworks/briefs/dockable-panels.md (Caity's "GO CHAT DOCKING", 2026-10-25); dockable panels, step 2.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_WindowLayout_h
#define hifi_WindowLayout_h

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <SettingHandle.h>

class QDockWidget;

class WindowLayout : public QObject {
    Q_OBJECT

public:
    static WindowLayout* instance();

    /// after sign-in, once: chat's panel first (so the saved layout finds it), then the saved layout
    void restore();

    /// a dock panel made after the restore takes its saved place, shown or hidden as it was; false if the layout
    /// has none for it (the caller adds it where it starts)
    bool restoreDock(QDockWidget* dock);

    /// the panel's moves, sizes, floating and showing are saved
    void watch(QDockWidget* dock);

    /// a save a moment from now: many changes in a row (a separator's drag) make one save
    void saveSoon();

    /// Build mode's own arrangement: normal play's saved, Build mode's put back as it was left; false when Build mode
    /// has none yet (the caller places its panels where they start, the others staying as they are)
    bool enterBuildMode();
    /// Build mode's arrangement saved, normal play's put back
    void leaveBuildMode();
    bool isBuildMode() const { return _buildMode; }

    /// View > Layouts: the named arrangements, by name, one list for both modes (Caity, 10-27: one saved in Build
    /// mode seemed lost in normal play's list)
    QStringList savedNames();
    /// the arrangement as it is now, under a name (replacing one of that name)
    void saveAs(const QString& name);
    /// a named arrangement put back; in either mode, Build mode's own panels and rows following the mode
    void apply(const QString& name);
    void forget(const QString& name);
    /// every panel back where it starts: the bar's panels closed, floating; the console closed; chat as on a first
    /// start; Build mode's docked where they start (My Files closed in normal play)
    void resetToDefault();

signals:
    void restored();   // chat's panel exists from here on

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    WindowLayout();
    void save();
    void putBack(const QByteArray& layout, const QStringList& frontTabs);   // under the guard, the bar's side after
    QVariantMap named();                        // the named arrangements, Build mode's old list joined in
    QStringList frontTabs() const;              // the docked panels in front of a tab group, by object name
    void raiseTabs(const QStringList& names);   // Qt keeps a tab group's order, not which tab was in front

    QTimer _saveTimer;
    bool _restored { false };   // no save before the restore: it would write the start's layout over the saved one
    bool _buildMode { false };
    Setting::Handle<QByteArray> _layoutSetting { "window/layout", QByteArray() };             // normal play's
    Setting::Handle<QByteArray> _buildLayoutSetting { "window/buildLayout", QByteArray() };   // Build mode's
    Setting::Handle<QStringList> _frontTabsSetting { "window/frontTabs", QStringList() };
    Setting::Handle<QStringList> _buildFrontTabsSetting { "window/buildFrontTabs", QStringList() };
    // View > Layouts' named arrangements: name -> { "state": saveState in base64, "front": front tabs, "build": saved
    // in Build mode }; Build mode kept its own list until 10-27, which the first look joins to this one
    Setting::Handle<QVariantMap> _namedSetting { "window/namedLayouts", QVariantMap() };
    Setting::Handle<QVariantMap> _buildNamedSetting { "window/namedBuildLayouts", QVariantMap() };
};

#endif // hifi_WindowLayout_h
