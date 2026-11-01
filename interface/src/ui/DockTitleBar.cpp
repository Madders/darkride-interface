//
//  DockTitleBar.cpp
//  interface/src/ui
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "DockTitleBar.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolButton>

#include <DependencyManager.h>
#include <gl/GLHelpers.h>

#include "PanelTheme.h"

namespace {
    const int ICON = 14;
    const int BUTTON = 20;

    enum class Glyph { PopOut, DockBack, Close };

    // drawn rather than shipped, so they take the title's colour, a skin's too
    QIcon glyphIcon(Glyph glyph, const QColor& color, qreal ratio) {
        QPixmap pixmap(QSize(ICON, ICON) * ratio);
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        switch (glyph) {
            case Glyph::PopOut: {
                // a window with an arrow leaving it by its top right corner
                QPainterPath frame;
                frame.moveTo(6.0, 3.5);
                frame.lineTo(3.5, 3.5);
                frame.quadTo(2.0, 3.5, 2.0, 5.0);
                frame.lineTo(2.0, 10.5);
                frame.quadTo(2.0, 12.0, 3.5, 12.0);
                frame.lineTo(9.0, 12.0);
                frame.quadTo(10.5, 12.0, 10.5, 10.5);
                frame.lineTo(10.5, 8.0);
                painter.drawPath(frame);
                painter.drawLine(QPointF(6.5, 7.5), QPointF(12.0, 2.0));
                painter.drawLine(QPointF(12.0, 2.0), QPointF(8.5, 2.0));
                painter.drawLine(QPointF(12.0, 2.0), QPointF(12.0, 5.5));
                break;
            }
            case Glyph::DockBack: {
                // a window with its right side filled: where the panel docks
                const QRectF frame(1.5, 2.5, 11.0, 9.0);
                painter.drawRoundedRect(frame, 1.5, 1.5);
                painter.drawLine(QPointF(8.5, 2.5), QPointF(8.5, 11.5));
                QColor fill = color;
                fill.setAlphaF(fill.alphaF() * 0.45);
                painter.setPen(Qt::NoPen);
                painter.setBrush(fill);
                painter.drawRect(QRectF(8.5, 3.0, 3.5, 8.0));
                break;
            }
            case Glyph::Close:
                painter.drawLine(QPointF(3.5, 3.5), QPointF(10.5, 10.5));
                painter.drawLine(QPointF(10.5, 3.5), QPointF(3.5, 10.5));
                break;
        }
        return QIcon(pixmap);
    }

    // Build mode's panel title (styles/editor.qss: QDockWidget#EditorInspector::title), its colours the skin's where
    // the skin gives them (the same roles as Editor::applyStyle's)
    QString ownSheet(const QVariantMap& skin) {
        const QVariantMap colors = skin.value("colors").toMap();
        auto role = [&colors](const QString& key, const QString& otherwise) {
            const QColor color(colors.value(key).toString());
            return color.isValid() ? color.name(QColor::HexArgb) : otherwise;
        };
        const QString text = role("text", "#d2d2d2");
        // a skin's own frame for panel titles (SKINS.md "panelTitle") is the application sheet's, and draws instead
        const bool framed = skin.value("frames").toMap().contains("panelTitle");
        const QString titleText = colors.contains("titleText") ? role("titleText", text) : text;
        QString sheet = "QWidget#DockTitleBar { ";
        if (!framed) {
            sheet += "background: " + role("card", "#3c3c3c") + "; border-bottom: 1px solid " +
                     role("separator", "#232323") + "; ";
        }
        sheet += "}\n";
        sheet += "QWidget#DockTitleBar QLabel { color: " + titleText +
                 "; font-size: 12px; background: transparent; }\n";
        // padding and margin too: a panel's own buttons (Build mode's Inspector and My Files) have theirs padded
        sheet += "QWidget#DockTitleBar QToolButton { border: none; border-radius: 3px; background: transparent;"
                 " padding: 0; margin: 0; }\n";
        sheet += "QWidget#DockTitleBar QToolButton:hover { background: rgba(255, 255, 255, 0.10); }\n";
        sheet += "QWidget#DockTitleBar QToolButton:pressed { background: rgba(255, 255, 255, 0.18); }\n";
        return sheet;
    }

    QToolButton* titleButton(QWidget* parent, const QString& tip) {
        auto button = new QToolButton(parent);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedSize(BUTTON, BUTTON);
        button->setIconSize(QSize(ICON, ICON));
        button->setToolTip(tip);
        return button;
    }
}

namespace {
    // the events that end in a panel's window coming or going, handled under the guard: the event is sent again from
    // inside it, and that second pass goes through
    class ViewChangeFilter : public QObject {
    public:
        using QObject::QObject;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override {
            switch (event->type()) {
                case QEvent::MouseButtonDblClick:
                case QEvent::MouseButtonRelease:
                case QEvent::NonClientAreaMouseButtonDblClick:
                case QEvent::NonClientAreaMouseButtonRelease:
                case QEvent::Close:
                    if (!_inside) {
                        gl::ViewChangeGuard guard(event->type());
                        _inside = true;
                        QCoreApplication::sendEvent(watched, event);
                        _inside = false;
                        return true;
                    }
                    break;
                default:
                    break;
            }
            return false;
        }

    private:
        bool _inside { false };
    };
}

void DockTitleBar::attach(QDockWidget* dock) {
    if (!dock || qobject_cast<DockTitleBar*>(dock->titleBarWidget())) {
        return;
    }
    dock->setTitleBarWidget(new DockTitleBar(dock));
    auto filter = new ViewChangeFilter(dock);
    dock->installEventFilter(filter);
    for (auto button : dock->titleBarWidget()->findChildren<QAbstractButton*>()) {
        button->installEventFilter(filter);
    }
}

DockTitleBar::DockTitleBar(QDockWidget* dock) : QWidget(dock), _dock(dock) {
    setObjectName("DockTitleBar");
    setAttribute(Qt::WA_StyledBackground);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 3, 4, 3);   // Build mode's titles: 5px 8px around 12 px text
    layout->setSpacing(2);
    _title = new QLabel(dock->windowTitle(), this);
    _title->setTextFormat(Qt::PlainText);
    _float = titleButton(this, QString());
    _close = titleButton(this, tr("Close"));
#ifdef Q_OS_MAC
    // the close button first, where macOS has it and Qt's own panel titles (Build mode's) put it
    layout->setContentsMargins(4, 3, 4, 3);
    layout->addWidget(_close);
    layout->addWidget(_title, 1);
    layout->addWidget(_float);
#else
    layout->addWidget(_title, 1);
    layout->addWidget(_float);
    layout->addWidget(_close);
#endif

    connect(dock, &QDockWidget::windowTitleChanged, _title, &QLabel::setText);
    connect(dock, &QDockWidget::topLevelChanged, this, &DockTitleBar::updateIcons);
    connect(_float, &QToolButton::clicked, this, &DockTitleBar::toggleFloating);
    connect(_close, &QToolButton::clicked, this, &DockTitleBar::closePanel);
    connect(DependencyManager::get<PanelTheme>().data(), &PanelTheme::changed, this, &DockTitleBar::updateLook);
    dock->installEventFilter(this);
    updateLook();
}

void DockTitleBar::updateLook() {
    setStyleSheet(ownSheet(DependencyManager::get<PanelTheme>()->getSkin()));
    updateIcons();
}

void DockTitleBar::updateIcons() {
    if (!_dock) {
        return;
    }
    _title->ensurePolished();   // the sheet's colour, set just now
    const QColor color = _title->palette().color(QPalette::WindowText);
    const qreal ratio = devicePixelRatioF();
    const bool floating = _dock->isFloating();
    _float->setIcon(glyphIcon(floating ? Glyph::DockBack : Glyph::PopOut, color, ratio));
    _float->setToolTip(floating ? tr("Dock in the main window") : tr("Open in its own window"));
    _float->setVisible(_dock->features().testFlag(QDockWidget::DockWidgetFloatable));
    _close->setIcon(glyphIcon(Glyph::Close, color, ratio));
    _close->setVisible(_dock->features().testFlag(QDockWidget::DockWidgetClosable));
}

void DockTitleBar::toggleFloating() {
    if (!_dock || !_dock->features().testFlag(QDockWidget::DockWidgetFloatable)) {
        return;
    }
    gl::ViewChangeGuard guard;   // docking or undocking resizes the GL view
    const bool floating = !_dock->isFloating();
    const QRect lastFloating = _floatingGeometry;   // floating it moves it first, to where it was docked
    _dock->setFloating(floating);
    // popping out: where it floated last, if that's still on a screen; else where it was docked, as Qt leaves it
    if (floating && lastFloating.isValid()) {
        for (QScreen* screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().intersects(lastFloating)) {
                _dock->setGeometry(lastFloating);
                break;
            }
        }
    }
    if (floating) {
        _dock->raise();
        _dock->activateWindow();
    }
}

void DockTitleBar::closePanel() {
    if (_dock) {
        gl::ViewChangeGuard guard;   // a docked panel closing gives its room back to the GL view
        _dock->close();
    }
}

bool DockTitleBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _dock && _dock->isFloating() && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        _floatingGeometry = _dock->geometry();
    } else if (watched == _dock && event->type() == QEvent::ScreenChangeInternal) {
        updateIcons();   // drawn for the new screen's pixel ratio
    }
    return QWidget::eventFilter(watched, event);
}

void DockTitleBar::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    // a skin's sheet reaching the title (re-polished) can change its colour, which the icons are drawn in
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange) {
        updateIcons();
    }
}

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    // as Qt's own title bar does, but through toggleFloating, under the guard and back to the last floating place
    if (event->button() == Qt::LeftButton) {
        toggleFloating();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void DockTitleBar::paintEvent(QPaintEvent*) {
    // the style sheet's background and border for a plain QWidget
    QStyleOption option;
    option.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
}
