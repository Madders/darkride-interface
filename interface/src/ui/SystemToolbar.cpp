//
//  SystemToolbar.cpp
//  interface/src/ui
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "SystemToolbar.h"

#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QAction>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <DependencyManager.h>
#include <PathUtils.h>
#include <ui/TabletScriptingInterface.h>
#include <ui/ToolbarScriptingInterface.h>

#include "InterfaceLogging.h"

namespace {
    const QString QML_TOOLBAR = "com.highfidelity.interface.toolbar.system";   // the floating bar (hifi/Desktop.qml)
    const int BUTTON = 48;              // as the QML bar draws them (hifi/toolbars/ToolbarButton.qml)
    // in a column, on the left or the right: a skin's padding made a button 94 px tall, and ten no longer fitted a
    // laptop's window (10-24)
    const int SIDE_BUTTON = 36;
    const qreal RADIUS = 4.0;
    const qreal ACTIVE_PADDING = 8.0;
    const QStringList PLACES { "floating", "top", "left", "right", "bottom" };

    Qt::ToolBarArea areaOf(const QString& place) {
        if (place == "top") return Qt::TopToolBarArea;
        if (place == "right") return Qt::RightToolBarArea;
        if (place == "bottom") return Qt::BottomToolBarArea;
        return Qt::LeftToolBarArea;
    }
    QString placeOf(Qt::ToolBarArea area) {
        switch (area) {
            case Qt::TopToolBarArea: return "top";
            case Qt::RightToolBarArea: return "right";
            case Qt::BottomToolBarArea: return "bottom";
            default: return "left";
        }
    }

    // an icon's picture, fitted into rect: an SVG or an image in a data: address (the bar's own buttons draw theirs so,
    // as "data:image/svg;xml,<svg ...>"), a file, or a path in the app's resources (as the QML bar resolves it)
    void drawIcon(QPainter& painter, const QString& source, const QRectF& rect) {
        auto fit = [&rect](QSizeF size) {
            if (size.isEmpty()) {
                return rect;
            }
            size.scale(rect.size(), Qt::KeepAspectRatio);
            return QRectF(rect.center() - QPointF(size.width() / 2, size.height() / 2), size);
        };
        auto drawSvg = [&painter, &fit](QSvgRenderer& svg) {
            if (svg.isValid()) {
                svg.render(&painter, fit(svg.defaultSize()));
            }
        };
        auto drawImage = [&painter, &fit](const QImage& image) {
            if (!image.isNull()) {
                painter.drawImage(fit(image.size()), image);
            }
        };
        if (source.startsWith("data:")) {
            const int comma = source.indexOf(',');
            const QString header = source.left(comma);
            const QByteArray payload = source.mid(comma + 1).toUtf8();
            const QByteArray bytes = header.contains(";base64") ? QByteArray::fromBase64(payload)
                                                                 : QUrl::fromPercentEncoding(payload).toUtf8();
            if (header.contains("svg")) {
                QSvgRenderer svg(bytes);
                drawSvg(svg);
            } else {
                drawImage(QImage::fromData(bytes));
            }
            return;
        }
        QString path = source;
        const QUrl url(source);
        if (url.isLocalFile()) {
            path = url.toLocalFile();
        } else if (url.scheme() == "qrc") {
            path = ":" + url.path();
        } else if (!url.scheme().isEmpty() && url.scheme().length() > 1) {
            return;   // a web address: not fetched for a button
        } else if (QFileInfo(path).isRelative()) {
            path = PathUtils::resourcesPath() + source;
        }
        if (path.endsWith(".svg", Qt::CaseInsensitive)) {
            QSvgRenderer svg(path);
            drawSvg(svg);
        } else {
            drawImage(QImage(path));
        }
    }
}

SystemToolbar* SystemToolbar::instance() {
    static QPointer<SystemToolbar> theInstance;   // the main window owns it once attached
    if (!theInstance) {
        theInstance = new SystemToolbar();
    }
    return theInstance;
}

SystemToolbar::SystemToolbar() : QToolBar("Buttons") {
    setObjectName("systemToolbar");
    setMovable(true);
    setFloatable(false);
    setAllowedAreas(Qt::AllToolBarAreas);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setIconSize(QSize(BUTTON, BUTTON));
    toggleViewAction()->setEnabled(false);   // the main window's toolbar menu doesn't hide it: Settings places it
    setSkinned(false);
    connect(this, &QToolBar::orientationChanged, this, [this]() {
        const QSize size(buttonSize(), buttonSize());
        setIconSize(size);
        for (QToolButton* button : findChildren<QToolButton*>(QString(), Qt::FindDirectChildrenOnly)) {
            button->setIconSize(size);
        }
    });
    hide();
}

int SystemToolbar::buttonSize() const {
    return orientation() == Qt::Vertical ? SIDE_BUTTON : BUTTON;
}

void SystemToolbar::setSkinned(bool skinned) {
    // its own look, until a skin gives one (the QML bar's: white captions on a dark strip)
    setStyleSheet(skinned ? QString() :
        "QToolBar#systemToolbar { background: #1d1f21; border: none; spacing: 4px; padding: 6px; }"
        "QToolBar#systemToolbar QToolButton { color: #ffffff; font-size: 12px; border: none; border-radius: 6px;"
        " padding: 2px 4px; }"
        "QToolBar#systemToolbar QToolButton:hover { background: rgba(255, 255, 255, 0.10); }"
        "QToolBar#systemToolbar QToolButton:pressed { background: rgba(255, 255, 255, 0.18); }");
}

void SystemToolbar::attach(QMainWindow* window, TabletProxy* tablet) {
    if (_window || !window || !tablet) {
        return;
    }
    _window = window;
    _tablet = tablet;
    window->addToolBar(areaOf(getPlace()), this);

    auto model = tablet->getButtons();
    connect(model, &QAbstractItemModel::rowsInserted, this, &SystemToolbar::rebuild);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &SystemToolbar::rebuild);
    connect(model, &QAbstractItemModel::modelReset, this, &SystemToolbar::rebuild);
    connect(model, &QAbstractItemModel::layoutChanged, this, &SystemToolbar::rebuild);
    // after the QML bar has followed the change itself (its `shown`), so this has the last word
    connect(tablet, &TabletProxy::toolbarModeChanged, this, [this]() {
        QTimer::singleShot(0, this, &SystemToolbar::updateVisibility);
    });
    rebuild();
    updateVisibility();
}

QString SystemToolbar::getPlace() const {
    const QString place = _placeSetting.get();
    return PLACES.contains(place) ? place : "left";
}

void SystemToolbar::setPlace(const QString& place) {
    if (!PLACES.contains(place) || place == getPlace()) {
        return;
    }
    _placeSetting.set(place);
    if (_window && place != "floating") {
        _window->addToolBar(areaOf(place), this);   // a toolbar the window has already: moved to that side
    }
    emit placeChanged();
    updateVisibility();
    qCDebug(interfaceapp) << "The bar's place:" << place << (_window ? placeOf(_window->toolBarArea(this)) : QString())
                          << (isVisible() ? "shown" : "hidden");
}

void SystemToolbar::setAllowed(bool allowed) {
    _allowed = allowed;
    updateVisibility();
}

void SystemToolbar::updateVisibility() {
    const bool desktopBar = _tablet && _tablet->getToolbarMode();   // not in VR
    const bool docked = isDocked();
    if (auto qmlToolbar = DependencyManager::get<ToolbarScriptingInterface>()->getToolbar(QML_TOOLBAR)) {
        // the sign-in gate's word, as SignInGate wrote it before; whether the QML bar is the one in use is its own
        // `shown` binding's (hifi/Desktop.qml: not while docked), which a C++ write would not remove, so both bars
        // showed when this wrote `shown` (10-24)
        qmlToolbar->writeProperty("visible", _allowed && desktopBar && !docked);
    }
    const bool show = _window && _allowed && desktopBar && docked;
    _wanted = show;
    if (show != isVisible()) {
        qCDebug(interfaceapp) << "The bar on a side:" << (show ? "shown" : "hidden") << getPlace()
                              << "(allowed" << _allowed << ", desktop" << desktopBar << ")";
    }
    setVisible(show);
}

void SystemToolbar::setVisible(bool) {
    QToolBar::setVisible(_wanted);   // whoever asks: Build mode's saved layout both showed and hid it (10-24)
}

bool SystemToolbar::event(QEvent* event) {
    const bool handled = QToolBar::event(event);
    // only the user's drag (the buttons take their own clicks): the window's own moves of the bar are setPlace's
    if (event->type() == QEvent::MouseButtonRelease) {
        QTimer::singleShot(0, this, &SystemToolbar::rememberArea);
    }
    return handled;
}

void SystemToolbar::rememberArea() {
    if (!_window || !isVisible() || !isDocked()) {
        return;
    }
    const QString place = placeOf(_window->toolBarArea(this));
    if (place != getPlace()) {
        _placeSetting.set(place);
        emit placeChanged();
    }
}

void SystemToolbar::rebuild() {
    // each button's action owns its button (QToolBar::addWidget), so deleting the action deletes the button
    for (QAction* action : actions()) {
        removeAction(action);
        delete action;
    }
    if (!_tablet) {
        return;
    }
    auto model = _tablet->getButtons();
    for (int row = 0; row < model->rowCount(); row++) {
        auto proxy = model->data(model->index(row), Qt::UserRole).value<TabletButtonProxy*>();
        if (!proxy) {
            continue;
        }
        auto button = new QToolButton(this);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(buttonSize(), buttonSize()));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        QPointer<TabletButtonProxy> weak(proxy);
        connect(button, &QToolButton::clicked, this, [weak]() {
            // a moment after the click, as the QML bar does: the button may open a panel that takes the mouse
            QTimer::singleShot(10, [weak]() {
                if (weak) {
                    emit weak->clicked();
                }
            });
        });
        connect(proxy, &TabletButtonProxy::propertiesChanged, button, [this, button, weak]() {
            if (weak) {
                refresh(button, weak);
            }
        });
        refresh(button, proxy);
        addWidget(button);
    }
}

void SystemToolbar::refresh(QToolButton* button, TabletButtonProxy* proxy) {
    const QVariantMap properties = proxy->getProperties();
    const bool active = properties.value("isActive").toBool();
    button->setIcon(QIcon(buttonPicture(properties, active)));
    button->setText(properties.value(active ? "activeText" : "text", properties.value("text")).toString());
    // a skin's style sheet can tell an open app's button: QToolButton[active="true"]
    button->setProperty("active", active);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

// the QML bar's button: a rounded square in the button's colour with its icon inside, shrunk into a grey square when
// its app is open
QPixmap SystemToolbar::buttonPicture(const QVariantMap& properties, bool active) const {
    const qreal ratio = devicePixelRatioF();
    QPixmap pixmap(QSize(BUTTON, BUTTON) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setPen(Qt::NoPen);

    QRectF square(0, 0, BUTTON, BUTTON);
    if (active) {
        painter.setBrush(QColor(153, 153, 153, 153));
        painter.drawRoundedRect(square, RADIUS, RADIUS);
        square.adjust(ACTIVE_PADDING / 2, ACTIVE_PADDING / 2, -ACTIVE_PADDING / 2, -ACTIVE_PADDING / 2);
    }
    QPainterPath shape;
    shape.addRoundedRect(square, RADIUS, RADIUS);
    painter.setClipPath(shape);
    painter.fillPath(shape, QColor(properties.value("backgroundColor", "#1d1f21").toString()));

    const qreal padding = properties.value("iconPadding", 8).toReal();
    const QString icon = active ? properties.value("activeIcon", properties.value("icon")).toString()
                                : properties.value("icon").toString();
    drawIcon(painter, icon, square.adjusted(padding, padding, -padding, -padding));
    return pixmap;
}
