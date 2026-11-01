//
//  FriendsScriptingInterface.h
//  interface/src/chat
//
//  Friends, for scripts and the QML panels: the friend requests waiting for you (the bar's People badge), who sees
//  you're online and where (the server's value, the one setting Settings, chat and the website share), and a direct
//  message with someone (People's Message). The friends themselves come from libraries/chat's FriendsService, which
//  chat's Friends page shows too. The 1.1 friends fixes (Caity's "friends as recommended", 2026-10-28;
//  team/social/FRIENDS-AND-DISCOVERY.md F1, F3, F5).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#ifndef hifi_FriendsScriptingInterface_h
#define hifi_FriendsScriptingInterface_h

#include <QtCore/QMutex>
#include <QtCore/QObject>

#include <DependencyManager.h>

/**jsdoc
 * Your Darkride friends: the requests waiting for you, who sees you're online and where, and direct messages.
 * @namespace Friends
 *
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 *
 * @property {number} requestCount - The friend requests waiting for you to accept or decline; <code>0</code> while
 *     signed out. Asked for at sign-in and once a minute after. <em>Read-only.</em>
 * @property {string} availability - Who sees you're online, and where: <code>"all"</code> (everyone),
 *     <code>"friends"</code> or <code>"none"</code> (no one: you look offline); <code>""</code> until Darkride has said.
 *     <em>Read-only.</em> Change it with {@link Friends.setAvailability}.
 * @property {boolean} canMessage - Whether {@link Friends.openDirectMessage} can open a conversation: you're signed in
 *     and chat hasn't turned you away. <em>Read-only.</em>
 */
class FriendsScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

    Q_PROPERTY(int requestCount READ getRequestCount NOTIFY requestCountChanged)
    Q_PROPERTY(QString availability READ getAvailability NOTIFY availabilityChanged)
    Q_PROPERTY(bool canMessage READ getCanMessage NOTIFY canMessageChanged)

public:
    FriendsScriptingInterface();

    int getRequestCount() const;
    QString getAvailability() const;
    bool getCanMessage() const;

    /**jsdoc
     * Changes who sees you're online, and where, on Darkride's server. {@link Friends.availabilityChanged} follows when
     * the server has it, or with the old value when it refused.
     * @function Friends.setAvailability
     * @param {string} availability - <code>"all"</code>, <code>"friends"</code> or <code>"none"</code>.
     */
    Q_INVOKABLE void setAvailability(const QString& availability);

    /**jsdoc
     * Opens chat in front on your direct message with someone, started if you have none yet. While chat is still
     * signing in, it opens as soon as chat is ready.
     * @function Friends.openDirectMessage
     * @param {string} username - Their Darkride username.
     */
    Q_INVOKABLE void openDirectMessage(const QString& username);

    /**jsdoc
     * Asks Darkride again for your friends, the requests and your setting, after a friend action made elsewhere, so
     * {@link Friends.requestCount} and chat's list follow at once.
     * @function Friends.refresh
     */
    Q_INVOKABLE void refresh();

signals:
    /**jsdoc
     * Triggered when {@link Friends.requestCount} changes.
     * @function Friends.requestCountChanged
     * @returns {Signal}
     */
    void requestCountChanged();

    /**jsdoc
     * Triggered when Darkride says what {@link Friends.availability} is, or refuses a change of it.
     * @function Friends.availabilityChanged
     * @returns {Signal}
     */
    void availabilityChanged();

    /**jsdoc
     * Triggered when {@link Friends.canMessage} changes.
     * @function Friends.canMessageChanged
     * @returns {Signal}
     */
    void canMessageChanged();

private:
    // main thread: the services' values copied for scripts' threads, and the changes signalled
    void updateRequests();
    void updateAvailability();
    void updateCanMessage();

    mutable QMutex _mutex;
    int _requestCount { 0 };
    QString _availability;
    bool _canMessage { false };
};

#endif // hifi_FriendsScriptingInterface_h
