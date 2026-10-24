//
//  ScriptableAvatar.cpp
//  assignment-client/src/avatars
//
//  Created by Clement on 7/22/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptableAvatar.h"

#include <QDebug>
#include <QThread>
#include <glm/gtx/transform.hpp>

#include <shared/QtHelpers.h>
#include <AnimUtil.h>
#include <AnimClip.h>
#include <ClientTraitsHandler.h>
#include <GLMHelpers.h>
#include <ResourceRequestObserver.h>
#include <AvatarLogging.h>
#include <EntityItem.h>
#include <EntityItemProperties.h>


ScriptableAvatar::ScriptableAvatar() {
    _clientTraitsHandler.reset(new ClientTraitsHandler(this));
}

QByteArray ScriptableAvatar::toByteArrayStateful(AvatarDataDetail dataDetail, bool dropFaceTracking) {
    _globalPosition = getWorldPosition();
    return AvatarData::toByteArrayStateful(dataDetail, dropFaceTracking);
}


// hold and priority unused but kept so that client side JS can run.
void ScriptableAvatar::startAnimation(const QString& url, float fps, float priority,
                              bool loop, bool hold, float firstFrame, float lastFrame, const QStringList& maskedJoints) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "startAnimation", Q_ARG(const QString&, url), Q_ARG(float, fps),
                                  Q_ARG(float, priority), Q_ARG(bool, loop), Q_ARG(bool, hold), Q_ARG(float, firstFrame),
                                  Q_ARG(float, lastFrame), Q_ARG(const QStringList&, maskedJoints));
        return;
    }
    if (_animationFadeSeconds > 0.0f && !_lastPoses.empty()) {   // fade from what plays now
        if (!_fading && _animation && _animationDetails.url != QUrl(url)) {
            _fadeFromAnimation = _animation;                    // the old clip keeps playing through the fade
            _fadeFromDetails = _animationDetails;
            _fadeFromPoses.clear();
        } else {
            _fadeFromAnimation.clear();                         // mid-fade, or the same clip again: from the pose now
            _fadeFromPoses = _lastPoses;
        }
        _fading = true;
        _fadeElapsed = 0.0f;
    }
    _animation = DependencyManager::get<AnimationCache>()->getAnimation(url);
    _animationDetails = AnimationDetails("", QUrl(url), fps, 0, loop, hold, false, firstFrame, lastFrame, true, firstFrame, false);
    _maskedJoints = maskedJoints;
}

bool ScriptableAvatar::samplePoses(const AnimationPointer& animation, AnimationDetails& details, float deltaTime,
                                   AnimPoseVec& poses, bool& ended) {
    ended = false;
    if (!animation || !animation->isLoaded() || animation->getFrames().size() == 0 || !_animSkeleton) {
        return false;
    }
    const QString key = details.url.toString();
    auto found = _retargeted.find(key);
    if (found == _retargeted.end()) {
        found = _retargeted.insert(key, copyAndRetargetFromNetworkAnim(animation, _animSkeleton));
    }
    const std::vector<AnimPoseVec>& frames = found.value();
    if (frames.empty()) {
        return false;
    }
    float currentFrame = details.currentFrame + deltaTime * details.fps;
    const float lastFrame = std::min(details.lastFrame, (float)(frames.size() - 1));   // a clip ends at its own end
    if (details.loop) {
        while (currentFrame >= lastFrame && lastFrame > details.firstFrame) {
            currentFrame -= (lastFrame - details.firstFrame);
        }
    } else if (currentFrame >= lastFrame) {
        currentFrame = lastFrame;                               // a clip that does not loop stops and holds its last
        ended = true;                                           // frame (it wrapped round before: a nod file's later
    }                                                           // frames moved the wrist, 2026-10-10)
    details.currentFrame = currentFrame;
    const int nJoints = _animSkeleton->getNumJoints();
    const int frameCount = (int)frames.size();
    const AnimPoseVec& floorPoses = frames[(int)glm::floor(currentFrame) % frameCount];
    const AnimPoseVec& ceilPoses = frames[(int)glm::ceil(currentFrame) % frameCount];
    poses = _animSkeleton->getRelativeDefaultPoses();
    if ((int)floorPoses.size() == nJoints && (int)ceilPoses.size() == nJoints) {
        blend(nJoints, floorPoses.data(), ceilPoses.data(), glm::fract(currentFrame), poses.data());
    }
    return true;
}

void ScriptableAvatar::stopAnimation() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "stopAnimation");
        return;
    }
    _animation.clear();
}

AnimationDetails ScriptableAvatar::getAnimationDetails() {
    if (QThread::currentThread() != thread()) {
        AnimationDetails result;
        BLOCKING_INVOKE_METHOD(this, "getAnimationDetails", 
                                  Q_RETURN_ARG(AnimationDetails, result));
        return result;
    }
    return _animationDetails;
}

int ScriptableAvatar::getJointIndex(const QString& name) const {
    // Faux joints:
    int result = AvatarData::getJointIndex(name);
    if (result != -1) {
        return result;
    }
    QReadLocker readLock(&_jointDataLock);
    return _fstJointIndices.value(name) - 1;
}

QStringList ScriptableAvatar::getJointNames() const {
    QReadLocker readLock(&_jointDataLock);
    return _fstJointNames;
    return QStringList();
}

void ScriptableAvatar::setSkeletonModelURL(const QUrl& skeletonModelURL) {
    _bind.reset();
    _animSkeleton.reset();
    _retargeted.clear();

    AvatarData::setSkeletonModelURL(skeletonModelURL);
    updateJointMappings();
}

int ScriptableAvatar::sendAvatarDataPacket(bool sendAll) {
    using namespace std::chrono;
    auto now = Clock::now();

    int MAX_DATA_RATE_MBPS = 3;
    int maxDataRateBytesPerSeconds = MAX_DATA_RATE_MBPS * BYTES_PER_KILOBYTE * KILO_PER_MEGA / BITS_IN_BYTE;
    int maxDataRateBytesPerMilliseconds = maxDataRateBytesPerSeconds / MSECS_PER_SECOND;

    auto bytesSent = 0;

    if (now > _nextTraitsSendWindow) {
        if (getIdentityDataChanged()) {
            bytesSent += sendIdentityPacket();
        }

        bytesSent += _clientTraitsHandler->sendChangedTraitsToMixer();

        // Compute the next send window based on how much data we sent and what
        // data rate we're trying to max at.
        milliseconds timeUntilNextSend { bytesSent / maxDataRateBytesPerMilliseconds };
        _nextTraitsSendWindow += timeUntilNextSend;

        // Don't let the next send window lag behind if we're not sending a lot of data.
        if (_nextTraitsSendWindow < now) {
            _nextTraitsSendWindow = now;
        }
    }

    bytesSent += AvatarData::sendAvatarDataPacket(sendAll);

    return bytesSent;
}

void ScriptableAvatar::update(float deltatime) {
    // The .fst gives the skeleton's .fbx (setJointMappingsFromNetworkReply); load it once as the bind pose.
    // This line was lost when the .fst reader moved here (eb097af796), so scripted avatars never animated.
    if (_bind.isNull() && !_skeletonFBXURL.isEmpty()) {
        _bind = DependencyManager::get<AnimationCache>()->getAnimation(_skeletonFBXURL);
    }

    // Run animation: the frames retargeted onto this avatar's skeleton the way a client's AnimClip does it (2026-10-10:
    // composing the raw frames with the model's pre/post rotations and a fixed 1/100 scale crumpled the avatar), with a
    // crossfade between clips
    if (!_bind.isNull() && _bind->isLoaded()) {
        if (!_animSkeleton) {
            _animSkeleton = std::make_shared<AnimSkeleton>(_bind->getHFMModel());
            _retargeted.clear();
            _lastPoses.clear();
        }
        AnimPoseVec poses;
        bool ended = false;
        bool playing = samplePoses(_animation, _animationDetails, deltatime, poses, ended);
        AnimPoseVec from;
        bool fromEnded = false;
        bool haveFrom = false;
        if (_fading) {
            if (_fadeFromAnimation) {
                haveFrom = samplePoses(_fadeFromAnimation, _fadeFromDetails, deltatime, from, fromEnded);
            } else if (!_fadeFromPoses.empty()) {
                from = _fadeFromPoses;
                haveFrom = true;
            }
        }
        if (!playing && haveFrom) {
            poses = from;                                           // the new clip is still loading: the old one goes on
        } else if (playing && _fading) {
            _fadeElapsed += deltatime;
            float t = _animationFadeSeconds > 0.0f ? glm::clamp(_fadeElapsed / _animationFadeSeconds, 0.0f, 1.0f) : 1.0f;
            float alpha = t * t * (3.0f - 2.0f * t);                // ease in and out
            if (haveFrom && from.size() == poses.size() && alpha < 1.0f) {
                AnimPoseVec target = poses;
                blend(poses.size(), from.data(), target.data(), alpha, poses.data());
            }
            if (t >= 1.0f) {
                _fading = false;
                _fadeFromAnimation.clear();
                _fadeFromPoses.clear();
            }
        }
        if (playing || haveFrom) {
            const int nJoints = _animSkeleton->getNumJoints();
            for (const QString& name : _maskedJoints) {                 // a masked joint keeps its default pose
                int j = _animSkeleton->nameToJointIndex(name);
                if (j >= 0 && j < (int)poses.size()) {
                    poses[j] = _animSkeleton->getRelativeDefaultPose(j);
                }
            }
            _lastPoses = poses;
            AnimPoseVec absPoses = poses;
            _animSkeleton->convertRelativePosesToAbsolute(absPoses);
            QWriteLocker writeLock(&_jointDataLock);
            if (_jointData.size() != nJoints) {
                _jointData.resize(nJoints);
            }
            for (int i = 0; i < nJoints && i < (int)poses.size(); i++) {
                JointData& data = _jointData[i];
                AnimPose& absPose = absPoses[i];
                if (data.rotation != absPose.rot()) {
                    data.rotation = absPose.rot();
                    data.rotationIsDefaultPose = false;
                }
                AnimPose& relPose = poses[i];
                if (data.translation != relPose.trans()) {
                    data.translation = relPose.trans();
                    data.translationIsDefaultPose = false;
                }
            }
        }
        if (playing && ended && !_fading) {
            _animation.clear();                                     // a clip that ends holds its last pose
        }
    }

    quint64 now = usecTimestampNow();
    quint64 dt = now - _lastSendAvatarDataTime;

    if (dt > MIN_TIME_BETWEEN_MY_AVATAR_DATA_SENDS) {
        sendAvatarDataPacket();
        _lastSendAvatarDataTime = now;
    }
}

void ScriptableAvatar::updateJointMappings() {
    {
        QWriteLocker writeLock(&_jointDataLock);
        _fstJointIndices.clear();
        _fstJointNames.clear();
        _jointData.clear();
    }

    if (_skeletonModelURL.fileName().toLower().endsWith(".fst")) {
        ////
        // TODO: Should we rely upon HTTPResourceRequest for ResourceRequestObserver instead?
        // HTTPResourceRequest::doSend() covers all of the following and
        // then some. It doesn't cover the connect() call, so we may
        // want to add a HTTPResourceRequest::doSend() method that does
        // connects.
        QNetworkAccessManager& networkAccessManager = NetworkAccessManager::getInstance();
        QNetworkRequest networkRequest = QNetworkRequest(_skeletonModelURL);
        networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        networkRequest.setHeader(QNetworkRequest::UserAgentHeader, TIVOLI_CLOUD_VR_USER_AGENT);
        DependencyManager::get<ResourceRequestObserver>()->update(
            _skeletonModelURL, -1, "AvatarData::updateJointMappings");
        QNetworkReply* networkReply = networkAccessManager.get(networkRequest);
        //
        ////
        connect(networkReply, &QNetworkReply::finished, this, &ScriptableAvatar::setJointMappingsFromNetworkReply);
    }
}

void ScriptableAvatar::setJointMappingsFromNetworkReply() {
    QNetworkReply* networkReply = static_cast<QNetworkReply*>(sender());
    // before we process this update, make sure that the skeleton model URL hasn't changed
    // since we made the FST request
    if (networkReply->url() != _skeletonModelURL) {
        qCDebug(avatars) << "Refusing to set joint mappings for FST URL that does not match the current URL";
        networkReply->deleteLater();
        return;
    }
    {
        QWriteLocker writeLock(&_jointDataLock);
        QByteArray line;
        while (!(line = networkReply->readLine()).isEmpty()) {
            line = line.trimmed();
            if (line.startsWith("filename")) {
                int filenameIndex = line.indexOf('=') + 1;
                if (filenameIndex > 0) {
                    _skeletonFBXURL = _skeletonModelURL.resolved(QString(line.mid(filenameIndex).trimmed()));
                }
            }
            if (!line.startsWith("jointIndex")) {
                continue;
            }
            int jointNameIndex = line.indexOf('=') + 1;
            if (jointNameIndex == 0) {
                continue;
            }
            int secondSeparatorIndex = line.indexOf('=', jointNameIndex);
            if (secondSeparatorIndex == -1) {
                continue;
            }
            QString jointName = line.mid(jointNameIndex, secondSeparatorIndex - jointNameIndex).trimmed();
            bool ok;
            int jointIndex = line.mid(secondSeparatorIndex + 1).trimmed().toInt(&ok);
            if (ok) {
                while (_fstJointNames.size() < jointIndex + 1) {
                    _fstJointNames.append(QString());
                }
                _fstJointNames[jointIndex] = jointName;
            }
        }
        for (int i = 0; i < _fstJointNames.size(); i++) {
            _fstJointIndices.insert(_fstJointNames.at(i), i + 1);
        }
    }
    networkReply->deleteLater();
}

AvatarEntityMap ScriptableAvatar::getAvatarEntityData() const {
    auto data = getAvatarEntityDataInternal(true);
    return data;
}

AvatarEntityMap ScriptableAvatar::getAvatarEntityDataNonDefault() const {
    auto data = getAvatarEntityDataInternal(false);
    return data;

}

AvatarEntityMap ScriptableAvatar::getAvatarEntityDataInternal(bool allProperties) const {
    // DANGER: Now that we store the AvatarEntityData in packed format this call is potentially Very Expensive!
    // Avoid calling this method if possible.
    AvatarEntityMap data;
    QUuid sessionID = getID();
    _avatarEntitiesLock.withReadLock([&] {
        for (const auto& itr : _entities) {
            QUuid id = itr.first;
            EntityItemPointer entity = itr.second;

            EncodeBitstreamParams params;
            auto desiredProperties = entity->getEntityProperties(params);
            desiredProperties += PROP_LOCAL_POSITION;
            desiredProperties += PROP_LOCAL_ROTATION;
            desiredProperties += PROP_LOCAL_VELOCITY;
            desiredProperties += PROP_LOCAL_ANGULAR_VELOCITY;
            desiredProperties += PROP_LOCAL_DIMENSIONS;
            EntityItemProperties properties = entity->getProperties(desiredProperties);

            QByteArray blob;
            EntityItemProperties::propertiesToBlob(sessionID, properties, blob, allProperties);
            data[id] = blob;
        }
    });
    return data;
}

void ScriptableAvatar::setAvatarEntityData(const AvatarEntityMap& avatarEntityData) {
    // Note: this is an invokable Script call
    // avatarEntityData is expected to be a map of QByteArrays that represent EntityItemProperties objects from JavaScript
    //
    if (avatarEntityData.size() > MAX_NUM_AVATAR_ENTITIES) {
        // the data is suspect
        qCDebug(avatars) << "discard suspect avatarEntityData with size =" << avatarEntityData.size();
        return;
    }

    // convert binary data to EntityItemProperties
    // NOTE: this operation is NOT efficient
    std::map<QUuid, EntityItemProperties> newProperties;
    AvatarEntityMap::const_iterator dataItr = avatarEntityData.begin();
    while (dataItr != avatarEntityData.end()) {
        EntityItemProperties properties;
        const QByteArray& blob = dataItr.value();
        if (!blob.isNull() && EntityItemProperties::blobToProperties(blob, properties)) {
            newProperties[dataItr.key()] = properties;
        }
        ++dataItr;
    }

    // delete existing entities not found in avatarEntityData
    std::vector<QUuid> idsToClear;
    _avatarEntitiesLock.withWriteLock([&] {
        std::map<QUuid, EntityItemPointer>::iterator entityItr = _entities.begin();
        while (entityItr != _entities.end()) {
            QUuid id = entityItr->first;
            std::map<QUuid, EntityItemProperties>::const_iterator propertiesItr = newProperties.find(id);
            if (propertiesItr == newProperties.end()) {
                idsToClear.push_back(id);
                entityItr = _entities.erase(entityItr);
            } else {
                ++entityItr;
            }
        }
    });

    // add or update entities
    _avatarEntitiesLock.withWriteLock([&] {
        std::map<QUuid, EntityItemProperties>::const_iterator propertiesItr = newProperties.begin();
        while (propertiesItr != newProperties.end()) {
            QUuid id = propertiesItr->first;
            const EntityItemProperties& properties = propertiesItr->second;
            std::map<QUuid, EntityItemPointer>::iterator entityItr = _entities.find(id);
            EntityItemPointer entity;
            if (entityItr != _entities.end()) {
                entity = entityItr->second;
                entity->setProperties(properties);
            } else {
                entity = EntityTypes::constructEntityItem(id, properties);
            }
            if (entity) {
                // build outgoing payload
                OctreePacketData packetData(false, AvatarTraits::MAXIMUM_TRAIT_SIZE);
                EncodeBitstreamParams params;
                EntityTreeElementExtraEncodeDataPointer extra { nullptr };
                OctreeElement::AppendState appendState = entity->appendEntityData(&packetData, params, extra);

                if (appendState == OctreeElement::COMPLETED) {
                    _entities[id] = entity;
                    QByteArray tempArray((const char*)packetData.getUncompressedData(), packetData.getUncompressedSize());
                    storeAvatarEntityDataPayload(id, tempArray);
                } else {
                    // payload doesn't fit
                    entityItr = _entities.find(id);
                    if (entityItr != _entities.end()) {
                        _entities.erase(entityItr);
                        idsToClear.push_back(id);
                    }

                }
            }
            ++propertiesItr;
        }
    });

    // clear deleted traits
    for (const auto& id : idsToClear) {
        clearAvatarEntity(id);
    }
}

void ScriptableAvatar::updateAvatarEntity(const QUuid& entityID, const QByteArray& entityData) {
    if (entityData.isNull()) {
        // interpret this as a DELETE
        std::map<QUuid, EntityItemPointer>::iterator itr = _entities.find(entityID);
        if (itr != _entities.end()) {
            _entities.erase(itr);
            clearAvatarEntity(entityID);
        }
        return;
    }

    EntityItemPointer entity;
    EntityItemProperties properties;
    if (!EntityItemProperties::blobToProperties(entityData, properties)) {
        // entityData is corrupt
        return;
    }

    std::map<QUuid, EntityItemPointer>::iterator itr = _entities.find(entityID);
    if (itr == _entities.end()) {
        // this is an ADD
        entity = EntityTypes::constructEntityItem(entityID, properties);
        if (entity) {
            OctreePacketData packetData(false, AvatarTraits::MAXIMUM_TRAIT_SIZE);
            EncodeBitstreamParams params;
            EntityTreeElementExtraEncodeDataPointer extra { nullptr };
            OctreeElement::AppendState appendState = entity->appendEntityData(&packetData, params, extra);

            if (appendState == OctreeElement::COMPLETED) {
                _entities[entityID] = entity;
                QByteArray tempArray((const char*)packetData.getUncompressedData(), packetData.getUncompressedSize());
                storeAvatarEntityDataPayload(entityID, tempArray);
            }
        }
    } else {
        // this is an UPDATE
        entity = itr->second;
        bool somethingChanged = entity->setProperties(properties);
        if (somethingChanged) {
            OctreePacketData packetData(false, AvatarTraits::MAXIMUM_TRAIT_SIZE);
            EncodeBitstreamParams params;
            EntityTreeElementExtraEncodeDataPointer extra { nullptr };
            OctreeElement::AppendState appendState = entity->appendEntityData(&packetData, params, extra);

            if (appendState == OctreeElement::COMPLETED) {
                QByteArray tempArray((const char*)packetData.getUncompressedData(), packetData.getUncompressedSize());
                storeAvatarEntityDataPayload(entityID, tempArray);
            }
        }
    }
}
