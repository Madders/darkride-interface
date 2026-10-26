//
//  AssetServer.h
//  assignment-client/src/assets
//
//  Created by Ryan Huffman on 2015/07/21
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AssetServer_h
#define hifi_AssetServer_h

#include <QtCore/QDir>
#include <QtCore/QThreadPool>
#include <QRunnable>

#include <ThreadedAssignment.h>

#include "AssetUtils.h"
#include "ReceivedMessage.h"

#include "RegisteredMetaTypes.h"

class AssetServer : public ThreadedAssignment {
    Q_OBJECT
public:
    AssetServer(ReceivedMessage& message);

    void aboutToFinish() override;

public slots:
    void run() override;

private slots:
    void completeSetup();

    void queueRequests(QSharedPointer<ReceivedMessage> packet, SharedNodePointer senderNode);
    void handleAssetGetInfo(QSharedPointer<ReceivedMessage> packet, SharedNodePointer senderNode);
    void handleAssetGet(QSharedPointer<ReceivedMessage> packet, SharedNodePointer senderNode);
    void handleAssetUpload(QSharedPointer<ReceivedMessage> packetList, SharedNodePointer senderNode);
    void handleAssetMappingOperation(QSharedPointer<ReceivedMessage> message, SharedNodePointer senderNode);

    void sendStatsPacket() override;

private:
    void replayRequests();

    void handleGetMappingOperation(ReceivedMessage& message, NLPacketList& replyPacket);
    void handleGetAllMappingOperation(NLPacketList& replyPacket);
    void handleSetMappingOperation(ReceivedMessage& message, bool hasWriteAccess, NLPacketList& replyPacket);
    void handleDeleteMappingsOperation(ReceivedMessage& message, bool hasWriteAccess, NLPacketList& replyPacket);
    void handleRenameMappingOperation(ReceivedMessage& message, bool hasWriteAccess, NLPacketList& replyPacket);
    void handleSetBakingEnabledOperation(ReceivedMessage& message, bool hasWriteAccess, NLPacketList& replyPacket);

    void handleAssetServerBackup(ReceivedMessage& message, NLPacketList& replyPacket);
    void handleAssetServerRestore(ReceivedMessage& message, NLPacketList& replyPacket);

    // Mapping file operations must be called from main assignment thread only
    bool loadMappingsFromFile();
    bool writeMappingsToFile();

    /// Set the mapping for path to hash
    bool setMapping(AssetUtils::AssetPath path, AssetUtils::AssetHash hash);

    /// Delete mapping `path`. Returns `true` if deletion of mappings succeeds, else `false`.
    bool deleteMappings(const AssetUtils::AssetPathList& paths);

    /// Rename mapping from `oldPath` to `newPath`. Returns true if successful
    bool renameMapping(AssetUtils::AssetPath oldPath, AssetUtils::AssetPath newPath);


    /// Delete any unmapped files from the local asset directory
    void cleanupUnmappedFiles();

    /// Delete any baked files for assets removed from the local asset directory
    void cleanupBakedFilesForDeletedAssets();

    QString getPathToAssetHash(const AssetUtils::AssetHash& assetHash);




    /// Remove baked paths when the original asset is deleteds
    void removeBakedPathsForDeletedAsset(AssetUtils::AssetHash originalAssetHash);

    AssetUtils::Mappings _fileMappings;

    QDir _resourcesDirectory;
    QDir _filesDirectory;

    /// Task pool for handling uploads and downloads of assets
    QThreadPool _transferTaskPool;


    QMutex _queuedRequestsMutex;
    bool _isQueueingRequests { true };
    using RequestQueue = QVector<QPair<QSharedPointer<ReceivedMessage>, SharedNodePointer>>;
    RequestQueue _queuedRequests;

    uint64_t _filesizeLimit;
};

#endif
