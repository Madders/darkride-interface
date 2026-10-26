//
//  ChatWindow.cpp
//  interface/src/chat
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ChatWindow.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QLocale>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QDesktopServices>
#include <QtGui/QImageReader>
#include <QtGui/QKeyEvent>
#include <QtGui/QMovie>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QClipboard>
#include <QtGui/QTextCursor>
#include <QtGui/QTextFrame>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QShortcut>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidgetAction>

#include <AccountManager.h>
#include <AddressManager.h>
#include <ChatService.h>
#include <FriendsService.h>
#include <SettingHandle.h>
#include <gl/GLHelpers.h>

#include "Application.h"
#include "InterfaceLogging.h"
#include "MainWindow.h"

// the web chat's colours (darkride_chat client/src/style.css), from the Darkride website's
static const QColor COLOUR_SIDEBAR("#171b2d");
static const QColor COLOUR_PAGE("#1c2136");
static const QColor COLOUR_SURFACE("#242a44");
static const QColor COLOUR_SELECTION("#333a5a");
static const QColor COLOUR_TEXT("#f4f6f8");
static const QColor COLOUR_MUTED("#9aa1bd");
static const QColor COLOUR_CYAN("#00afff");
static const QColor COLOUR_RED("#ff5555");
static const QColor COLOUR_VIOLET("#a548ff");
static const QColor COLOUR_GREEN("#50fa7b");
// a person's or room's own colour behind its first letter, as the web chat picks it (avatar.ts)
static const char* AVATAR_COLOURS[] = { "#0090d6", "#8a3bdc", "#2f9e5b", "#d4692b", "#c43f82", "#23a39a" };

// messages from one person this close together share one name and picture, as in Discord
static const qint64 GROUP_GAP_MS = 7 * 60 * 1000;
// a click each in a message's menu, as in the web client (client/src/emoji.ts)
static const QStringList QUICK_REACTIONS { "\U0001F44D", "\u2764\uFE0F", "\U0001F602", "\U0001F62E", "\U0001F622", "\U0001F389" };
static const QColor COLOUR_CHIP("#2b3150");
// the sidebar's first row, the Friends page
static const QString FRIENDS_ROW = "friends:";
// a friend's lists again at most this often when the window comes forward
static const qint64 FRIENDS_FRESH_MS = 15 * 1000;
static const int FRIENDS_REFRESH_MS = 60 * 1000;
static const QColor COLOUR_CHIP_MINE("#1d3a5f");
// a message's picture fits in this, as Discord's do in a narrower column
static const QSize PICTURE_MAX(400, 300);
static const QSize PICTURE_UNKNOWN(300, 200);
// a GIF bigger than this shows as its first frame from a thumbnail rather than playing
static const qint64 GIF_PLAY_MAX_BYTES = 12 * 1024 * 1024;
static const int AVATAR_FETCH_SIZE = 96;

static Setting::Handle<QByteArray> chatWindowGeometry("chat/windowGeometry");
static Setting::Handle<QString> chatLastRoom("chat/room");

enum RoomRole {
    RoomIdRole = Qt::UserRole,
    RoomKindRole,       // header, channel, dm, invite
    RoomUnreadRole,
    RoomNotificationsRole,
    RoomAvatarRole,     // a dm's or group's picture
    RoomSubtitleRole,   // "Invited", "3 Members"
};

static QColor colourFor(const QString& seed) {
    quint32 n = 0;
    for (QChar c : seed) {
        n = n * 31 + c.unicode();
    }
    return QColor(AVATAR_COLOURS[n % (sizeof(AVATAR_COLOURS) / sizeof(AVATAR_COLOURS[0]))]);
}

static QString initialOf(const QString& name) {
    QString trimmed = name;
    if (trimmed.startsWith('@') || trimmed.startsWith('#')) {
        trimmed.remove(0, 1);
    }
    trimmed = trimmed.trimmed();
    return trimmed.isEmpty() ? QString("?") : trimmed.left(trimmed.at(0).isHighSurrogate() ? 2 : 1).toUpper();
}

static QImage friendsIcon(int size, qreal pixelRatio) {
    QImage image(QSize(size, size) * pixelRatio, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(pixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(COLOUR_SURFACE);
    painter.drawEllipse(QRectF(0, 0, size, size));
    // a head and shoulders, and another behind
    auto person = [&](qreal x, qreal scale, const QColor& colour) {
        painter.setBrush(colour);
        qreal head = size * 0.2 * scale;
        painter.drawEllipse(QPointF(x, size * 0.4), head, head);
        painter.drawPie(QRectF(x - size * 0.3 * scale, size * 0.62, size * 0.6 * scale, size * 0.5 * scale), 0, 180 * 16);
    };
    person(size * 0.62, 0.8, COLOUR_MUTED);
    person(size * 0.42, 1.0, COLOUR_TEXT);
    return image;
}

static QImage avatarImage(const QString& seed, const QString& name, int size, qreal pixelRatio) {
    QImage image(QSize(size, size) * pixelRatio, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(pixelRatio);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colourFor(seed));
    painter.drawEllipse(QRectF(0, 0, size, size));
    QFont font = painter.font();
    font.setPixelSize(qRound(size * 0.42));
    font.setWeight(QFont::Medium);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, initialOf(name));
    return image;
}

// the part of @name:server before the colon
static QString localpartOf(const QString& userId) {
    QString id = userId.startsWith('@') ? userId.mid(1) : userId;
    int colon = id.indexOf(':');
    return colon < 0 ? id : id.left(colon);
}

// times as the system writes them (11:47 AM, or 11:47)
static QString whenText(qint64 ms) {
    QDateTime when = QDateTime::fromMSecsSinceEpoch(ms);
    QDate today = QDate::currentDate();
    QLocale locale = QLocale::system();
    QString clock = locale.toString(when.time(), QLocale::ShortFormat);
    if (when.date() == today) {
        return "Today at " + clock;
    }
    if (when.date() == today.addDays(-1)) {
        return "Yesterday at " + clock;
    }
    return locale.toString(when.date(), QLocale::ShortFormat) + " " + clock;
}

static QString dayText(qint64 ms) {
    return QLocale::system().toString(QDateTime::fromMSecsSinceEpoch(ms).date(), "MMMM d, yyyy");
}

static QString sizeText(qint64 bytes) {
    if (bytes <= 0) {
        return QString();
    }
    return QLocale::system().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

// a picture cut round, filling the circle
static QImage roundImage(const QImage& image, int size, qreal pixelRatio) {
    QImage round(QSize(size, size) * pixelRatio, QImage::Format_ARGB32_Premultiplied);
    round.setDevicePixelRatio(pixelRatio);
    round.fill(Qt::transparent);
    QPainter painter(&round);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath circle;
    circle.addEllipse(QRectF(0, 0, size, size));
    painter.setClipPath(circle);
    QSize filled = image.size().scaled(QSize(size, size), Qt::KeepAspectRatioByExpanding);
    painter.drawImage(QRectF((size - filled.width()) / 2.0, (size - filled.height()) / 2.0, filled.width(), filled.height()),
                      image);
    return round;
}

// a message's picture in its box, the corners rounded as Discord's
static QImage boxedImage(const QImage& image, const QSize& box, qreal pixelRatio) {
    QImage boxed(box * pixelRatio, QImage::Format_ARGB32_Premultiplied);
    boxed.setDevicePixelRatio(pixelRatio);
    boxed.fill(Qt::transparent);
    QPainter painter(&boxed);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath corners;
    corners.addRoundedRect(QRectF(0, 0, box.width(), box.height()), 6, 6);
    painter.setClipPath(corners);
    if (image.isNull()) {
        painter.fillPath(corners, COLOUR_SURFACE);
    } else {
        QSize fitted = image.size().scaled(box, Qt::KeepAspectRatio);
        painter.drawImage(QRectF((box.width() - fitted.width()) / 2.0, (box.height() - fitted.height()) / 2.0,
                                 fitted.width(), fitted.height()),
                          image);
    }
    return boxed;
}

// a picture's size in the conversation: its own, made to fit PICTURE_MAX, never enlarged
static QSize pictureBox(QSize size) {
    if (size.width() <= 0 || size.height() <= 0) {
        return PICTURE_UNKNOWN;
    }
    if (size.width() > PICTURE_MAX.width() || size.height() > PICTURE_MAX.height()) {
        size.scale(PICTURE_MAX, Qt::KeepAspectRatio);
    }
    return size.expandedTo(QSize(16, 16));
}

static QString compactJson(const QJsonValue& value) {
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    return value.toString();
}

// plain text as rich text: escaped, web addresses made links, line breaks kept
static QString richBody(const QString& body) {
    static const QRegularExpression url(R"((https?://[^\s<]*[^\s<.,;:!?)\]'"]))");
    QString html = body.toHtmlEscaped();
    html.replace(url, "<a href=\"\\1\">\\1</a>");
    return html;
}

// --- the room list: section headings, channels with "#", direct messages with the person's picture ----------------

class RoomDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QString kind = index.data(RoomKindRole).toString();
        return QSize(option.rect.width(), kind == "header" ? 34 : kind == "channel" ? 34 : 42);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QString kind = index.data(RoomKindRole).toString();
        QString name = index.data(Qt::DisplayRole).toString();
        QRect rect = option.rect;
        QFont font = option.font;

        if (kind == "header") {
            font.setPixelSize(11);
            font.setWeight(QFont::DemiBold);
            font.setCapitalization(QFont::AllUppercase);
            painter->setFont(font);
            painter->setPen(COLOUR_MUTED);
            painter->drawText(rect.adjusted(16, 12, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, name);
            painter->restore();
            return;
        }

        int unread = index.data(RoomUnreadRole).toInt();
        int notifications = index.data(RoomNotificationsRole).toInt();
        bool selected = option.state & QStyle::State_Selected;
        bool hovered = option.state & QStyle::State_MouseOver;
        bool bright = selected || hovered || unread > 0 || kind == "invite";

        QRectF row = QRectF(rect).adjusted(8, 1, -8, -1);
        QPainterPath shape;
        shape.addRoundedRect(row, 4, 4);
        if (selected) {
            painter->fillPath(shape, COLOUR_SELECTION);
        } else if (kind == "invite") {
            painter->fillPath(shape, QColor(165, 72, 255, 31));
        } else if (hovered) {
            painter->fillPath(shape, QColor(255, 255, 255, 13));
        }
        // a channel with new messages: the small white pip at the left edge, as Discord marks it
        if (kind == "channel" && unread > 0 && !selected) {
            QPainterPath pip;
            pip.addRoundedRect(QRectF(rect.left(), rect.center().y() - 4, 4, 8), 2, 2);
            painter->fillPath(pip, COLOUR_TEXT);
        }

        int x = row.left() + 8;
        if (kind != "channel") {
            QImage picture = index.data(RoomAvatarRole).value<QImage>();
            int size = 28;
            painter->drawImage(QRectF(x, row.center().y() - size / 2.0, size, size), picture);
            x += size + 10;
        } else {
            QFont hash = font;
            hash.setPixelSize(20);
            painter->setFont(hash);
            painter->setPen(COLOUR_MUTED);
            painter->drawText(QRect(x, row.top(), 20, row.height()), Qt::AlignCenter, "#");
            x += 28;
        }

        // the count on the right: red for mentions and direct messages, as Discord's
        QString badge;
        QColor badgeColour = COLOUR_SELECTION;
        if (notifications > 0) {
            badge = QString::number(notifications);
            badgeColour = COLOUR_RED;
        } else if (kind == "dm" && unread > 0) {
            badge = QString::number(unread);
        }
        int right = row.right() - 8;
        if (!badge.isEmpty()) {
            QFont small = font;
            small.setPixelSize(12);
            small.setWeight(QFont::Bold);
            painter->setFont(small);
            int width = std::max(18, QFontMetrics(small).horizontalAdvance(badge) + 10);
            QRectF pill(right - width, row.center().y() - 9, width, 18);
            QPainterPath path;
            path.addRoundedRect(pill, 9, 9);
            painter->fillPath(path, badgeColour);
            painter->setPen(Qt::white);
            painter->drawText(pill, Qt::AlignCenter, badge);
            right -= width + 6;
        }

        font.setPixelSize(15);
        font.setWeight(unread > 0 ? QFont::Medium : QFont::Normal);
        painter->setFont(font);
        painter->setPen(bright ? COLOUR_TEXT : COLOUR_MUTED);
        QRect textRect(x, row.top(), right - x, row.height());
        QString subtitle = index.data(RoomSubtitleRole).toString();
        if (!subtitle.isEmpty()) {
            QFontMetrics metrics(font);
            painter->drawText(textRect.adjusted(0, 0, 0, -14), Qt::AlignLeft | Qt::AlignVCenter,
                              metrics.elidedText(name, Qt::ElideRight, textRect.width()));
            QFont sub = font;
            sub.setPixelSize(12);
            sub.setWeight(QFont::Normal);
            painter->setFont(sub);
            painter->setPen(COLOUR_MUTED);
            painter->drawText(textRect.adjusted(0, 18, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetrics(sub).elidedText(subtitle, Qt::ElideRight, textRect.width()));
        } else {
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetrics(font).elidedText(name, Qt::ElideRight, textRect.width()));
        }
        painter->restore();
    }
};

// --- the window ---------------------------------------------------------------------------------------------------

QPointer<ChatWindow> ChatWindow::_instance;

QKeySequence ChatWindow::shortcut() {
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D);
}

void ChatWindow::toggle() {
    // a window coming or going beside the viewer's also changes the GL view's surface on macOS: no frame is drawn to
    // it meanwhile (gl::ViewChangeGuard); opening chat crashed in Apple's GL driver as build mode's panels had
    gl::ViewChangeGuard guard;
    if (!_instance) {
        _instance = new ChatWindow(qApp->getWindow());
    }
    if (_instance->isVisible() && _instance->isActiveWindow()) {
        _instance->hide();
        return;
    }
    _instance->show();
    _instance->raise();
    _instance->activateWindow();
    if (_instance->_composer->isEnabled()) {
        _instance->_composer->setFocus();
    }
}

ChatWindow::ChatWindow(QWidget* parent) : QWidget(parent, Qt::Tool) {
    setObjectName("DarkrideChat");
    setWindowTitle("Darkride Chat");
    setMinimumSize(640, 420);
    resize(940, 640);
    setStyleSheet(QString(R"(
        #DarkrideChat { background: %1; }
        #ChatSidebar { background: %2; }
        #ChatSidebarHead, #ChatRoomHead {
            color: %3; font-size: 15px; font-weight: 500; padding: 0 16px;
            border-bottom: 1px solid rgba(0, 0, 0, 0.35);
        }
        #ChatRooms { background: %2; border: none; outline: 0; }
        #ChatMe { background: #10142a; }
        #ChatMeName { color: %3; font-size: 14px; font-weight: 500; }
        #ChatMeStatus { color: %4; font-size: 12px; }
        #ChatState { color: %4; font-size: 15px; }
        #ChatTimeline { background: %1; border: none; }
        #ChatComposer {
            background: %5; color: %3; border: none; border-radius: 8px; padding: 8px 12px; font-size: 15px;
            selection-background-color: %6;
        }
        #ChatComposerNote { color: %4; font-size: 12px; }
        QPushButton {
            background: %7; color: #091023; border: none; border-radius: 4px; padding: 7px 16px; font-weight: 500;
        }
        QPushButton:hover { background: #33c0ff; }
        QPushButton#ChatDecline { background: %5; color: %3; }
        QMenu { background: #10142a; color: %3; border: 1px solid rgba(0, 0, 0, 0.5); padding: 6px; font-size: 14px; }
        QMenu::item { padding: 6px 28px 6px 10px; border-radius: 3px; }
        QMenu::item:selected { background: %7; color: #091023; }
        QMenu::item:disabled { color: %4; }
        QMenu::separator { height: 1px; background: rgba(255, 255, 255, 0.08); margin: 4px 6px; }
        #ChatFriends { background: %1; border: none; outline: 0; }
        #ChatFriendName { color: %3; font-size: 15px; font-weight: 500; }
        #ChatFriendStatus { color: %4; font-size: 12px; }
        #ChatFriendsHeading { color: %4; font-size: 12px; font-weight: 600; }
        #ChatFriendsNote { color: %4; font-size: 13px; }
        #ChatAddFriendTitle { color: %3; font-size: 16px; font-weight: 600; }
        QLineEdit#ChatAddFriendName {
            background: #10142a; color: %3; border: 1px solid rgba(0, 0, 0, 0.5); border-radius: 8px; padding: 10px 12px;
            font-size: 15px; selection-background-color: %6;
        }
        QPushButton#ChatTab { background: transparent; color: %4; padding: 4px 10px; }
        QPushButton#ChatTab:hover { background: rgba(255, 255, 255, 0.06); color: %3; }
        QPushButton#ChatTab:checked { background: %6; color: %3; }
        QPushButton#ChatAddTab { background: #248046; color: white; padding: 4px 10px; }
        QPushButton#ChatAddTab:checked { background: transparent; color: %8; }
        QPushButton#ChatRowButton { background: %5; color: %3; padding: 6px 12px; }
        QPushButton#ChatRowButton:hover { background: %6; }
        QPushButton#ChatRowAccept { background: #248046; color: white; padding: 6px 12px; }
        QToolButton#ChatQuickReaction {
            background: %5; border: none; border-radius: 4px; font-size: 20px; min-width: 34px; min-height: 34px;
        }
        QToolButton#ChatQuickReaction:hover { background: %6; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar::handle:vertical { background: %2; border-radius: 4px; min-height: 30px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )")
                      .arg(COLOUR_PAGE.name(), COLOUR_SIDEBAR.name(), COLOUR_TEXT.name(), COLOUR_MUTED.name(),
                           COLOUR_SURFACE.name(), COLOUR_SELECTION.name(), COLOUR_CYAN.name(), COLOUR_GREEN.name()));

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // the left: Darkride's channels, then direct messages, then who you are
    auto sidebar = new QWidget(this);
    sidebar->setObjectName("ChatSidebar");
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(240);
    auto sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    _spaceTitle = new QLabel("Darkride", sidebar);
    _spaceTitle->setObjectName("ChatSidebarHead");
    _spaceTitle->setFixedHeight(48);
    _spaceTitle->setTextFormat(Qt::PlainText);
    sidebarLayout->addWidget(_spaceTitle);
    _roomList = new QListWidget(sidebar);
    _roomList->setObjectName("ChatRooms");
    _roomList->setItemDelegate(new RoomDelegate(_roomList));
    _roomList->setMouseTracking(true);
    _roomList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _roomList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    sidebarLayout->addWidget(_roomList, 1);
    auto me = new QWidget(sidebar);
    me->setObjectName("ChatMe");
    me->setAttribute(Qt::WA_StyledBackground);
    me->setFixedHeight(52);
    auto meLayout = new QHBoxLayout(me);
    meLayout->setContentsMargins(8, 6, 8, 6);
    meLayout->setSpacing(8);
    _userAvatar = new QLabel(me);
    _userAvatar->setFixedSize(32, 32);
    meLayout->addWidget(_userAvatar);
    auto meText = new QVBoxLayout();
    meText->setSpacing(0);
    _userName = new QLabel(me);
    _userName->setObjectName("ChatMeName");
    _userStatus = new QLabel(me);
    _userStatus->setObjectName("ChatMeStatus");
    meText->addWidget(_userName);
    meText->addWidget(_userStatus);
    meLayout->addLayout(meText, 1);
    sidebarLayout->addWidget(me);
    layout->addWidget(sidebar);
    _me = me;
    _me->setCursor(Qt::PointingHandCursor);
    _me->setToolTip("Who sees where you are");
    _me->installEventFilter(this);

    // the right: the room's name, then its conversation and the message box (or why there is none)
    auto main = new QWidget(this);
    auto mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    _roomTitle = new QLabel(main);
    _roomTitle->setObjectName("ChatRoomHead");
    _roomTitle->setFixedHeight(48);
    _roomTitle->setTextFormat(Qt::PlainText);
    mainLayout->addWidget(_roomTitle);
    _pages = new QStackedWidget(main);
    mainLayout->addWidget(_pages, 1);
    layout->addWidget(main, 1);

    // page 0: signing in, signed out, or chat unavailable
    auto statePage = new QWidget(_pages);
    auto stateLayout = new QVBoxLayout(statePage);
    stateLayout->addStretch(1);
    _stateText = new QLabel(statePage);
    _stateText->setObjectName("ChatState");
    _stateText->setAlignment(Qt::AlignCenter);
    _stateText->setWordWrap(true);
    stateLayout->addWidget(_stateText);
    _retryButton = new QPushButton("Try Again", statePage);
    stateLayout->addWidget(_retryButton, 0, Qt::AlignHCenter);
    stateLayout->addStretch(1);
    _pages->addWidget(statePage);
    connect(_retryButton, &QPushButton::clicked, this, [] {
        if (auto service = DependencyManager::get<ChatService>()) {
            service->retry();
        }
    });

    // page 1: an invitation, with Join and Decline
    auto invitePage = new QWidget(_pages);
    auto inviteLayout = new QVBoxLayout(invitePage);
    inviteLayout->addStretch(1);
    _inviteText = new QLabel(invitePage);
    _inviteText->setObjectName("ChatState");
    _inviteText->setAlignment(Qt::AlignCenter);
    _inviteText->setWordWrap(true);
    inviteLayout->addWidget(_inviteText);
    auto inviteButtons = new QHBoxLayout();
    inviteButtons->addStretch(1);
    auto join = new QPushButton("Join", invitePage);
    auto decline = new QPushButton("Decline", invitePage);
    decline->setObjectName("ChatDecline");
    inviteButtons->addWidget(join);
    inviteButtons->addWidget(decline);
    inviteButtons->addStretch(1);
    inviteLayout->addLayout(inviteButtons);
    inviteLayout->addStretch(1);
    _pages->addWidget(invitePage);
    connect(join, &QPushButton::clicked, this, [this] {
        if (auto service = DependencyManager::get<ChatService>()) {
            service->join(_room);
        }
    });
    connect(decline, &QPushButton::clicked, this, [this] {
        if (auto service = DependencyManager::get<ChatService>()) {
            service->leave(_room);
        }
    });

    // page 2: the conversation
    auto conversation = new QWidget(_pages);
    auto conversationLayout = new QVBoxLayout(conversation);
    conversationLayout->setContentsMargins(0, 0, 0, 0);
    conversationLayout->setSpacing(0);
    _timeline = new QTextBrowser(conversation);
    _timeline->setObjectName("ChatTimeline");
    _timeline->setOpenLinks(false);
    _timeline->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _timeline->document()->setDocumentMargin(0);
    _timeline->document()->setDefaultStyleSheet(
        QString("a { color: %1; text-decoration: none; } body { color: %2; }").arg(COLOUR_CYAN.name(), COLOUR_TEXT.name()));
    conversationLayout->addWidget(_timeline, 1);
    auto composerArea = new QWidget(conversation);
    auto composerLayout = new QVBoxLayout(composerArea);
    composerLayout->setContentsMargins(16, 0, 16, 4);
    composerLayout->setSpacing(2);
    _composer = new QPlainTextEdit(composerArea);
    _composer->setObjectName("ChatComposer");
    _composer->setTabChangesFocus(true);
    _composer->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _composer->installEventFilter(this);
    composerLayout->addWidget(_composer);
    _composerNote = new QLabel(composerArea);
    _composerNote->setObjectName("ChatComposerNote");
    _composerNote->setFixedHeight(18);
    composerLayout->addWidget(_composerNote);
    conversationLayout->addWidget(composerArea);
    _pages->addWidget(conversation);

    // page 3: friends, with Discord's tabs
    auto friendsPage = new QWidget(_pages);
    auto friendsLayout = new QVBoxLayout(friendsPage);
    friendsLayout->setContentsMargins(0, 8, 0, 0);
    friendsLayout->setSpacing(6);
    auto tabs = new QHBoxLayout();
    tabs->setContentsMargins(16, 0, 16, 0);
    tabs->setSpacing(8);
    static const QList<QPair<QString, QString>> TABS { { "online", "Online" }, { "all", "All" }, { "pending", "Pending" },
                                                       { "add", "Add Friend" } };
    for (const auto& tab : TABS) {
        auto button = new QPushButton(tab.second, friendsPage);
        button->setObjectName(tab.first == "add" ? "ChatAddTab" : "ChatTab");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        QString id = tab.first;
        connect(button, &QPushButton::clicked, this, [this, id] { setFriendsTab(id); });
        tabs->addWidget(button);
        _friendsTabs.insert(tab.first, button);
    }
    tabs->addStretch(1);
    friendsLayout->addLayout(tabs);
    _addFriendNote = new QLabel(friendsPage);
    _addFriendNote->setObjectName("ChatFriendsNote");
    _addFriendNote->setContentsMargins(16, 0, 16, 0);
    _addFriendNote->setWordWrap(true);
    _addFriendNote->hide();
    friendsLayout->addWidget(_addFriendNote);
    _friendsPages = new QStackedWidget(friendsPage);
    friendsLayout->addWidget(_friendsPages, 1);
    auto listPage = new QWidget(_friendsPages);
    auto listLayout = new QVBoxLayout(listPage);
    listLayout->setContentsMargins(0, 0, 0, 0);
    _friendsList = new QListWidget(listPage);
    _friendsList->setObjectName("ChatFriends");
    _friendsList->setSelectionMode(QAbstractItemView::NoSelection);
    _friendsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _friendsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listLayout->addWidget(_friendsList, 1);
    _friendsEmpty = new QLabel(listPage);
    _friendsEmpty->setObjectName("ChatState");
    _friendsEmpty->setAlignment(Qt::AlignCenter);
    _friendsEmpty->setWordWrap(true);
    listLayout->addWidget(_friendsEmpty, 1);
    _friendsPages->addWidget(listPage);
    auto addPage = new QWidget(_friendsPages);
    auto addLayout = new QVBoxLayout(addPage);
    addLayout->setContentsMargins(16, 12, 16, 12);
    addLayout->setSpacing(8);
    auto addTitle = new QLabel("Add Friend", addPage);
    addTitle->setObjectName("ChatAddFriendTitle");
    addLayout->addWidget(addTitle);
    auto addText = new QLabel("You can add friends with their Darkride username.", addPage);
    addText->setObjectName("ChatFriendStatus");
    addLayout->addWidget(addText);
    auto addRow = new QHBoxLayout();
    _addFriendName = new QLineEdit(addPage);
    _addFriendName->setObjectName("ChatAddFriendName");
    _addFriendName->setPlaceholderText("Their username");
    addRow->addWidget(_addFriendName, 1);
    auto send = new QPushButton("Send Friend Request", addPage);
    addRow->addWidget(send);
    addLayout->addLayout(addRow);
    addLayout->addStretch(1);
    _friendsPages->addWidget(addPage);
    _pages->addWidget(friendsPage);
    connect(send, &QPushButton::clicked, this, &ChatWindow::sendFriendRequest);
    connect(_addFriendName, &QLineEdit::returnPressed, this, &ChatWindow::sendFriendRequest);
    _friendsTabs.value(_friendsTab)->setChecked(true);
    _friendsTimer = new QTimer(this);
    _friendsTimer->setInterval(FRIENDS_REFRESH_MS);
    connect(_friendsTimer, &QTimer::timeout, this, [this] { askFriends(true); });

    connect(_timeline, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.scheme() == "https" || url.scheme() == "http") {
            QDesktopServices::openUrl(url);
        } else if (url.scheme() == "chatopen") {
            int index = url.path().toInt();
            if (index >= 0 && index < _links.size()) {
                openMedia(_links[index].value("media").toObject(), _links[index].value("name").toString());
            }
        } else if (url.scheme() == "chatreact") {
            int index = url.path().toInt();
            auto service = DependencyManager::get<ChatService>();
            if (service && index >= 0 && index < _reactionLinks.size()) {
                service->react(_room, _reactionLinks[index].value("id").toString(), _reactionLinks[index].value("key").toString());
            }
        }
    });
    _timeline->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_timeline, &QWidget::customContextMenuRequested, this, &ChatWindow::showMessageMenu);
    connect(_timeline->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (_rendering) {
            // a redraw starts the document again at the top: not the reader scrolling there
            return;
        }
        _stickToBottom = value >= _timeline->verticalScrollBar()->maximum() - 4;
        auto service = DependencyManager::get<ChatService>();
        if (value == 0 && service && !_room.isEmpty() && !_roomStart && !_loadingOlder && !_items.isEmpty()) {
            _loadingOlder = true;
            _anchorBottom = true;
            service->loadOlder(_room);
        }
        markReadIfSeen();
    });
    // at the bottom, the conversation stays there as what's below grows (a picture coming in, a line laid out late)
    connect(_timeline->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int maximum) {
        if (!_rendering && _stickToBottom) {
            _timeline->verticalScrollBar()->setValue(maximum);
        }
    });
    connect(_composer->document(), &QTextDocument::contentsChanged, this, &ChatWindow::fitComposer);
    connect(_roomList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item && item->data(RoomKindRole).toString() != "header") {
            selectRoom(item->data(RoomIdRole).toString());
        }
    });

    // the menu's shortcut hides it again from here too (on Windows the menu's shortcuts work only in the main window)
    auto hideShortcut = new QShortcut(shortcut(), this);
    connect(hideShortcut, &QShortcut::activated, this, [this] {
        gl::ViewChangeGuard guard;
        hide();
    });

    if (auto service = DependencyManager::get<ChatService>()) {
        connect(service.data(), &ChatService::stateChanged, this, &ChatWindow::updateState);
        connect(service.data(), &ChatService::roomsChanged, this, &ChatWindow::updateRooms);
        connect(service.data(), &ChatService::timelineChanged, this, &ChatWindow::updateTimeline);
        connect(service.data(), &ChatService::commandDone, this, &ChatWindow::commandDone);
        connect(service.data(), &ChatService::syncStateChanged, this, &ChatWindow::updateSync);
        connect(service.data(), &ChatService::profileChanged, this, &ChatWindow::updateState);
        connect(service.data(), &ChatService::mediaReady, this, &ChatWindow::mediaReady);
        connect(service.data(), &ChatService::commandFailed, this, [this](const QString& op, const QString& message) {
            if (op == "send" || op == "join" || op == "leave" || op == "open" || op == "edit" || op == "delete" ||
                op == "react") {
                _composerNote->setText("Couldn't " + op + ": " + message);
            }
        });
    }

    if (auto friends = DependencyManager::get<FriendsService>()) {
        connect(friends.data(), &FriendsService::changed, this, &ChatWindow::updateFriends);
        connect(friends.data(), &FriendsService::availabilityChanged, this, [this] {
            auto service = DependencyManager::get<ChatService>();
            updateSync(service ? service->syncState() : QString());
        });
        connect(friends.data(), &FriendsService::actionDone, this,
                [this](const QString& action, const QString& username, bool ok, const QString& message) {
            if (action == "ask") {
                _addFriendNote->setText(ok ? message : "That didn't work: " + message);
                if (ok) {
                    _addFriendName->clear();
                }
            } else if (!ok) {
                _addFriendNote->setText("That didn't work: " + message);
            } else {
                _addFriendNote->clear();
            }
            _addFriendNote->setVisible(!_addFriendNote->text().isEmpty());
        });
    }

    QByteArray geometry = chatWindowGeometry.get();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    fitComposer();
    updateState();
}

void ChatWindow::updateState() {
    auto service = DependencyManager::get<ChatService>();
    bool ready = service && service->state() == ChatService::State::Ready;

    QString username = DependencyManager::get<AccountManager>()->getAccountInfo().getUsername();
    QString userId = service ? service->userId() : QString();
    QString shownName = service && ready && !service->profileName().isEmpty() ? service->profileName() : username;
    _userName->setText(shownName.isEmpty() ? QString("Not signed in") : shownName);
    _userAvatar->setPixmap(QPixmap::fromImage(roundAvatar(userId.isEmpty() ? username : userId,
                                                          shownName.isEmpty() ? "?" : shownName,
                                                          service && ready ? service->profileAvatar() : QString(), 32)));
    updateSync(service ? service->syncState() : QString());

    if (!ready) {
        if (!_room.isEmpty() && service) {
            service->close(_room);
        }
        _room.clear();
        _friendsShown = false;
        _items = QJsonArray();
        _rooms = QJsonArray();
        _roomList->clear();
        _roomTitle->clear();
        _stateText->setText(service ? service->stateText() : QString("Chat isn't running."));
        _retryButton->setVisible(service && service->state() == ChatService::State::Unavailable);
        _pages->setCurrentIndex(0);
        return;
    }
    updateRooms(service->rooms());
    askFriends();
}

void ChatWindow::updateSync(const QString& state) {
    auto service = DependencyManager::get<ChatService>();
    if (!service || service->state() != ChatService::State::Ready) {
        _userStatus->setText(service && service->state() == ChatService::State::SigningIn ? "Signing in…" : "Offline");
    } else if (state == "running" || state == "idle") {
        auto friends = DependencyManager::get<FriendsService>();
        QString availability = friends ? friends->availability() : QString();
        _userStatus->setText(availability == "none"      ? "Invisible"
                             : availability == "all"     ? "Online · seen by everyone"
                             : availability == "friends" ? "Online · seen by friends"
                                                         : "Online");
    } else if (state == "offline" || state == "error") {
        _userStatus->setText("Reconnecting…");
    } else {
        _userStatus->setText("Connecting…");
    }
}

void ChatWindow::updateRooms(const QJsonArray& rooms) {
    _rooms = rooms;
    auto service = DependencyManager::get<ChatService>();
    QJsonArray spaces = service ? service->spaces() : QJsonArray();
    QString spaceName = spaces.isEmpty() ? QString() : spaces.first().toObject().value("name").toString();
    _spaceTitle->setText(spaceName.isEmpty() ? QString("Darkride") : spaceName);
    // channels are the rooms the space lists; direct messages, and rooms outside the space, are conversations, as
    // the web chat files them
    QList<QJsonObject> invites, channels, dms;
    for (const QJsonValue& value : rooms) {
        QJsonObject room = value.toObject();
        if (room.value("invited").toBool()) {
            invites << room;
        } else if (!room.value("dm").toBool() && !room.value("space").toString().isEmpty()) {
            channels << room;
        } else {
            dms << room;
        }
    }
    // channels stay put, by name, as a server's channels do in Discord; direct messages come newest first, as the
    // library sends them
    std::sort(channels.begin(), channels.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value("name").toString().compare(b.value("name").toString(), Qt::CaseInsensitive) < 0;
    });

    QSignalBlocker blocker(_roomList);
    int scroll = _roomList->verticalScrollBar()->value();
    _roomList->clear();
    auto addHeader = [this](const QString& title) {
        auto item = new QListWidgetItem(title, _roomList);
        item->setData(RoomKindRole, "header");
        item->setFlags(Qt::NoItemFlags);
    };
    QListWidgetItem* current = nullptr;
    {
        auto item = new QListWidgetItem("Friends", _roomList);
        item->setData(RoomIdRole, FRIENDS_ROW);
        item->setData(RoomKindRole, "friends");
        item->setData(RoomAvatarRole, friendsIcon(28, devicePixelRatioF()));
        auto friends = DependencyManager::get<FriendsService>();
        item->setData(RoomNotificationsRole, friends ? friends->incoming().size() : 0);
        if (_friendsShown) {
            current = item;
        }
    }
    auto addRoom = [&](const QJsonObject& room, const QString& kind) {
        QString id = room.value("id").toString();
        QString name = room.value("name").toString();
        auto item = new QListWidgetItem(name, _roomList);
        item->setData(RoomIdRole, id);
        item->setData(RoomKindRole, kind);
        item->setData(RoomUnreadRole, room.value("unread").toInt());
        item->setData(RoomNotificationsRole, room.value("notifications").toInt());
        if (kind != "channel") {
            item->setData(RoomAvatarRole, roundAvatar(id, name, room.value("avatar").toString(), 28));
        }
        if (kind == "invite") {
            item->setData(RoomSubtitleRole, "Invited");
        } else if (kind == "group") {
            item->setData(RoomSubtitleRole, QString("%1 Members").arg(room.value("members").toInt()));
        }
        if (id == _room) {
            current = item;
        }
    };
    if (!invites.isEmpty()) {
        addHeader("Invites");
        for (const auto& room : invites) {
            addRoom(room, "invite");
        }
    }
    addHeader("Channels");
    for (const auto& room : channels) {
        addRoom(room, "channel");
    }
    if (!dms.isEmpty()) {
        addHeader("Direct Messages");
        for (const auto& room : dms) {
            addRoom(room, room.value("dm").toBool() ? "dm" : "group");
        }
    }
    if (current) {
        _roomList->setCurrentItem(current);
    }
    _roomList->verticalScrollBar()->setValue(scroll);
    blocker.unblock();

    if (_friendsShown) {
        // the Friends page stays up
    } else if (_room.isEmpty() || !current) {
        // the room shown last time, else the first channel (General)
        QString wanted = chatLastRoom.get();
        QString chosen;
        for (const QJsonValue& value : rooms) {
            if (value.toObject().value("id").toString() == wanted) {
                chosen = wanted;
            }
        }
        if (chosen.isEmpty() && !channels.isEmpty()) {
            chosen = channels.first().value("id").toString();
        }
        if (chosen.isEmpty() && !dms.isEmpty()) {
            chosen = dms.first().value("id").toString();
        }
        if (!chosen.isEmpty()) {
            selectRoom(chosen);
        } else {
            _room.clear();
            _roomTitle->clear();
            _stateText->setText("No rooms yet.");
            _retryButton->hide();
            _pages->setCurrentIndex(0);
        }
    } else {
        // an invitation accepted here or elsewhere becomes a room to talk in
        showRoomPage();
    }
}

QJsonObject ChatWindow::roomById(const QString& room) const {
    for (const QJsonValue& value : _rooms) {
        if (value.toObject().value("id").toString() == room) {
            return value.toObject();
        }
    }
    return QJsonObject();
}

void ChatWindow::selectRoom(const QString& room) {
    if (room == FRIENDS_ROW) {
        showFriendsPage();
        return;
    }
    auto service = DependencyManager::get<ChatService>();
    if (_friendsShown) {
        _friendsShown = false;
        if (room == _room) {
            // back to the room shown before the Friends page
            showRoomPage();
            renderTimeline();
            return;
        }
    }
    if (!service || room == _room) {
        return;
    }
    if (!_room.isEmpty()) {
        service->close(_room);
    }
    _room = room;
    _items = QJsonArray();
    qDeleteAll(_movies);
    _movies.clear();
    _pictureUses.clear();
    // the last room's pictures go with its document; the files stay for coming back
    _timeline->document()->clear();
    _resources.clear();
    _pictured.clear();
    _roomStart = false;
    _loadingOlder = false;
    _anchorBottom = false;
    _stickToBottom = true;
    stopEditing();
    chatLastRoom.set(room);
    for (int i = 0; i < _roomList->count(); i++) {
        if (_roomList->item(i)->data(RoomIdRole).toString() == room) {
            QSignalBlocker blocker(_roomList);
            _roomList->setCurrentRow(i);
        }
    }
    if (!roomById(room).value("invited").toBool()) {
        service->open(room);
    }
    showRoomPage();
    renderTimeline();
}

void ChatWindow::showRoomPage() {
    QJsonObject room = roomById(_room);
    bool dm = room.value("dm").toBool();
    QString name = room.value("name").toString();
    QString mark = dm ? "@" : room.value("space").toString().isEmpty() ? "" : "#";
    _roomTitle->setText(mark.isEmpty() ? name : mark + " " + name);
    if (room.value("invited").toBool()) {
        _inviteText->setText(dm ? name + " wants to talk with you." : "You're invited to " + name + ".");
        _pages->setCurrentIndex(1);
        return;
    }
    _composer->setPlaceholderText("Message " + mark + name);
    if (_pages->currentIndex() != 2) {
        _pages->setCurrentIndex(2);
        if (isActiveWindow()) {
            _composer->setFocus();
        }
    }
}

void ChatWindow::updateTimeline(const QString& room, const QJsonArray& items) {
    if (room != _room) {
        return;
    }
    _items = items;
    renderTimeline();
    markReadIfSeen();
}

void ChatWindow::commandDone(const QJsonObject& done) {
    QString op = done.value("op").toString();
    QString room = done.value("room").toString();
    if ((op == "open" || op == "back") && room == _room) {
        _roomStart = done.value("start").toBool();
        _loadingOlder = false;
        if (op == "open") {
            renderTimeline();   // the beginning of the room, or the end of "Loading…"
        }
    } else if (op == "join") {
        _room.clear();
        selectRoom(room);
    } else if (op == "create_dm" && !_dmWanted.isEmpty()) {
        // a friend's new direct message, from the Friends page
        _dmWanted.clear();
        selectRoom(room);
    } else if (op == "leave" && room == _room) {
        _room.clear();
        chatLastRoom.set(QString());
        updateRooms(_rooms);
    }
}

QImage ChatWindow::roundAvatar(const QString& seed, const QString& name, const QString& mxc, int size) {
    if (!mxc.isEmpty()) {
        auto found = _avatarImages.constFind(mxc);
        if (found != _avatarImages.constEnd() && !found->isNull()) {
            return roundImage(*found, size, devicePixelRatioF());
        }
        requestMedia("avatar|" + mxc, QJsonObject { { "url", mxc } }, AVATAR_FETCH_SIZE, AVATAR_FETCH_SIZE, true);
    }
    return avatarImage(seed, name, size, devicePixelRatioF());
}

QString ChatWindow::avatarSource(const QString& seed, const QString& name, const QString& mxc) {
    QUrl source("avatar:" + QString::number(qHash(seed + "\n" + initialOf(name) + "\n" + mxc)));
    bool loaded = !mxc.isEmpty() && _avatarImages.contains(mxc);
    if (!_resources.contains(source) || (loaded && !_pictured.contains(source))) {
        _timeline->document()->addResource(QTextDocument::ImageResource, source, roundAvatar(seed, name, mxc, 40));
        _resources.insert(source);
        if (loaded) {
            _pictured.insert(source);
        }
    }
    if (!mxc.isEmpty() && !loaded) {
        // the letter until the picture comes
        _pictureUses.insert("avatar|" + mxc, { source, QSize(40, 40), true });
    }
    return source.toString();
}

QString ChatWindow::mediaHtml(const QJsonObject& item) {
    QString kind = item.value("kind").toString();
    QString body = item.value("body").toString();
    QJsonObject media = item.value("media").toObject();
    QString muted = COLOUR_MUTED.name();
    QString mimetype = media.value("mimetype").toString();
    qint64 size = (qint64)media.value("size").toDouble();
    QJsonValue source = media.value("source");
    QJsonValue thumbnail = media.value("thumbnail_source");
    bool encrypted = source.toObject().contains("file");

    int link = _links.size();
    _links.append(QJsonObject { { "media", media }, { "name", body } });
    QString open = QString("chatopen:%1").arg(link);

    // a picture, or a video's still: what to fetch to show it, and how big
    QString key;
    QJsonValue shownSource;
    int width = 0, height = 0;
    QSize own((int)media.value("width").toDouble(), (int)media.value("height").toDouble());
    if (kind == "image" && mimetype == "image/gif" && !encrypted && (size == 0 || size <= GIF_PLAY_MAX_BYTES)) {
        key = "file|" + compactJson(source);   // the whole GIF, to play it
        shownSource = source;
    } else if (kind == "image" || kind == "video") {
        QSize box = pictureBox(own);
        if (!thumbnail.isNull() && !thumbnail.isUndefined() && (encrypted || kind == "video")) {
            // an encrypted picture's server can't make thumbnails of it: the sender's own, decrypted here
            key = "file|" + compactJson(thumbnail);
            shownSource = thumbnail;
        } else if (kind == "image" && !encrypted) {
            width = (int)std::ceil(box.width() * devicePixelRatioF());
            height = (int)std::ceil(box.height() * devicePixelRatioF());
            key = QString("thumb|%1|%2x%3").arg(compactJson(source)).arg(width).arg(height);
            shownSource = source;
        } else if (kind == "image") {
            key = "file|" + compactJson(source);
            shownSource = source;
        }
    }

    QString html;
    if (!key.isEmpty()) {
        QSize box = pictureBox(_pictureSizes.value(key, own));
        QUrl resource("picture:" + QString::number(qHash(key + QString("|%1x%2").arg(box.width()).arg(box.height()))));
        _pictureUses.insert(key, { resource, box, false });
        if (_mediaFiles.contains(key)) {
            if (QMovie* movie = _movies.value(key)) {
                if (!_resources.contains(resource)) {
                    _timeline->document()->addResource(QTextDocument::ImageResource, resource,
                                                       boxedImage(movie->currentImage(), box, devicePixelRatioF()));
                }
            } else if (!_resources.contains(resource)) {
                showPicture(key, _mediaFiles.value(key));
            }
        } else {
            if (!_resources.contains(resource)) {
                _timeline->document()->addResource(QTextDocument::ImageResource, resource,
                                                   boxedImage(QImage(), box, devicePixelRatioF()));
            }
            requestMedia(key, shownSource, width, height);
        }
        _resources.insert(resource);
        html += QString("<a href=\"%1\"><img src=\"%2\" width=\"%3\" height=\"%4\"></a>")
                    .arg(open, resource.toString())
                    .arg(box.width())
                    .arg(box.height());
        if (kind == "image") {
            return html;
        }
        html += "<br>";
    }
    // a file, a sound, or a video under its still: its name to open it by, and its size
    static const QMap<QString, QString> words { { "file", "File" }, { "video", "Video" }, { "audio", "Sound" },
                                               { "image", "Picture" } };
    html += QString("<span style=\"color:%1;\">%2:</span> <a href=\"%3\">%4</a>")
                .arg(muted, words.value(kind), open, body.toHtmlEscaped());
    QString bytes = sizeText(size);
    if (!bytes.isEmpty()) {
        html += QString(" <span style=\"color:%1; font-size:12px;\">%2</span>").arg(muted, bytes);
    }
    return html;
}

void ChatWindow::requestMedia(const QString& key, const QJsonValue& source, int width, int height, bool crop) {
    auto service = DependencyManager::get<ChatService>();
    if (!service || _mediaFiles.contains(key) || _mediaWanted.contains(key) || _mediaFailed.contains(key)) {
        return;
    }
    _mediaWanted.insert(key);
    service->fetchMedia(key, source, width, height, crop);
}

void ChatWindow::mediaReady(const QString& key, const QString& path, const QString& error) {
    if (!_mediaWanted.remove(key)) {
        return;
    }
    if (path.isEmpty()) {
        // asked for once: a picture the server hasn't got stays a letter or an empty frame
        qCDebug(interfaceapp) << "chat: no media for" << key << error;
        _mediaFailed.insert(key);
        _opening.remove(key);
        return;
    }
    _mediaFiles.insert(key, path);

    if (key.startsWith("avatar|")) {
        QString mxc = key.mid(7);
        QImage image(path);
        if (image.isNull()) {
            return;
        }
        _avatarImages.insert(mxc, image);
        for (const PictureUse& use : _pictureUses.values(key)) {
            _timeline->document()->addResource(QTextDocument::ImageResource, use.resource,
                                               roundImage(image, use.box.width(), devicePixelRatioF()));
            _pictured.insert(use.resource);
        }
        _pictureUses.remove(key);
        _timeline->viewport()->update();
        // the room list and the corner have their own copies
        updateState();
        return;
    }

    auto opening = _opening.find(key);
    if (opening != _opening.end()) {
        QString name = opening.value();
        _opening.erase(opening);
        // under its own name, so the system knows what opens it
        QString folder = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("darkride-chat");
        QDir().mkpath(folder);
        QString safe = name;
        safe.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");
        if (safe.trimmed().isEmpty()) {
            safe = "download";
        }
        QString copy = QDir(folder).filePath(safe);
        QFile::remove(copy);
        if (QFile::copy(path, copy)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(copy));
        }
    }
    if (_pictureUses.contains(key)) {
        showPicture(key, path);
    }
}

void ChatWindow::showPicture(const QString& key, const QString& path) {
    QImageReader reader(path);
    QSize own = reader.size();
    if (own.isValid() && !_pictureSizes.contains(key)) {
        // a message that didn't say how big its picture is: laid out again at the picture's own size
        QList<PictureUse> uses = _pictureUses.values(key);
        if (!uses.isEmpty() && uses.first().box == PICTURE_UNKNOWN && pictureBox(own) != PICTURE_UNKNOWN) {
            _pictureSizes.insert(key, own);
            QTimer::singleShot(0, this, [this] { renderTimeline(); });
            return;
        }
    }
    if (reader.supportsAnimation() && reader.imageCount() > 1) {
        if (_movies.contains(key)) {
            return;
        }
        auto movie = new QMovie(path, QByteArray(), this);
        _movies.insert(key, movie);
        connect(movie, &QMovie::frameChanged, this, [this, key] {
            QMovie* movie = _movies.value(key);
            if (!movie) {
                return;
            }
            QImage frame = movie->currentImage();
            for (const PictureUse& use : _pictureUses.values(key)) {
                _timeline->document()->addResource(QTextDocument::ImageResource, use.resource,
                                                   boxedImage(frame, use.box, devicePixelRatioF()));
            }
            _timeline->viewport()->update();
        });
        movie->start();
        return;
    }
    QImage image = reader.read();
    for (const PictureUse& use : _pictureUses.values(key)) {
        _timeline->document()->addResource(QTextDocument::ImageResource, use.resource,
                                           boxedImage(image, use.box, devicePixelRatioF()));
    }
    _timeline->viewport()->update();
}

void ChatWindow::openMedia(const QJsonObject& media, const QString& name) {
    QJsonValue source = media.value("source");
    if (source.isUndefined() || source.isNull()) {
        return;
    }
    QString key = "file|" + compactJson(source);
    _opening.insert(key, name);
    if (_mediaFiles.contains(key)) {
        _mediaWanted.insert(key);
        mediaReady(key, _mediaFiles.value(key), QString());
        return;
    }
    _composerNote->setText("Opening " + name + "…");
    QTimer::singleShot(4000, _composerNote, &QLabel::clear);
    requestMedia(key, source);
}

void ChatWindow::renderTimeline() {
    QScrollBar* scroll = _timeline->verticalScrollBar();
    int fromBottom = scroll->maximum() - scroll->value();
    int fromTop = scroll->value();

    QJsonObject room = roomById(_room);
    QString name = room.value("name").toString();
    bool dm = room.value("dm").toBool();
    QString muted = COLOUR_MUTED.name();
    // made again below, as the messages are; the GIFs playing carry on
    _pictureUses.clear();
    _links.clear();
    _reactionLinks.clear();
    _tableItems.clear();
    // the names the conversation knows, for who reacted
    QHash<QString, QString> names;
    for (const QJsonValue& value : _items) {
        QJsonObject item = value.toObject();
        if (!item.value("name").toString().isEmpty()) {
            names.insert(item.value("sender").toString(), item.value("name").toString());
        }
    }

    QString html = "<html><body style=\"font-size:15px;\">";
    if (_items.isEmpty() && !_roomStart) {
        html += QString("<p style=\"color:%1; margin:24px 16px;\">Loading…</p>").arg(muted);
    }
    if (_roomStart) {
        // the room's first message is here: a welcome, as Discord starts a channel
        html += QString("<p style=\"margin:32px 16px 4px 16px; font-size:28px; font-weight:bold;\">%1</p>")
                    .arg(dm ? name.toHtmlEscaped() : "Welcome to #" + name.toHtmlEscaped());
        html += QString("<p style=\"color:%1; margin:0 16px 8px 16px;\">%2</p>")
                    .arg(muted, dm ? "This is the beginning of your direct messages with " + name.toHtmlEscaped() + "."
                                   : "This is the start of the #" + name.toHtmlEscaped() + " channel.");
    } else if (_loadingOlder) {
        html += QString("<p style=\"color:%1; margin:12px 16px;\">Loading…</p>").arg(muted);
    }

    QString lastSender;
    qint64 lastTs = 0;
    QDate lastDay;
    for (int index = 0; index < _items.size(); index++) {
        QJsonObject item = _items[index].toObject();
        qint64 ts = (qint64)item.value("ts").toDouble();
        QString sender = item.value("sender").toString();
        QString senderName = item.value("name").toString();
        if (senderName.isEmpty()) {
            senderName = localpartOf(sender);
        }
        QDate day = QDateTime::fromMSecsSinceEpoch(ts).date();
        if (day != lastDay) {
            html += QString("<p align=\"center\" style=\"color:%1; font-size:12px; font-weight:500; margin:20px 16px 4px 16px;\">"
                            "%2</p>")
                        .arg(muted, dayText(ts));
            lastDay = day;
            lastSender.clear();
        }

        QString kind = item.value("kind").toString();
        QString body = item.value("body").toString();
        if (kind == "member") {
            // "Judas joined.", "Caitlyn invited Judas.": a line of its own, as the web chat draws it
            QString user = item.value("user_name").toString().toHtmlEscaped();
            QString by = senderName.toHtmlEscaped();
            QString change = item.value("change").toString();
            static const QMap<QString, QString> lines {
                { "joined", "%1 joined." }, { "left", "%1 left." }, { "invited", "%2 invited %1." },
                { "kicked", "%2 removed %1." }, { "banned", "%2 banned %1." },
                { "declined", "%1 declined the invitation." }, { "uninvited", "%2 took back %1's invitation." } };
            QString form = lines.value(change, "%1 changed.");
            QString userHtml = "<span style=\"color:" + COLOUR_TEXT.name() + ";\">" + user + "</span>";
            QString byHtml = "<span style=\"color:" + COLOUR_TEXT.name() + ";\">" + by + "</span>";
            QString line = form.contains("%2") ? form.arg(userHtml, byHtml) : form.arg(userHtml);
            bool away = change == "left" || change == "kicked" || change == "banned";
            _tableItems.append(-1);
            html += QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:10px;\"><tr>"
                            "<td width=\"72\" align=\"center\" style=\"color:%1;\">%2</td>"
                            "<td style=\"color:%3; padding-right:16px;\">%4&nbsp;&nbsp;"
                            "<span style=\"font-size:12px;\">%5</span></td></tr></table>")
                        .arg(away ? COLOUR_RED.name() : COLOUR_GREEN.name(), away ? "←" : "→", muted, line,
                             whenText(ts));
            lastSender.clear();
            continue;
        }
        QString text;
        if (kind == "text") {
            text = richBody(body);
        } else if (kind == "notice") {
            text = QString("<span style=\"color:%1;\">%2</span>").arg(muted, richBody(body));
        } else if (kind == "emote") {
            text = "<i>* " + senderName.toHtmlEscaped() + " " + richBody(body) + "</i>";
        } else if (kind == "image" || kind == "file" || kind == "video" || kind == "audio") {
            text = mediaHtml(item);
        } else if (kind == "undecryptable") {
            text = QString("<i style=\"color:%1;\">An encrypted message this device can't read yet.</i>").arg(muted);
        } else {
            text = QString("<i style=\"color:%1;\">A message this chat can't show yet.</i>").arg(muted);
        }
        QString state = item.value("state").toString();
        if (state == "sending" || state == "waiting") {
            text = QString("<span style=\"color:%1;\">%2</span>").arg(muted, text);
        } else if (state == "failed") {
            text += QString(" <span style=\"color:%1; font-size:12px;\">Not sent</span>").arg(COLOUR_RED.name());
        }
        if (item.value("edited").toBool()) {
            text += QString(" <span style=\"color:%1; font-size:11px;\">(edited)</span>").arg(muted);
        }
        if (kind != "image" && kind != "video") {
            text = "<span style=\"white-space:pre-wrap;\">" + text + "</span>";
        } else {
            text = "<div style=\"margin-top:4px;\">" + text + "</div>";
        }
        text += reactionsHtml(item, names);

        bool grouped = sender == lastSender && ts - lastTs < GROUP_GAP_MS;
        _tableItems.append(index);
        if (grouped) {
            html += QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:2px;\"><tr>"
                            "<td width=\"72\"></td><td style=\"padding-right:16px;\">%1</td></tr></table>")
                        .arg(text);
        } else {
            html += QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin-top:14px;\"><tr>"
                            "<td width=\"72\" valign=\"top\" style=\"padding-left:16px; padding-top:2px;\">"
                            "<img src=\"%1\" width=\"40\" height=\"40\"></td>"
                            "<td valign=\"top\" style=\"padding-right:16px;\">"
                            "<span style=\"color:%2; font-weight:500;\">%3</span>&nbsp;&nbsp;"
                            "<span style=\"color:%4; font-size:12px;\">%5</span><br>%6</td></tr></table>")
                        .arg(avatarSource(sender, senderName, item.value("avatar").toString()), COLOUR_CYAN.name(),
                             senderName.toHtmlEscaped(), muted,
                             whenText(ts), text);
        }
        lastSender = sender;
        lastTs = ts;
    }
    html += "<p style=\"margin:0 0 12px 0;\"></p></body></html>";
    _rendering = true;
    _timeline->setHtml(html);
    // laid out whole now: left to itself the document lays out a long conversation in steps afterwards, and "the
    // bottom" would be found too soon
    _timeline->document()->size();

    // stay at the bottom when there; keep one's place when reading back; older messages loading come in above
    if (_anchorBottom) {
        scroll->setValue(scroll->maximum() - fromBottom);
    } else if (_stickToBottom) {
        scroll->setValue(scroll->maximum());
    } else {
        scroll->setValue(fromTop);
    }
    _rendering = false;
    if (!_items.isEmpty()) {
        _anchorBottom = false;
    }
}

void ChatWindow::sendMessage() {
    auto service = DependencyManager::get<ChatService>();
    QString text = _composer->toPlainText();
    while (text.endsWith('\n') || text.endsWith(' ')) {
        text.chop(1);
    }
    if (!_editingId.isEmpty()) {
        // an edit to nothing would be a delete: that has its own item in the message's menu
        if (service && !text.trimmed().isEmpty()) {
            for (const QJsonValue& value : _items) {
                QJsonObject item = value.toObject();
                if (item.value("id").toString() == _editingId && item.value("body").toString() != text) {
                    service->edit(_room, _editingId, text);
                }
            }
        }
        stopEditing();
        return;
    }
    if (!service || _room.isEmpty() || text.trimmed().isEmpty()) {
        return;
    }
    service->send(_room, text);
    _composer->clear();
    _composerNote->clear();
    // one's own message is the newest: down to it, and staying there as it comes in
    _stickToBottom = true;
    QScrollBar* scroll = _timeline->verticalScrollBar();
    scroll->setValue(scroll->maximum());
}

QString ChatWindow::reactionsHtml(const QJsonObject& item, const QHash<QString, QString>& names) {
    QJsonArray reactions = item.value("reactions").toArray();
    if (reactions.isEmpty()) {
        return QString();
    }
    QString html = "<div style=\"margin-top:4px; font-size:13px;\">";
    for (const QJsonValue& value : reactions) {
        QJsonObject reaction = value.toObject();
        QStringList who;
        for (const QJsonValue& sender : reaction.value("senders").toArray()) {
            who.append(names.value(sender.toString(), localpartOf(sender.toString())));
        }
        bool mine = reaction.value("mine").toBool();
        _reactionLinks.append(QJsonObject { { "id", item.value("id") }, { "key", reaction.value("key") } });
        html += QString("<a href=\"chatreact:%1\" title=\"%2\"><span style=\"background-color:%3; color:%4;\">"
                        "&nbsp;%5&nbsp;%6&nbsp;</span></a>&nbsp; ")
                    .arg(_reactionLinks.size() - 1)
                    .arg(who.join(", ").toHtmlEscaped(), (mine ? COLOUR_CHIP_MINE : COLOUR_CHIP).name(),
                         (mine ? COLOUR_CYAN : COLOUR_TEXT).name(), reaction.value("key").toString().toHtmlEscaped())
                    .arg(reaction.value("count").toInt());
    }
    return html + "</div>";
}

int ChatWindow::messageAt(const QPoint& pos) const {
    // each message is a table of its own at the top of the document (renderTimeline notes them in _tableItems)
    QTextFrame* root = _timeline->document()->rootFrame();
    QTextFrame* frame = _timeline->cursorForPosition(pos).currentFrame();
    if (!frame || frame == root) {
        return -1;
    }
    while (frame->parentFrame() && frame->parentFrame() != root) {
        frame = frame->parentFrame();
    }
    int table = root->childFrames().indexOf(frame);
    return table >= 0 && table < _tableItems.size() ? _tableItems[table] : -1;
}

void ChatWindow::showMessageMenu(const QPoint& pos) {
    QPoint documentPos = pos + QPoint(_timeline->horizontalScrollBar()->value(), _timeline->verticalScrollBar()->value());
    QMenu* menu = _timeline->createStandardContextMenu(documentPos);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    auto service = DependencyManager::get<ChatService>();
    int index = messageAt(pos);
    QJsonObject item = index >= 0 && index < _items.size() ? _items[index].toObject() : QJsonObject();
    QString id = item.value("id").toString();
    bool sent = item.value("state").toString() == "sent";
    QList<QAction*> standard = menu->actions();
    QAction* first = standard.isEmpty() ? nullptr : standard.first();
    if (service && !id.isEmpty() && sent && item.value("kind").toString() != "member") {
        // a row of quick reactions on top, then the message's own items, then the browser's (Copy, Select All)
        auto row = new QWidget(menu);
        auto rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 4);
        rowLayout->setSpacing(4);
        for (const QString& key : QUICK_REACTIONS) {
            auto button = new QToolButton(row);
            button->setObjectName("ChatQuickReaction");
            button->setText(key);
            button->setAutoRaise(true);
            connect(button, &QToolButton::clicked, menu, [this, menu, id, key] {
                if (auto service = DependencyManager::get<ChatService>()) {
                    service->react(_room, id, key);
                }
                menu->close();
            });
            rowLayout->addWidget(button);
        }
        auto rowAction = new QWidgetAction(menu);
        rowAction->setDefaultWidget(row);
        menu->insertAction(first, rowAction);
        menu->insertSeparator(first);
        if (item.value("editable").toBool()) {
            QAction* edit = new QAction("Edit Message", menu);
            connect(edit, &QAction::triggered, this, [this, item] { startEditing(item); });
            menu->insertAction(first, edit);
        }
        QString kind = item.value("kind").toString();
        if (kind == "text" || kind == "emote" || kind == "notice") {
            QAction* copy = new QAction("Copy Text", menu);
            QString body = item.value("body").toString();
            connect(copy, &QAction::triggered, this, [body] { QApplication::clipboard()->setText(body); });
            menu->insertAction(first, copy);
        }
        if (item.value("own").toBool()) {
            QAction* remove = new QAction("Delete Message", menu);
            connect(remove, &QAction::triggered, this, [this, item] { deleteMessage(item); });
            menu->insertAction(first, remove);
        }
        if (first) {
            menu->insertSeparator(first);
        }
    }
    menu->popup(_timeline->viewport()->mapToGlobal(pos));
}

void ChatWindow::startEditing(const QJsonObject& item) {
    _editingId = item.value("id").toString();
    _composer->setPlainText(item.value("body").toString());
    _composer->moveCursor(QTextCursor::End);
    _composerNote->setText("Editing a message: Enter saves it, Esc cancels");
    _composer->setFocus();
}

void ChatWindow::stopEditing() {
    if (!_editingId.isEmpty()) {
        _editingId.clear();
        _composer->clear();
    }
    _composerNote->clear();
}

void ChatWindow::deleteMessage(const QJsonObject& item) {
    QMessageBox question(QMessageBox::NoIcon, "Delete Message", "Delete this message? It goes for everyone.",
                         QMessageBox::NoButton, this);
    QPushButton* remove = question.addButton("Delete", QMessageBox::DestructiveRole);
    question.addButton(QMessageBox::Cancel);
    question.setDefaultButton(QMessageBox::Cancel);
    question.exec();
    auto service = DependencyManager::get<ChatService>();
    if (question.clickedButton() == remove && service) {
        if (_editingId == item.value("id").toString()) {
            stopEditing();
        }
        service->deleteMessage(_room, item.value("id").toString());
    }
}

void ChatWindow::markReadIfSeen() {
    auto service = DependencyManager::get<ChatService>();
    if (!service || _room.isEmpty() || !isVisible() || !isActiveWindow() || _pages->currentIndex() != 2) {
        return;
    }
    QScrollBar* scroll = _timeline->verticalScrollBar();
    if (scroll->value() < scroll->maximum() - 4) {
        return;
    }
    QJsonObject room = roomById(_room);
    if (room.value("unread").toInt() > 0 || room.value("notifications").toInt() > 0) {
        service->markRead(_room);
    }
}

void ChatWindow::fitComposer() {
    // one line to start, growing with what's typed up to eight
    int lines = std::min(8, std::max(1, (int)_composer->document()->size().height()));
    int height = lines * _composer->fontMetrics().lineSpacing() + 2 * (int)_composer->document()->documentMargin() + 18;
    _composer->setFixedHeight(height);
}

bool ChatWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _me && event->type() == QEvent::MouseButtonRelease) {
        showAvailabilityMenu();
        return true;
    }
    if (watched == _composer && event->type() == QEvent::KeyPress) {
        auto key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(key->modifiers() & Qt::ShiftModifier)) {
            sendMessage();
            return true;
        }
        if (key->key() == Qt::Key_Escape && !_editingId.isEmpty()) {
            stopEditing();
            return true;
        }
        // Up in an empty message box edits your newest message, as in Discord
        if (key->key() == Qt::Key_Up && key->modifiers() == Qt::NoModifier && _editingId.isEmpty() &&
            _composer->toPlainText().isEmpty()) {
            for (int i = _items.size() - 1; i >= 0; i--) {
                QJsonObject item = _items[i].toObject();
                if (item.value("editable").toBool() && item.value("state").toString() == "sent") {
                    startEditing(item);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChatWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    markReadIfSeen();
    askFriends();
    _friendsTimer->start();
}

void ChatWindow::closeEvent(QCloseEvent* event) {
    // the close button hides it, under the guard (toggle)
    event->ignore();
    gl::ViewChangeGuard guard;
    hide();
}

void ChatWindow::hideEvent(QHideEvent* event) {
    _friendsTimer->stop();
    saveGeometrySetting();
    QWidget::hideEvent(event);
}

void ChatWindow::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange) {
        markReadIfSeen();
        if (isActiveWindow()) {
            askFriends();
        }
    }
}

void ChatWindow::askFriends(bool now) {
    auto friends = DependencyManager::get<FriendsService>();
    qint64 time = QDateTime::currentMSecsSinceEpoch();
    if (friends && (now || time - _friendsAskedAt > FRIENDS_FRESH_MS)) {
        _friendsAskedAt = time;
        friends->refresh();
    }
}

QString ChatWindow::userIdFor(const QString& username) const {
    auto service = DependencyManager::get<ChatService>();
    QString me = service ? service->userId() : QString();
    return "@" + username.toLower() + ":" + me.mid(me.indexOf(':') + 1);
}

void ChatWindow::showFriendsPage() {
    _friendsShown = true;
    stopEditing();
    for (int i = 0; i < _roomList->count(); i++) {
        if (_roomList->item(i)->data(RoomIdRole).toString() == FRIENDS_ROW) {
            QSignalBlocker blocker(_roomList);
            _roomList->setCurrentRow(i);
        }
    }
    _roomTitle->setText("Friends");
    _pages->setCurrentIndex(3);
    updateFriends();
    askFriends(true);
}

void ChatWindow::setFriendsTab(const QString& tab) {
    _friendsTab = tab;
    for (auto i = _friendsTabs.begin(); i != _friendsTabs.end(); i++) {
        i.value()->setChecked(i.key() == tab);
    }
    _addFriendNote->clear();
    _addFriendNote->hide();
    updateFriends();
    if (tab == "add") {
        _addFriendName->setFocus();
    }
}

void ChatWindow::updateFriends() {
    auto friends = DependencyManager::get<FriendsService>();
    QJsonArray incoming = friends ? friends->incoming() : QJsonArray();
    // the requests waiting for you: the sidebar row's count and the Pending tab's
    for (int i = 0; i < _roomList->count(); i++) {
        if (_roomList->item(i)->data(RoomIdRole).toString() == FRIENDS_ROW) {
            _roomList->item(i)->setData(RoomNotificationsRole, incoming.size());
        }
    }
    _friendsTabs.value("pending")->setText(incoming.isEmpty() ? QString("Pending") : QString("Pending (%1)").arg(incoming.size()));
    if (!_friendsShown || !friends) {
        return;
    }
    _friendsList->clear();
    if (_friendsTab == "add") {
        _friendsPages->setCurrentIndex(1);
        return;
    }
    _friendsPages->setCurrentIndex(0);

    auto button = [](const QString& text, const QString& name = "ChatRowButton") {
        auto b = new QPushButton(text);
        b->setObjectName(name);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    auto heading = [this](const QString& text) {
        auto label = new QLabel(text);
        label->setObjectName("ChatFriendsHeading");
        label->setContentsMargins(16, 8, 16, 4);
        auto item = new QListWidgetItem(_friendsList);
        item->setFlags(Qt::NoItemFlags);
        item->setSizeHint(QSize(0, 32));
        _friendsList->setItemWidget(item, label);
    };
    QString empty;
    if (_friendsTab == "online" || _friendsTab == "all") {
        QList<QJsonObject> shown;
        for (const QJsonValue& value : friends->friends()) {
            if (_friendsTab == "all" || value.toObject().value("online").toBool()) {
                shown.append(value.toObject());
            }
        }
        if (!shown.isEmpty()) {
            heading(QString("%1 — %2").arg(_friendsTab == "online" ? "ONLINE" : "ALL FRIENDS").arg(shown.size()));
        }
        for (const QJsonObject& person : shown) {
            QString username = person.value("username").toString();
            bool online = person.value("online").toBool();
            QJsonObject location = person.value("location").toObject();
            QString world = location.value("name").toString();
            QString status = !online ? "Offline" : world.isEmpty() ? "Online" : "In " + world;
            QList<QPushButton*> buttons;
            auto message = button("Message");
            connect(message, &QPushButton::clicked, this, [this, username] { messageFriend(username); });
            buttons.append(message);
            if (online && !location.isEmpty()) {
                // to where they are: the metaverse checks again that they let you (AddressManager::goToUser)
                auto join = button("Join", "ChatRowAccept");
                connect(join, &QPushButton::clicked, this, [username] {
                    DependencyManager::get<AddressManager>()->goToUser(username);
                });
                buttons.append(join);
            }
            auto more = button("…");
            connect(more, &QPushButton::clicked, this, [this, more, username] {
                QMenu* menu = new QMenu(this);
                menu->setAttribute(Qt::WA_DeleteOnClose);
                connect(menu->addAction("Remove Friend"), &QAction::triggered, this, [this, username] {
                    QMessageBox question(QMessageBox::NoIcon, "Remove Friend",
                                         QString("Remove %1 as a friend?").arg(username), QMessageBox::NoButton, this);
                    QPushButton* remove = question.addButton("Remove Friend", QMessageBox::DestructiveRole);
                    question.addButton(QMessageBox::Cancel);
                    question.setDefaultButton(QMessageBox::Cancel);
                    question.exec();
                    auto friends = DependencyManager::get<FriendsService>();
                    if (question.clickedButton() == remove && friends) {
                        friends->remove(username);
                    }
                });
                menu->popup(more->mapToGlobal(QPoint(0, more->height())));
            });
            buttons.append(more);
            addFriendRow(username, status, buttons, !online);
        }
        if (shown.isEmpty()) {
            empty = _friendsTab == "online" && !friends->friends().isEmpty()
                        ? "None of your friends are online right now."
                        : "No friends here yet. Add some with their Darkride username.";
        }
    } else {
        QJsonArray outgoing = friends->outgoing();
        if (!incoming.isEmpty() || !outgoing.isEmpty()) {
            heading(QString("PENDING — %1").arg(incoming.size() + outgoing.size()));
        }
        for (const QJsonValue& value : incoming) {
            QString username = value.toObject().value("username").toString();
            auto accept = button("Accept", "ChatRowAccept");
            connect(accept, &QPushButton::clicked, this, [username] {
                DependencyManager::get<FriendsService>()->accept(username);
            });
            auto decline = button("Decline");
            connect(decline, &QPushButton::clicked, this, [username] {
                DependencyManager::get<FriendsService>()->decline(username);
            });
            addFriendRow(username, "Wants to be your friend", { accept, decline });
        }
        for (const QJsonValue& value : outgoing) {
            QString username = value.toObject().value("username").toString();
            auto cancel = button("Cancel");
            connect(cancel, &QPushButton::clicked, this, [username] {
                DependencyManager::get<FriendsService>()->withdraw(username);
            });
            addFriendRow(username, "Friend request sent", { cancel });
        }
        if (incoming.isEmpty() && outgoing.isEmpty()) {
            empty = "No friend requests waiting.";
        }
    }
    if (!friends->error().isEmpty()) {
        empty = friends->error();
    }
    _friendsEmpty->setText(empty);
    _friendsEmpty->setVisible(!empty.isEmpty());
    _friendsList->setVisible(_friendsList->count() > 0);
}

void ChatWindow::addFriendRow(const QString& username, const QString& status, const QList<QPushButton*>& buttons,
                              bool dimmed) {
    // their picture: the one their direct message shows, if you have one with them
    QString userId = userIdFor(username);
    QString mxc;
    for (const QJsonValue& value : _rooms) {
        QJsonObject room = value.toObject();
        if (room.value("dm_user").toString() == userId) {
            mxc = room.value("avatar").toString();
        }
    }
    auto row = new QWidget();
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 6, 16, 6);
    layout->setSpacing(10);
    auto picture = new QLabel(row);
    picture->setFixedSize(32, 32);
    picture->setPixmap(QPixmap::fromImage(roundAvatar(userId, username, mxc, 32)));
    if (dimmed) {
        auto faded = new QGraphicsOpacityEffect(picture);
        faded->setOpacity(0.5);
        picture->setGraphicsEffect(faded);
    }
    layout->addWidget(picture);
    auto text = new QVBoxLayout();
    text->setSpacing(0);
    auto name = new QLabel(username, row);
    name->setObjectName("ChatFriendName");
    name->setTextFormat(Qt::PlainText);
    auto line = new QLabel(status, row);
    line->setObjectName("ChatFriendStatus");
    line->setTextFormat(Qt::PlainText);
    text->addWidget(name);
    text->addWidget(line);
    layout->addLayout(text, 1);
    for (QPushButton* b : buttons) {
        b->setParent(row);
        layout->addWidget(b);
    }
    auto item = new QListWidgetItem(_friendsList);
    item->setFlags(Qt::ItemIsEnabled);
    item->setSizeHint(QSize(0, 52));
    _friendsList->setItemWidget(item, row);
}

void ChatWindow::messageFriend(const QString& username) {
    QString userId = userIdFor(username);
    for (const QJsonValue& value : _rooms) {
        QJsonObject room = value.toObject();
        if (room.value("dm").toBool() && room.value("dm_user").toString() == userId) {
            selectRoom(room.value("id").toString());
            return;
        }
    }
    auto service = DependencyManager::get<ChatService>();
    if (!service || service->state() != ChatService::State::Ready) {
        return;
    }
    _dmWanted = userId;
    _addFriendNote->setText("Starting a conversation with " + username + "…");
    _addFriendNote->show();
    service->createDm(userId);
}

void ChatWindow::sendFriendRequest() {
    QString username = _addFriendName->text().trimmed();
    if (username.startsWith('@')) {
        username.remove(0, 1);
    }
    auto friends = DependencyManager::get<FriendsService>();
    if (username.isEmpty() || !friends) {
        return;
    }
    _addFriendNote->setText("Sending…");
    _addFriendNote->show();
    friends->ask(username);
}

void ChatWindow::showAvailabilityMenu() {
    auto friends = DependencyManager::get<FriendsService>();
    if (!friends || !DependencyManager::get<AccountManager>()->isLoggedIn()) {
        return;
    }
    QMenu* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction("Who sees where I am")->setEnabled(false);
    static const QList<QPair<QString, QString>> CHOICES { { "all", "Everyone" }, { "friends", "Friends" },
                                                          { "none", "Nobody" } };
    for (const auto& choice : CHOICES) {
        QAction* action = menu->addAction(choice.second);
        action->setCheckable(true);
        action->setChecked(friends->availability() == choice.first);
        QString value = choice.first;
        connect(action, &QAction::triggered, this, [value] {
            DependencyManager::get<FriendsService>()->setAvailability(value);
        });
    }
    menu->popup(_me->mapToGlobal(QPoint(8, -menu->sizeHint().height())));
}

void ChatWindow::saveGeometrySetting() {
    chatWindowGeometry.set(saveGeometry());
}
