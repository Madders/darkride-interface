//
//  ChatWindow.h
//  interface/src/chat
//
//  Darkride chat's window (View > Chat): the channels and direct messages down the left, the conversation and the
//  message box on the right, as the web chat at chat.darkride.ai lays them out. A window of its own beside the
//  viewer's, so opening it never resizes the 3D view. ChatService (libraries/chat) does the talking.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ChatWindow_h
#define hifi_ChatWindow_h

#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QKeySequence>
#include <QtWidgets/QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMovie;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTimer;

class ChatWindow : public QWidget {
    Q_OBJECT
public:
    // View > Chat: shows the window (made the first time), or hides it when it is already in front
    static void toggle();
    static QKeySequence shortcut();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    explicit ChatWindow(QWidget* parent);

    void updateState();
    void updateRooms(const QJsonArray& rooms);
    void updateTimeline(const QString& room, const QJsonArray& items);
    void commandDone(const QJsonObject& done);
    void updateSync(const QString& state);
    void selectRoom(const QString& room);
    QJsonObject roomById(const QString& room) const;
    void showRoomPage();
    void renderTimeline();
    void sendMessage();
    // the message under a point of the conversation (viewport coordinates): its index in _items, or -1
    int messageAt(const QPoint& pos) const;
    // right-click: quick reactions, Edit, Copy Text and Delete for the message there, then the browser's own
    void showMessageMenu(const QPoint& pos);
    // the message box edits one of your messages: Enter saves it, Esc stops
    void startEditing(const QJsonObject& item);
    void stopEditing();
    void deleteMessage(const QJsonObject& item);
    // a message's reactions as rich text: one chip per emoji with its count, yours lit, a click toggling yours
    QString reactionsHtml(const QJsonObject& item, const QHash<QString, QString>& names);

    // the Friends page (FriendsService, the metaverse's friends), as Discord's: Online, All, Pending and Add Friend;
    // the sidebar's first row opens it
    void showFriendsPage();
    void setFriendsTab(const QString& tab);
    void updateFriends();
    void addFriendRow(const QString& username, const QString& status, const QList<QPushButton*>& buttons,
                      bool dimmed = false);
    // a friend's direct message, made when there is none yet
    void messageFriend(const QString& username);
    void sendFriendRequest();
    // "who sees where I am" (the metaverse's availability), from your name at the foot of the sidebar
    void showAvailabilityMenu();
    // a Darkride username's chat account, on the chat's own server
    QString userIdFor(const QString& username) const;
    // the friends lists asked for again, unless they were a moment ago
    void askFriends(bool now = false);
    void markReadIfSeen();
    void fitComposer();
    void saveGeometrySetting();

    // a person's or room's round picture: theirs once fetched (mxc), until then the first letter of the name on a
    // colour of their own; avatarSource names it in the conversation's document, at 40 px
    QImage roundAvatar(const QString& seed, const QString& name, const QString& mxc, int size);
    QString avatarSource(const QString& seed, const QString& name, const QString& mxc);
    // a message's picture, file, video or sound as rich text: a picture shown at most 400 x 300, the rest as a link
    QString mediaHtml(const QJsonObject& item);
    void requestMedia(const QString& key, const QJsonValue& source, int width = 0, int height = 0, bool crop = false);
    void mediaReady(const QString& key, const QString& path, const QString& error);
    // fills the conversation's pictures waiting for this media, a GIF playing
    void showPicture(const QString& key, const QString& path);
    // the whole file, opened by the system's own app (Preview, Photos, a player)
    void openMedia(const QJsonObject& media, const QString& name);

    static QPointer<ChatWindow> _instance;

    QLabel* _spaceTitle { nullptr };
    QListWidget* _roomList { nullptr };
    QLabel* _userAvatar { nullptr };
    QLabel* _userName { nullptr };
    QLabel* _userStatus { nullptr };
    QLabel* _roomTitle { nullptr };
    QStackedWidget* _pages { nullptr };
    QLabel* _stateText { nullptr };
    QPushButton* _retryButton { nullptr };
    QLabel* _inviteText { nullptr };
    QTextBrowser* _timeline { nullptr };
    QPlainTextEdit* _composer { nullptr };
    QLabel* _composerNote { nullptr };
    QWidget* _me { nullptr };
    QStackedWidget* _friendsPages { nullptr };   // the list, or the Add Friend form
    QListWidget* _friendsList { nullptr };
    QLabel* _friendsEmpty { nullptr };
    QLineEdit* _addFriendName { nullptr };
    QLabel* _addFriendNote { nullptr };
    QHash<QString, QPushButton*> _friendsTabs;
    QString _friendsTab { "online" };
    bool _friendsShown { false };               // the Friends page is up (the last room stays _room)
    QString _dmWanted;                           // a friend's chat account whose new DM is being made, to open
    QTimer* _friendsTimer { nullptr };          // asks for the lists again every minute while the window is up
    qint64 _friendsAskedAt { 0 };

    QJsonArray _rooms;
    QString _room;            // the room shown
    QJsonArray _items;        // its newest messages
    bool _roomStart { false };    // its first message is among them: nothing older to load
    bool _loadingOlder { false };
    bool _anchorBottom { false }; // the next render keeps the distance from the bottom (older messages came in above)
    bool _stickToBottom { true }; // the reader is at the newest message: the conversation keeps them there
    bool _rendering { false };    // the document is being made again: its scroll changes are not the reader's

    // where the conversation shows a picture: its name in the document and its size there
    struct PictureUse {
        QUrl resource;
        QSize box;
        bool round { false };
    };
    QSet<QUrl> _resources;                      // the pictures already given to the conversation's document
    QSet<QUrl> _pictured;                       // the avatars among them that show the person's own picture
    QHash<QString, QString> _mediaFiles;        // media key → its file
    QSet<QString> _mediaWanted;                 // asked for, not answered yet
    QSet<QString> _mediaFailed;                 // asked for and refused: not asked again this session
    QMultiHash<QString, PictureUse> _pictureUses;   // media key → where the conversation shows it
    QHash<QString, QSize> _pictureSizes;        // media key → a picture's own size, when its message didn't say
    QHash<QString, QImage> _avatarImages;       // mxc → a person's or room's picture
    QHash<QString, QMovie*> _movies;            // media key → a GIF playing in the conversation
    QHash<QString, QString> _opening;           // media key → the name to open the file under, once it is here
    QVector<QJsonObject> _links;                // chatopen:<n> in the conversation → {media, name}
    QVector<QJsonObject> _reactionLinks;        // chatreact:<n> → {id, key}
    QVector<int> _tableItems;                   // the document's top-level tables in order → index in _items (-1: not
                                                // a message)
    QString _editingId;                         // the message the message box is editing, if any
};

#endif // hifi_ChatWindow_h
