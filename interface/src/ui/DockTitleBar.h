//
//  DockTitleBar.h
//  interface/src/ui
//
//  A dock panel's title bar (QDockWidget::setTitleBarWidget) for panels that float as well as dock, chat's first:
//  the panel's name, a pop-out button (docked) or a dock-back button (floating), and a close button, in Build mode's
//  panel colours, or the skin's ("panelTitle", SKINS.md) while one is on. Docked or floating it is the same bar, so
//  dragging it docks and undocks, and a double-click does too. Qt's own bar would do on Windows, but a floating panel
//  on macOS gets the system's small title bar instead, which a drag doesn't dock back from.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_DockTitleBar_h
#define hifi_DockTitleBar_h

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

class QDockWidget;
class QLabel;
class QToolButton;

class DockTitleBar : public QWidget {
    Q_OBJECT

public:
    explicit DockTitleBar(QDockWidget* dock);

    /// gives a dock panel this title bar, and puts its float and close buttons, a double-click on its title and the end
    /// of a drag that docks it under gl::ViewChangeGuard: a panel's own window coming or going beside the viewer's
    /// changes the GL view's surface on macOS (as chat's ChatDock and ui/QmlDock do in their own event handlers)
    static void attach(QDockWidget* dock);

    /// the buttons' actions (a double-click on the bar floats or docks too)
    void toggleFloating();
    void closePanel();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;   // the panel's place while it floats
    void changeEvent(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateLook();     // the skin's look or Build mode's
    void updateIcons();    // drawn in the title's colour: pop out while docked, dock back while floating

    QPointer<QDockWidget> _dock;
    QLabel* _title { nullptr };
    QToolButton* _float { nullptr };
    QToolButton* _close { nullptr };
    QRect _floatingGeometry;   // where it last floated, this run: popping out goes back there
};

#endif // hifi_DockTitleBar_h
