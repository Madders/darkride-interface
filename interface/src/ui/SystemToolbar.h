//
//  SystemToolbar.h
//  interface/src/ui
//
//  The bar of app buttons (the system tablet's: Audio, Settings, Snap, Avatar, People, Explore, Create...) as a
//  native toolbar (QToolBar) on a side of the main window, so the 3D view sits beside it instead of under it (a
//  player's ask, through Caity, 10-24). "left" (the default: Caity, 10-24, never the floating bar by default, and at
//  the bottom it crowds Build mode's Assets panel), "top", "right" or "bottom" docks this one there, and dragging it by its handle to another side moves it; place
//  "floating" keeps the QML bar over the bottom of the view instead (hifi/Desktop.qml's sysToolbar, as before). The sign-in screen and VR hide whichever bar is in use: SignInGate says when a bar may show
//  (setAllowed). Its look is the Qt style sheet's, so a skin reaches it. QML panels set the place (Settings).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_SystemToolbar_h
#define hifi_SystemToolbar_h

#include <QtCore/QPointer>
#include <QtWidgets/QToolBar>

#include <SettingHandle.h>

class QMainWindow;
class QToolButton;
class TabletButtonProxy;
class TabletProxy;

class SystemToolbar : public QToolBar {
    Q_OBJECT
    Q_PROPERTY(QString place READ getPlace WRITE setPlace NOTIFY placeChanged)
    Q_PROPERTY(bool docked READ isDocked NOTIFY placeChanged)

public:
    static SystemToolbar* instance();

    /// on the main window, once it and the system tablet exist
    void attach(QMainWindow* window, TabletProxy* tablet);

    QString getPlace() const;
    void setPlace(const QString& place);
    bool isDocked() const { return getPlace() != "floating"; }

    /// the sign-in gate's say: a bar may show (false on the sign-in screen)
    void setAllowed(bool allowed);

    /// back on the side its setting names, after the window's saved layout (ui/WindowLayout.h) put it where it was
    /// when the layout was saved
    void restorePlace();

    /// a skin is on: the bar drops its own look for the application's style sheet (Skins)
    void setSkinned(bool skinned);

    /// the bar's own line icon for a button (look A), by its label and, for Audio, whether it's muted
    /// ("icons/bar/audio-muted.svg"), or "" when the bar has none for it; the floating bar asks it too (BarButton.qml)
    Q_INVOKABLE QString iconFor(const QString& text, bool muted) const;

signals:
    void placeChanged();

protected:
    bool event(QEvent* event) override;   // a drag by the handle let go: remember the side it went to

public:
    /// shown or hidden only as updateVisibility wants: the window's saved layout (QMainWindow::restoreState)
    /// holds this toolbar too, and restoring it showed this bar beside the floating one, or hid the docked one (10-24)
    void setVisible(bool visible) override;

private:
    SystemToolbar();

    void rebuild();
    void refresh(QToolButton* button, TabletButtonProxy* proxy);
    void fitCells();   // every button the size of the largest
    void updateVisibility();
    void rememberArea();
    QPixmap buttonPicture(const QVariantMap& properties, bool active) const;

    QPointer<QMainWindow> _window;
    QPointer<TabletProxy> _tablet;
    Setting::Handle<QString> _placeSetting { "toolbar/systemPlace", "left" };
    // no bar until the sign-in gate says one may show (SignInGate::resumeAfterLoginDialogActionTaken), as the QML bar
    // always waited: not on the sign-in screen
    bool _allowed { false };
    bool _wanted { false };   // updateVisibility's word, which setVisible keeps to
    bool _skinned { false };  // a skin's style sheet draws the bar: the icons take its text colour
};

#endif // hifi_SystemToolbar_h
