//
//  QmlDock.h
//  interface/src/ui
//
//  A dock panel that shows one of Darkride's QML panels (qml/darkride/*Panel.qml) natively, so it can sit beside the
//  3D view, be tabbed with other panels or float in its own window, as chat and Build mode's panels do; ui/Panels.h
//  makes them and opens them by name.
//  Dockable panels, step 2 (Caity's "go step 2", 2026-10-26; team/editor-ui/DOCKING-STEP2-PLAN.md). The panel is the
//  same QML its overlay window showed, made on the app's one QML engine in a context under the desktop's (so it finds
//  MetaverseRequests, the account, MyAvatar and the rest by the same names) and drawn by a QQuickWidget. Its QML is
//  made the first time the panel shows, and a hidden panel draws nothing: its page is hidden with it (a panel tabbed
//  behind another too), so the page's own onVisibleChanged reads its values again on each showing and its timers stop,
//  as in its overlay window. Every float, dock and close happens under gl::ViewChangeGuard, as chat's
//  (chat/ChatWindow.cpp's ChatDock).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_QmlDock_h
#define hifi_QmlDock_h

#include <QtCore/QUrl>
#include <QtWidgets/QDockWidget>

class QQuickItem;
class QQuickWidget;

class QmlDock : public QDockWidget {
    Q_OBJECT
public:
    QmlDock(const QString& name, const QUrl& source, QWidget* parent);

    // the panel's QML, once made (for tests)
    QQuickItem* panel() const;
    // the page and its view gone, at quit before the QML engine (Panels::releaseQml)
    void releaseContent();
    // the page made before the panel first shows, whichever way it shows (Panels, the saved layout, a drag)
    void setVisible(bool visible) override;

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void makeContent();

    QUrl _source;
    QQuickWidget* _view { nullptr };
    bool _releasing { false };
};

#endif // hifi_QmlDock_h
