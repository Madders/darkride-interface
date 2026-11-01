//
//  ViewportContainer.h
//  interface/src/editor/ui
//
//  Holds the GL view as the main window's central widget. While build mode's panels are being
//  resized it keeps the GL view's size fixed and applies the new size once the dragging stops:
//  resizing the native GL view on every mouse move races the presenting thread (a crash inside
//  Apple's GL driver was caught doing exactly that). After a change of the display's scale or of screen it puts the
//  view's size again, at the new scale.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ViewportContainer_h
#define hifi_ViewportContainer_h

#include <QtCore/QTimer>
#include <QtWidgets/QWidget>

class ViewportContainer : public QWidget {
    Q_OBJECT
public:
    ViewportContainer(QWidget* viewport, QWidget* parent = nullptr);

    // true while build mode is on: resizes of the container reach the viewport only after a pause
    void setDeferResizes(bool defer);
    // room around the GL view for a frame drawn by the container's style sheet (a skin's viewport frame, ui/Skins.h)
    void setInset(const QMargins& inset);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applySize();
    void watchScreen();     // the window's screen, for a change of its scale
    void reapplySize();     // the view's size put again, once the new scale has settled

    QWidget* _viewport { nullptr };
    bool _defer { false };
    QTimer _settle;
    QTimer _rescale;
    QMetaObject::Connection _screenChanged;
    QMetaObject::Connection _screenGeometry;
    QMetaObject::Connection _screenDpi;
};

#endif // hifi_ViewportContainer_h
