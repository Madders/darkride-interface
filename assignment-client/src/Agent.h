//
//  Agent.h
//  assignment-client/src
//
//  Created by Stephen Birarda on 7/1/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Agent_h
#define hifi_Agent_h

#include <atomic>
#include <memory>
#include <vector>

#include <QtScript/QScriptEngine>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QUuid>

#include <EntityEditPacketSender.h>
#include <EntityTree.h>
#include <ScriptEngine.h>
#include <ThreadedAssignment.h>
#include <ViewFrustum.h>

#include <plugins/CodecPlugin.h>

#include "AudioGate.h"
#include "MixedAudioStream.h"
#include "entities/EntityTreeHeadlessViewer.h"
#include "avatars/ScriptableAvatar.h"

class Agent : public ThreadedAssignment {
    Q_OBJECT

    Q_PROPERTY(bool isAvatar READ isAvatar WRITE setIsAvatar)
    Q_PROPERTY(bool isPlayingAvatarSound READ isPlayingAvatarSound)
    Q_PROPERTY(bool isListeningToAudioStream READ isListeningToAudioStream WRITE setIsListeningToAudioStream)
    Q_PROPERTY(bool isNoiseGateEnabled READ isNoiseGateEnabled WRITE setIsNoiseGateEnabled)
    Q_PROPERTY(float lastReceivedAudioLoudness READ getLastReceivedAudioLoudness)
    Q_PROPERTY(QUuid sessionUUID READ getSessionUUID)

public:
    Agent(ReceivedMessage& message);

    bool isPlayingAvatarSound() const { return _avatarSound != NULL; }

    bool isListeningToAudioStream() const { return _isListeningToAudioStream; }
    float getAvatarViewRadius() const { return _avatarViewRadius; }
    void setAvatarViewRadius(float radius) { _avatarViewRadius = std::max(0.0f, radius); }
    void setIsListeningToAudioStream(bool isListeningToAudioStream);

    bool isNoiseGateEnabled() const { return _isNoiseGateEnabled; }
    void setIsNoiseGateEnabled(bool isNoiseGateEnabled);

    float getLastReceivedAudioLoudness() const { return _lastReceivedAudioLoudness; }
    QUuid getSessionUUID() const;

    virtual void aboutToFinish() override;

public slots:
    void run() override;

    void playAvatarSound(SharedSoundPointer avatarSound);

    void setIsAvatar(bool isAvatar);
    bool isAvatar() const { return _isAvatar; }

    Q_INVOKABLE virtual void stop() override;

    /*@jsdoc
     * Takes what the agent has heard since the last call: the mixed audio the audio mixer sends this
     * agent, as 16-bit signed mono PCM at 24,000 Hz. Empty unless <code>isListeningToAudioStream</code>
     * is <code>true</code>. At most the last 30 s are kept between calls.
     * @function Agent.takeHeardAudio
     * @returns {ArrayBuffer} The audio heard since the last call.
     */
    Q_INVOKABLE QByteArray takeHeardAudio();

private slots:
    void requestScript();
    void scriptRequestFinished();
    void executeScript();

    void handleAudioPacket(QSharedPointer<ReceivedMessage> message);
    void handleOctreePacket(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);
    void handleSelectedAudioFormat(QSharedPointer<ReceivedMessage> message);
    void handleMismatchAudioFormat(SharedNodePointer node, const QString& currentCodec, const QString& receivedCodec);

    void nodeActivated(SharedNodePointer activatedNode);
    void nodeKilled(SharedNodePointer killedNode);

    void processAgentAvatarAudio();

    // an AI resident signed in with its account tells the metaverse where it is, like Interface does
    void sendPresence();

private:
    void negotiateAudioFormat();
    void selectAudioFormat(const QString& selectedCodecName);
    void encodeFrameOfZeros(QByteArray& encodedZeros);
    void computeLoudness(const QByteArray* decodedBuffer, QSharedPointer<ScriptableAvatar>);

    ScriptEnginePointer _scriptEngine;
    EntityEditPacketSender _entityEditSender;
    EntityTreeHeadlessViewer _entityViewer;

    MixedAudioStream _receivedAudioStream;
    float _lastReceivedAudioLoudness;
    QByteArray _heardAudio;     // mono PCM heard since the last takeHeardAudio()
    QMutex _heardAudioMutex;

    void setAvatarSound(SharedSoundPointer avatarSound) { _avatarSound = avatarSound; }

    void queryAvatars();

    QString _scriptContents;
    QTimer* _scriptRequestTimeout { nullptr };
    ResourceRequest* _pendingScriptRequest { nullptr };
    bool _isListeningToAudioStream = false;
    // the sphere around the agent's avatar in which every avatar counts as seen (ViewFrustum's centre sphere)
    std::atomic<float> _avatarViewRadius { DEFAULT_CENTER_SPHERE_RADIUS };
    SharedSoundPointer _avatarSound;
    bool _shouldMuteRecordingAudio { false };
    int _numAvatarSoundSentBytes = 0;
    bool _isAvatar = false;
    QTimer* _avatarQueryTimer = nullptr;
    QTimer* _presenceTimer { nullptr };
    QHash<QUuid, quint16> _outgoingScriptAudioSequenceNumbers;

    AudioGate _audioGate;
    bool _audioGateOpen { true };
    bool _isNoiseGateEnabled { false };

    CodecPluginPointer _codec;
    QString _selectedCodecName;
    Encoder* _encoder { nullptr };
    QTimer _avatarAudioTimer;
    bool _flushEncoder { false };
};

#endif // hifi_Agent_h
