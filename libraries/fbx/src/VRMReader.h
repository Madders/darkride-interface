//
//  VRMReader.h
//  libraries/fbx/src
//
//  Created on 2026-10-19 for Darkride's avatar support.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_VRMReader_h
#define hifi_VRMReader_h

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

#include <BlendshapeConstants.h>

// What Darkride uses of a VRM avatar (https://vrm.dev), from the VRM (0.x) or VRMC_vrm (1.0) extension of a glTF binary
// (.vrm, or a .glb that kept the extension): which nodes are which humanoid bones, which morph targets make the mouth
// shapes and blinks, and who made it and on what terms.
struct VRMInfo {
    int version { -1 };  // 0 for 0.x, 1 for 1.0; -1 if it isn't a VRM
    QHash<QString, QString> jointNames;  // a node's name: Darkride's name for its joint
    QHash<int, QVector<BlendshapeSource>> blendshapes;  // a Blendshapes value: the morph targets to blend for it
    QString title;
    QString authors;
    QString licence;

    bool isVRM() const { return version >= 0; }
};

VRMInfo readVRM(const QByteArray& glb);

#endif // hifi_VRMReader_h
