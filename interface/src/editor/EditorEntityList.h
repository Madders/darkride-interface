//
//  EditorEntityList.h
//  interface/src/editor
//
//  Asks the entity server for its list of every entity in the domain, so the Hierarchy can show what has
//  not streamed in yet (NATIVE-EDITOR.md 8). The request and the reply are MessagesData packets sent point to
//  point (EntityListMessage.h); a server too old to know them never answers, and the request times out.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EditorEntityList_h
#define hifi_EditorEntityList_h

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QVector>

#include <EntityListMessage.h>

class EditorEntityList : public QObject {
    Q_OBJECT
public:
    enum class State {
        None,           // not asked yet
        Waiting,        // a request is out
        Answered,       // the last request was answered
        Unanswered      // no entity server, or it did not answer in time
    };

    explicit EditorEntityList(QObject* parent = nullptr);

    void request();
    State state() const { return _state; }

signals:
    void received(const QVector<EntityListMessage::Record>& records);
    void stateChanged();

private slots:
    void onData(QString channel, QByteArray data, QUuid senderID, bool localOnly);

private:
    void setState(State state);

    State _state { State::None };
    quint32 _requestNumber { 0 };
    QUuid _serverID;    // the entity server the request went to; only its reply counts
    QTimer _timeout;
};

#endif // hifi_EditorEntityList_h
