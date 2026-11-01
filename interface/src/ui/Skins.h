//
//  Skins.h
//  interface/src/ui
//
//  Darkride's skins (SKINS.md in the workspace; players' ask, 10-24): a skin is a folder or a .zip of one, holding
//  skin.json, pictures and fonts, data only, so a skin from anyone is safe to load. Installed skins live in the app
//  data's skins/ folder; the choice is saved. Applying one: a style sheet made from its frames is set on the
//  application (every Qt widget: menus, toolbars, docks, buttons, fields, tabs, scroll bars, the status bar), the 3D
//  view gets its frame (ViewportContainer's inset), PanelTheme carries its colours and frames to the QML panels, its
//  fonts load, and a click-through layer over the main window draws the window's frame and the decorations (the 3D
//  view is a native surface above that layer, so nothing covers the world). QML panels only, as MetaverseRequests.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_Skins_h
#define hifi_Skins_h

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtGui/QFont>

#include <DependencyManager.h>
#include <SettingHandle.h>

class QMainWindow;
class SkinLayer;

class Skins : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

    /// the skins installed: [{ id, name, author }], by name
    Q_PROPERTY(QVariantList installed READ getInstalled NOTIFY changed)
    /// the skin in use, by id; "" is Darkride's own look
    Q_PROPERTY(QString current READ getCurrent NOTIFY changed)

public:
    static const int FORMAT { 1 };

    /// the main window, once it exists; the saved choice goes on when the sign-in gate lets it (setHeld)
    void start(QMainWindow* window);
    /// the sign-in gate's say (Caity, 10-24: no skin until you're signed in, it's a setting): held on the sign-in
    /// screen, the skin is off and the choice stays saved; let go, the saved choice goes on
    void setHeld(bool held);

    QVariantList getInstalled() const;
    QString getCurrent() const { return _current; }

    /// puts a skin on ("" for Darkride's own look): "" when it is on, else what is wrong with it
    Q_INVOKABLE QString setCurrent(const QString& id);
    /// installs a skin from a .zip, a skin.json or a folder (a path or a file: URL) and puts it on: "" or the problem
    Q_INVOKABLE QString install(const QString& pathOrUrl);
    /// removes an installed skin (Darkride's own look if it was on): "" or the problem
    Q_INVOKABLE QString remove(const QString& id);
    /// where the installed skins are, for a panel's "show the folder"
    Q_INVOKABLE QString folder() const;

signals:
    void changed();

private:
    Skins() = default;

    QString apply(const QString& id);
    void takeOff();

    QPointer<QMainWindow> _window;
    QPointer<SkinLayer> _layer;
    Setting::Handle<QString> _currentSetting { "skin/current", QString() };
    QString _current;
    bool _held { true };
    QFont _ownFont;
    QList<int> _fontIds;
};

#endif // hifi_Skins_h
