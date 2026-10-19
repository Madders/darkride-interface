//
//  SecondLifeSkeleton.cpp
//  libraries/fbx/src
//
//  Created on 2026-10-19 for Darkride's avatar support.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SecondLifeSkeleton.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include <QHash>
#include <QString>

#include <assimp/scene.h>

namespace {

// Second Life's skeleton (avatar_skeleton.xml), parents before children: each joint's parent, its bones and their
// collision volumes. Bento's extra spine, face, wing, tail and hind-limb bones are left out: a file with them has its
// bones, and they hang from bones that are kept.
const std::vector<std::pair<QString, QString>>& PARENTS() {
    static const std::vector<std::pair<QString, QString>> parents = [] {
        std::vector<std::pair<QString, QString>> result {
            { "mPelvis", "" }, { "PELVIS", "mPelvis" }, { "BUTT", "mPelvis" },
            { "mTorso", "mPelvis" }, { "BELLY", "mTorso" }, { "LOWER_BACK", "mTorso" },
            { "LEFT_HANDLE", "mTorso" }, { "RIGHT_HANDLE", "mTorso" },
            { "mChest", "mTorso" }, { "CHEST", "mChest" }, { "UPPER_BACK", "mChest" },
            { "LEFT_PEC", "mChest" }, { "RIGHT_PEC", "mChest" },
            { "mNeck", "mChest" }, { "NECK", "mNeck" },
            { "mHead", "mNeck" }, { "HEAD", "mHead" }, { "mSkull", "mHead" }, { "mFaceRoot", "mHead" },
            { "mEyeLeft", "mHead" }, { "mEyeRight", "mHead" },
        };
        for (const QString& side : { QString("Left"), QString("Right") }) {
            const QString v = side == "Left" ? "L_" : "R_";
            result.insert(result.end(), {
                { "mCollar" + side, "mChest" }, { v + "CLAVICLE", "mCollar" + side },
                { "mShoulder" + side, "mCollar" + side }, { v + "UPPER_ARM", "mShoulder" + side },
                { "mElbow" + side, "mShoulder" + side }, { v + "LOWER_ARM", "mElbow" + side },
                { "mWrist" + side, "mElbow" + side }, { v + "HAND", "mWrist" + side },
                { "mHip" + side, "mPelvis" }, { v + "UPPER_LEG", "mHip" + side },
                { "mKnee" + side, "mHip" + side }, { v + "LOWER_LEG", "mKnee" + side },
                { "mAnkle" + side, "mKnee" + side }, { v + "FOOT", "mAnkle" + side },
                { "mFoot" + side, "mAnkle" + side }, { "mToe" + side, "mFoot" + side },
            });
        }
        return result;
    }();
    return parents;
}

QString parentOf(const QString& joint) {
    auto entry = std::find_if(PARENTS().begin(), PARENTS().end(),
                              [&](const std::pair<QString, QString>& parent) { return parent.first == joint; });
    return entry != PARENTS().end() ? entry->second : QString();
}

// The volume a missing bone can be found from: the volume's bind pose less its offset is the bone's
const QHash<QString, QString>& VOLUMES() {
    static const QHash<QString, QString> volumes = [] {
        QHash<QString, QString> result {
            { "mPelvis", "PELVIS" }, { "mTorso", "BELLY" }, { "mChest", "CHEST" }, { "mNeck", "NECK" }, { "mHead", "HEAD" },
        };
        for (const QString& side : { QString("Left"), QString("Right") }) {
            const QString v = side == "Left" ? "L_" : "R_";
            result.insert("mCollar" + side, v + "CLAVICLE");
            result.insert("mShoulder" + side, v + "UPPER_ARM");
            result.insert("mElbow" + side, v + "LOWER_ARM");
            result.insert("mWrist" + side, v + "HAND");
            result.insert("mHip" + side, v + "UPPER_LEG");
            result.insert("mKnee" + side, v + "LOWER_LEG");
            result.insert("mAnkle" + side, v + "FOOT");
        }
        return result;
    }();
    return volumes;
}

// A transform without its scale: a collision volume's bind pose carries the volume's size (L_UPPER_ARM is 0.19 long)
aiMatrix4x4 unscaled(const aiMatrix4x4& transform) {
    aiVector3D scaling, position;
    aiQuaternion rotation;
    transform.Decompose(scaling, rotation, position);
    return aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), rotation, position);
}

aiMatrix4x4 globalTransform(const aiNode* node) {
    aiMatrix4x4 transform = node->mTransformation;
    for (const aiNode* parent = node->mParent; parent; parent = parent->mParent) {
        transform = parent->mTransformation * transform;
    }
    return transform;
}

const aiNode* nodeWithMesh(const aiNode* node, unsigned int mesh) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        if (node->mMeshes[i] == mesh) {
            return node;
        }
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        if (const aiNode* found = nodeWithMesh(node->mChildren[i], mesh)) {
            return found;
        }
    }
    return nullptr;
}

void detach(aiNode* node) {
    aiNode* parent = node->mParent;
    if (!parent) {
        return;
    }
    aiNode** children = parent->mNumChildren > 1 ? new aiNode*[parent->mNumChildren - 1] : nullptr;
    unsigned int count = 0;
    for (unsigned int i = 0; i < parent->mNumChildren; i++) {
        if (parent->mChildren[i] != node) {
            children[count++] = parent->mChildren[i];
        }
    }
    delete[] parent->mChildren;
    parent->mChildren = children;
    parent->mNumChildren = count;
    node->mParent = nullptr;
}

void attach(aiNode* node, aiNode* parent) {
    aiNode** children = new aiNode*[parent->mNumChildren + 1];
    for (unsigned int i = 0; i < parent->mNumChildren; i++) {
        children[i] = parent->mChildren[i];
    }
    children[parent->mNumChildren] = node;
    delete[] parent->mChildren;
    parent->mChildren = children;
    parent->mNumChildren++;
    node->mParent = parent;
}

}

QStringList rebuildSecondLifeSkeleton(aiScene* scene) {
    QStringList added;
    if (!scene || !scene->mRootNode) {
        return added;
    }
    aiNode* root = scene->mRootNode;
    auto find = [&](const QString& name) { return root->FindNode(name.toUtf8().constData()); };
    // only a body with Second Life's collision volumes and without the bones they belong under
    aiNode* pelvisVolume = find("PELVIS");
    if (!pelvisVolume || !pelvisVolume->mParent || find("mPelvis") || !find("HEAD") || find("mHead")) {
        return added;
    }
    aiNode* armature = pelvisVolume->mParent;

    // where each skinning joint was bound, less its size: the global transform of the mesh it skins, times its offset's
    // inverse
    QHash<QString, aiMatrix4x4> bindPoses;
    std::vector<aiMatrix4x4> meshTransforms;
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        const aiNode* meshNode = nodeWithMesh(root, meshIndex);
        meshTransforms.push_back(meshNode ? globalTransform(meshNode) : aiMatrix4x4());
        for (unsigned int i = 0; i < mesh->mNumBones; i++) {
            const QString name = mesh->mBones[i]->mName.C_Str();
            if (!bindPoses.contains(name)) {
                bindPoses.insert(name, unscaled(meshTransforms.back() * aiMatrix4x4(mesh->mBones[i]->mOffsetMatrix).Inverse()));
            }
        }
    }
    if (!bindPoses.contains("PELVIS")) {
        return added;
    }

    // the missing bones, from their volumes: a volume's transform is its offset from its bone
    QHash<QString, aiMatrix4x4> globals = bindPoses;
    for (const auto& joint : PARENTS()) {
        const QString& bone = joint.first;
        const QString volume = VOLUMES().value(bone);
        aiNode* volumeNode = volume.isEmpty() ? nullptr : find(volume);
        if (!volumeNode || find(bone) || !bindPoses.contains(volume)) {
            continue;
        }
        globals.insert(bone, bindPoses[volume] * unscaled(volumeNode->mTransformation).Inverse());
        attach(new aiNode(bone.toStdString()), armature);
        added << bone;
    }
    if (added.isEmpty()) {
        return added;
    }

    // every joint of the skeleton under its nearest parent there is
    for (const auto& joint : PARENTS()) {
        aiNode* node = find(joint.first);
        if (!node) {
            continue;
        }
        aiNode* parent = nullptr;
        for (QString parentName = joint.second; !parentName.isEmpty() && !parent; parentName = parentOf(parentName)) {
            parent = find(parentName);
        }
        if (!parent) {
            parent = armature;
        }
        if (node->mParent != parent) {
            detach(node);
            attach(node, parent);
        }
    }
    // then each joint at its rest, parents before children (the fingers and face too)
    std::function<void(aiNode*)> rest = [&](aiNode* node) {
        auto global = globals.find(node->mName.C_Str());
        if (global != globals.end() && node->mParent) {
            node->mTransformation = globalTransform(node->mParent).Inverse() * global.value();
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            rest(node->mChildren[i]);
        }
    };
    rest(armature);

    // and each skinning joint's offset to match its unscaled rest, so the skin lies as it was bound
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        for (unsigned int i = 0; i < mesh->mNumBones; i++) {
            auto global = globals.find(mesh->mBones[i]->mName.C_Str());
            if (global != globals.end()) {
                mesh->mBones[i]->mOffsetMatrix = aiMatrix4x4(global.value()).Inverse() * meshTransforms[meshIndex];
            }
        }
    }
    return added;
}

const QHash<QString, QString>& secondLifeJointNames() {
    static const QHash<QString, QString> names = [] {
        QHash<QString, QString> result {
            { "mPelvis", "Hips" }, { "mTorso", "Spine" }, { "mChest", "Spine2" }, { "mNeck", "Neck" }, { "mHead", "Head" },
            { "mEyeLeft", "LeftEye" }, { "mEyeRight", "RightEye" },
        };
        for (const QString& side : { QString("Left"), QString("Right") }) {
            result.insert("mCollar" + side, side + "Shoulder");
            result.insert("mShoulder" + side, side + "Arm");
            result.insert("mElbow" + side, side + "ForeArm");
            result.insert("mWrist" + side, side + "Hand");
            result.insert("mHip" + side, side + "UpLeg");
            result.insert("mKnee" + side, side + "Leg");
            result.insert("mAnkle" + side, side + "Foot");
            result.insert("mFoot" + side, side + "ToeBase");
            result.insert("mToe" + side, side + "Toe_End");
            for (const QString& finger : { QString("Thumb"), QString("Index"), QString("Middle"), QString("Ring"), QString("Pinky") }) {
                for (int n = 1; n <= 3; n++) {
                    result.insert("mHand" + finger + QString::number(n) + side, side + "Hand" + finger + QString::number(n));
                }
            }
        }
        return result;
    }();
    return names;
}
