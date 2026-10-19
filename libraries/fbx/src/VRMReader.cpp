//
//  VRMReader.cpp
//  libraries/fbx/src
//
//  Created on 2026-10-19 for Darkride's avatar support.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "VRMReader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QtEndian>

namespace {

// VRM's humanoid bones (1.0's names; 0.x's are the same but for the thumb's), Darkride's names for them
const QHash<QString, QString>& BONE_NAMES() {
    static const QHash<QString, QString> names = [] {
        QHash<QString, QString> result {
            { "hips", "Hips" }, { "spine", "Spine" }, { "neck", "Neck" }, { "head", "Head" },
            { "leftEye", "LeftEye" }, { "rightEye", "RightEye" },
        };
        for (const char* side : { "left", "right" }) {
            const QString vrm(side);
            const QString darkride = vrm == "left" ? "Left" : "Right";
            result.insert(vrm + "UpperLeg", darkride + "UpLeg");
            result.insert(vrm + "LowerLeg", darkride + "Leg");
            result.insert(vrm + "Foot", darkride + "Foot");
            result.insert(vrm + "Toes", darkride + "ToeBase");
            result.insert(vrm + "Shoulder", darkride + "Shoulder");
            result.insert(vrm + "UpperArm", darkride + "Arm");
            result.insert(vrm + "LowerArm", darkride + "ForeArm");
            result.insert(vrm + "Hand", darkride + "Hand");
            // 1.0's thumb starts at its metacarpal, 0.x's at its "proximal", which is the same bone
            result.insert(vrm + "ThumbMetacarpal", darkride + "HandThumb1");
            result.insert(vrm + "ThumbProximal", darkride + "HandThumb2");
            result.insert(vrm + "ThumbDistal", darkride + "HandThumb3");
            for (const auto& finger : { std::make_pair("Index", "Index"), std::make_pair("Middle", "Middle"),
                                        std::make_pair("Ring", "Ring"), std::make_pair("Little", "Pinky") }) {
                result.insert(vrm + finger.first + "Proximal", darkride + "Hand" + finger.second + "1");
                result.insert(vrm + finger.first + "Intermediate", darkride + "Hand" + finger.second + "2");
                result.insert(vrm + finger.first + "Distal", darkride + "Hand" + finger.second + "3");
            }
        }
        return result;
    }();
    return names;
}

// 0.x's expression presets by 1.0's names
QString presetName(const QString& name) {
    static const QHash<QString, QString> PRESETS_0 {
        { "a", "aa" }, { "i", "ih" }, { "u", "ou" }, { "e", "ee" }, { "o", "oh" },
        { "blink_l", "blinkLeft" }, { "blink_r", "blinkRight" },
        { "joy", "happy" }, { "sorrow", "sad" }, { "fun", "relaxed" },
    };
    return PRESETS_0.value(name.toLower(), name);
}

}

VRMInfo readVRM(const QByteArray& glb) {
    VRMInfo info;
    const int HEADER = 12;
    const int CHUNK_HEADER = 8;
    if (glb.size() < HEADER + CHUNK_HEADER || !glb.startsWith("glTF") || glb.mid(HEADER + 4, 4) != "JSON") {
        return info;
    }
    const quint32 jsonLength = qFromLittleEndian<quint32>(glb.constData() + HEADER);
    if (jsonLength > (quint32)(glb.size() - HEADER - CHUNK_HEADER)) {
        return info;
    }
    const QJsonObject gltf = QJsonDocument::fromJson(glb.mid(HEADER + CHUNK_HEADER, (int)jsonLength)).object();
    const QJsonObject extensions = gltf["extensions"].toObject();
    const QJsonObject vrm0 = extensions["VRM"].toObject();
    const QJsonObject vrm1 = extensions["VRMC_vrm"].toObject();
    if (!vrm1.isEmpty()) {
        info.version = 1;
    } else if (!vrm0.isEmpty()) {
        info.version = 0;
    } else {
        return info;
    }

    const QJsonArray nodes = gltf["nodes"].toArray();
    const QJsonArray meshes = gltf["meshes"].toArray();
    auto targetName = [&](int mesh, int index) {
        const QJsonObject meshObject = meshes.at(mesh).toObject();
        QJsonArray names = meshObject["extras"].toObject()["targetNames"].toArray();
        if (names.isEmpty()) {
            names = meshObject["primitives"].toArray().at(0).toObject()["extras"].toObject()["targetNames"].toArray();
        }
        return names.at(index).toString();
    };

    // humanoid bones
    QHash<QString, int> bones;
    if (info.version == 1) {
        const QJsonObject humanBones = vrm1["humanoid"].toObject()["humanBones"].toObject();
        for (auto itr = humanBones.begin(); itr != humanBones.end(); itr++) {
            bones.insert(itr.key(), itr.value().toObject()["node"].toInt(-1));
        }
    } else {
        for (const QJsonValue& bone : vrm0["humanoid"].toObject()["humanBones"].toArray()) {
            bones.insert(bone.toObject()["bone"].toString(), bone.toObject()["node"].toInt(-1));
        }
        // 0.x's thumb bones are proximal, intermediate and distal; 1.0's, the same bones, metacarpal, proximal and distal
        for (const char* side : { "left", "right" }) {
            const QString thumb = QString(side) + "Thumb";
            const int proximal = bones.contains(thumb + "Proximal") ? bones.take(thumb + "Proximal") : -1;
            const int intermediate = bones.contains(thumb + "Intermediate") ? bones.take(thumb + "Intermediate") : -1;
            if (proximal >= 0) {
                bones.insert(thumb + "Metacarpal", proximal);
            }
            if (intermediate >= 0) {
                bones.insert(thumb + "Proximal", intermediate);
            }
        }
    }
    for (auto itr = bones.begin(); itr != bones.end(); itr++) {
        QString name = BONE_NAMES().value(itr.key());
        // the chest is the top of the spine (Spine2), where the arms and neck join, unless there's an upper chest
        if (itr.key() == "chest") {
            name = bones.contains("upperChest") ? "Spine1" : "Spine2";
        } else if (itr.key() == "upperChest") {
            name = "Spine2";
        }
        const QString nodeName = nodes.at(itr.value()).toObject()["name"].toString();
        if (!name.isEmpty() && !nodeName.isEmpty()) {
            info.jointNames.insert(nodeName, name);
        }
    }

    // expressions, by 1.0's preset names
    QHash<QString, QVector<BlendshapeSource>> expressions;
    if (info.version == 1) {
        const QJsonObject presets = vrm1["expressions"].toObject()["preset"].toObject();
        for (auto itr = presets.begin(); itr != presets.end(); itr++) {
            for (const QJsonValue& bind : itr.value().toObject()["morphTargetBinds"].toArray()) {
                const int mesh = nodes.at(bind.toObject()["node"].toInt(-1)).toObject()["mesh"].toInt(-1);
                const QString name = targetName(mesh, bind.toObject()["index"].toInt(-1));
                if (!name.isEmpty()) {
                    expressions[itr.key()].append({ name, (float)bind.toObject()["weight"].toDouble(1.0) });
                }
            }
        }
    } else {
        for (const QJsonValue& group : vrm0["blendShapeMaster"].toObject()["blendShapeGroups"].toArray()) {
            const QString preset = presetName(group.toObject()["presetName"].toString());
            for (const QJsonValue& bind : group.toObject()["binds"].toArray()) {
                const QString name = targetName(bind.toObject()["mesh"].toInt(-1), bind.toObject()["index"].toInt(-1));
                if (!name.isEmpty()) {
                    expressions[preset].append({ name, (float)bind.toObject()["weight"].toDouble(100.0) / 100.0f });
                }
            }
        }
    }
    auto use = [&](Blendshapes blendshape, const QString& preset, float weight) {
        for (BlendshapeSource source : expressions.value(preset)) {
            source.weight *= weight;
            info.blendshapes[(int)blendshape].append(source);
        }
    };
    // what voice drives, less than fully as the presets are a whole viseme each
    use(Blendshapes::JawOpen, "aa", 0.8f);
    use(Blendshapes::LipsFunnel, "ou", 0.8f);
    if (expressions.contains("blinkLeft") && expressions.contains("blinkRight")) {
        use(Blendshapes::EyeBlink_L, "blinkLeft", 1.0f);
        use(Blendshapes::EyeBlink_R, "blinkRight", 1.0f);
    } else {
        // both eyes in one, so half each: blinks close both together
        use(Blendshapes::EyeBlink_L, "blink", 0.5f);
        use(Blendshapes::EyeBlink_R, "blink", 0.5f);
    }

    // who made it, and on what terms
    if (info.version == 1) {
        const QJsonObject meta = vrm1["meta"].toObject();
        info.title = meta["name"].toString();
        QStringList authors;
        for (const QJsonValue& author : meta["authors"].toArray()) {
            authors << author.toString();
        }
        info.authors = authors.join(", ");
        info.licence = QString("avatar use %1, commercial use %2, %3").arg(meta["avatarPermission"].toString(),
            meta["commercialUsage"].toString(), meta["licenseUrl"].toString());
    } else {
        const QJsonObject meta = vrm0["meta"].toObject();
        info.title = meta["title"].toString();
        info.authors = meta["author"].toString();
        info.licence = QString("%1, avatar use %2, commercial use %3").arg(meta["licenseName"].toString(),
            meta["allowedUserName"].toString(), meta["commercialUssageName"].toString());
    }
    return info;
}
