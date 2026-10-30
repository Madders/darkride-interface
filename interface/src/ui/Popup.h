//
//  Popup.h
//  interface/src/ui
//
//  Darkride's native popups, in the design language Caity chose for all of them (10-18): a narrow centred alert in the
//  manner of macOS, with a picture on top, a bold title, at most one short line, and full-width buttons, side by side
//  when there are two (Cancel on the left, the default on the right) and stacked when there are more (the default on
//  top), the default in blue; a button that deletes something for good is red and never the default (Cancel is);
//  further detail sits behind a small Details fold-out, in a scrolling field; a question that needs a word or a link
//  has one field under the line. A success gets no alert but a banner in the window's corner that fades by itself.
//  A wait that takes no answer (Darkride closing) gets a notice: the alert without buttons, taken down by the app.
//  The viewer's other popups share this design in QML (interface/resources/qml/dialogs/alert/AlertPanel.qml).
//  While a skin is on, a popup takes the skin's colours, font and "popup" frame (SKINS.md, stage 2).
//  Moved out of the avatar importer (10-25) so build mode's questions use it too.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_Popup_h
#define hifi_Popup_h

#include <memory>
#include <vector>

#include <QColor>
#include <QDialog>
#include <QMargins>
#include <QPixmap>
#include <QString>
#include <QWidget>

class QLineEdit;
class QPushButton;

namespace popup {

enum class Role { Default, Cancel, Plain, Destructive };

struct Button {
    QString label;  // "&&" for an ampersand: a button reads "&" as a keyboard shortcut mark
    Role role;
};

// What a popup is drawn in: Darkride's own colours, or a skin's (read when the popup opens)
struct Look {
    QColor panel { 43, 43, 46 };
    QColor border { 255, 255, 255, 26 };
    QColor title { 255, 255, 255 };
    QColor text { 255, 255, 255, 189 };
    QColor well { 0, 0, 0, 71 };            // under the field and the details
    QColor button { 255, 255, 255, 36 };
    QColor buttonPressed { 255, 255, 255, 56 };
    QColor buttonText { 255, 255, 255 };
    QColor tint { 0x0b, 0x6b, 0xd8 };       // the default button
    QColor destructive { 0xff, 0x45, 0x3a };
    QString font;                           // the skin's font family; "" for the system's
    QPixmap frame;                          // the skin's "popup" frame, drawn 9-slice, in place of the rounded panel
    QMargins slice;
    QMargins padding;
};

// The one field of a question that needs a word or a link; the default button waits until it holds something.
struct Field {
    QString placeholder;
    QString text;
};

class Alert : public QDialog {
public:
    Alert(const QPixmap& picture, const QString& title, const QString& text, const QString& details,
          const std::vector<Button>& buttons, QWidget* parent, const Field* field = nullptr);

    int answer() const { return _answer; }
    QString fieldText() const;  // trimmed; empty without a field
    // the rights line (RightsLine.h) in small grey text under the buttons, for an alert whose default button puts
    // something where others see it
    void addRightsLine();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    int _answer { -1 };
    int _default { -1 };
    int _cancel { -1 };
    QLineEdit* _field { nullptr };
    QPushButton* _defaultButton { nullptr };
    std::shared_ptr<const Look> _look;
};

// A note that comes and goes in the top right corner of the window: clicked, it goes at once.
class Banner : public QWidget {
public:
    Banner(const QPixmap& picture, const QString& title, const QString& text, QWidget* anchor, int milliseconds = 4500);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    void fade(qreal to, int milliseconds, bool thenClose = false);

    std::shared_ptr<const Look> _look;
};

// An alert with no buttons, over the window a third of the way down, that never takes the focus or raises the app:
// for a wait that takes no answer, such as Darkride closing. Whoever shows it takes it down (or the app's end does).
// On macOS it shows only while Darkride is the active app (a splash screen is a tool panel there, and those hide in
// the background), so a quit chosen from the Dock while another app is in front passes without it.
class Notice : public QWidget {
public:
    Notice(const QPixmap& picture, const QString& title, const QString& text, QWidget* anchor);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::shared_ptr<const Look> _look;
};

// Darkride's logo, the picture of a popup with no subject of its own.
QPixmap appIcon();

}

#endif  // hifi_Popup_h
