//
//  AvatarImporter.cpp
//  interface/src/avatar
//
//  Created on 2026-10-12 for Darkride's avatar rigger (AUTO-RIGGING.md in the project root).
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AvatarImporter.h"

#include <memory>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegExp>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <AssimpSerializer.h>
#include <AutoRig.h>
#include <AvatarWriter.h>
#include <FBXSerializer.h>
#include <GLMHelpers.h>
#include <GLTFSerializer.h>
#include <HumanoidJointMap.h>
#include <NetworkAccessManager.h>
#include <OBJSerializer.h>
#include <PathUtils.h>
#include <VRMReader.h>

#include "Application.h"
#include "AvatarManager.h"
#include "InterfaceLogging.h"
#include "MyAvatar.h"
#include "editor/EditorCreate.h"

namespace {

const char* TITLE = "Import Avatar";
const float USUAL_HEIGHT = 1.70f;

// The joints a rig needs before Darkride's animations can drive it. Not Spine1: Second Life's and some VRM skeletons
// have two spine joints, and the animations drive them well enough.
const char* const NEEDED_JOINTS[] = { "Hips", "Spine", "Neck", "Head",
                                      "LeftArm", "LeftForeArm", "LeftHand", "RightArm", "RightForeArm", "RightHand",
                                      "LeftUpLeg", "LeftLeg", "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot" };

enum class RigKind { None, Standard, Other };

struct Import {
    QString path;
    QString name;
    HFMModel::Pointer model;
    autorig::Body body;
    RigKind rig { RigKind::None };
    QString why;  // for RigKind::Other: what keeps the animations from driving it
    VRMInfo vrm;
};
using ImportPointer = std::shared_ptr<Import>;

HFMModel::Pointer readModel(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }
    const QByteArray data = file.readAll();
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    const QUrl url = QUrl::fromLocalFile(info.absoluteFilePath());
    const QVariantHash mapping;
    if (suffix == "fbx") {
        return FBXSerializer().read(data, mapping, url);
    }
    if (suffix == "gltf" || suffix == "glb") {
        return GLTFSerializer().read(data, mapping, url);
    }
    if (suffix == "obj") {
        return OBJSerializer().read(data, mapping, url);
    }
    return AssimpSerializer().read(data, mapping, url);
}

// None: no skin. Standard: the needed joints, by their names or by the names Darkride gives them when it's worn (a
// VRM's humanoid bones, else HumanoidJointMap), so Darkride's animations drive it; joints whose axes aren't Mixamo's
// get offsets when it's worn (HumanoidRotationOffsets). Other: a skeleton in which the needed joints can't be found.
RigKind classify(const HFMModel& model, const VRMInfo& vrm, QString& why) {
    if (model.skinDeformers.empty()) {
        return RigKind::None;
    }
    QStringList names = model.getJointNames();
    for (QString& name : names) {
        name = vrm.jointNames.value(name, name);
    }
    if (!hasMainHumanoidJoints(names)) {
        std::vector<HumanoidJointInfo> joints;
        for (int i = 0; i < (int)model.joints.size(); i++) {
            const HFMJoint& joint = model.joints[i];
            joints.push_back({ names[i], joint.parentIndex, joint.isSkeletonJoint, extractTranslation(joint.globalTransform) });
        }
        const QHash<int, QString> humanoidNames = mapHumanoidJoints(joints);
        for (auto itr = humanoidNames.begin(); itr != humanoidNames.end(); itr++) {
            names[itr.key()] = itr.value();
        }
    }
    for (const char* name : NEEDED_JOINTS) {
        if (!names.contains(name)) {
            why = QString("Darkride can't tell which of its joints is the %1").arg(name);
            return RigKind::Other;
        }
    }
    return RigKind::Standard;
}

// A VRM's maker and terms, to show with the questions about it
QString credit(const VRMInfo& vrm, const QString& name) {
    if (!vrm.isVRM()) {
        return QString();
    }
    return QString("\n\n%1 is by %2. Its licence: %3.").arg(vrm.title.isEmpty() ? name : vrm.title,
        vrm.authors.isEmpty() ? QString("an unnamed author") : vrm.authors, vrm.licence);
}

QString avatarName(const QString& path) {
    QString name = QFileInfo(path).completeBaseName();
    name.replace(QRegExp("[^A-Za-z0-9_-]+"), "_");
    return name.isEmpty() ? QString("avatar") : name;
}

// A new folder for each import (name, name-2, name-3...), so the client never shows a cached copy of an earlier one.
QString avatarFolder(const QString& name) {
    const QString base = PathUtils::getAppDataPath() + "Avatars/" + name;
    QString folder = base;
    for (int n = 2; QDir(folder).exists(); n++) {
        folder = base + "-" + QString::number(n);
    }
    return folder;
}

struct Choice {
    QString label;
    QMessageBox::ButtonRole role;  // AcceptRole is the default button, RejectRole the one Escape presses
};

// One of the importer's questions: a native dialog with buttons named for what they do (the client's own dialogs
// only offer Yes, No and Cancel). Returns the chosen button's place in the list, or -1.
int ask(const QString& text, const QString& details, const std::vector<Choice>& choices) {
    QMessageBox box;
    box.setWindowTitle(TITLE);
    box.setText(text);
    box.setInformativeText(details);
    std::vector<QAbstractButton*> buttons;
    for (const Choice& choice : choices) {
        QPushButton* button = box.addButton(choice.label, choice.role);
        buttons.push_back(button);
        if (choice.role == QMessageBox::AcceptRole) {
            box.setDefaultButton(button);
        } else if (choice.role == QMessageBox::RejectRole) {
            box.setEscapeButton(button);
        }
    }
    qCDebug(interfaceapp) << "Import Avatar: asking" << text;
    box.exec();
    auto chosen = std::find(buttons.begin(), buttons.end(), box.clickedButton());
    const int answer = chosen == buttons.end() ? -1 : (int)(chosen - buttons.begin());
    qCDebug(interfaceapp) << "Import Avatar: answered" << (answer < 0 ? QString("nothing") : choices[answer].label);
    return answer;
}

void warn(const QString& text) {
    qCDebug(interfaceapp) << "Import Avatar: warning" << text;
    QMessageBox::warning(nullptr, TITLE, text);
}

void inform(const QString& text) {
    qCDebug(interfaceapp) << "Import Avatar: telling" << text;
    QMessageBox::information(nullptr, TITLE, text);
}

// A line before and after each step, so a client that stops here shows in its log the step it stopped in (a freeze
// right after "Wear it", 10-13, left no trace).
void wearUrl(const QString& url, const QString& name) {
    qCDebug(interfaceapp) << "Import Avatar: putting on" << url;
    DependencyManager::get<AvatarManager>()->getMyAvatar()->useFullAvatarURL(url, name);
    qCDebug(interfaceapp) << "Import Avatar: avatar change started";
    emit qApp->fullAvatarURLChanged(url, name);
    qCDebug(interfaceapp) << "Import Avatar: wear done";
}

void wear(const QString& fstPath, const QString& name) {
    wearUrl(QUrl::fromLocalFile(fstPath).toString(), name);
}

// An avatar on this computer goes out to others as a blank address (AvatarData::getWireSafeSkeletonModelURL), so they
// see the default. In My Files it has a web address everyone loads: the model goes up with the files it names beside
// it (game rips name their textures; anything else in the folder stays here), then the .fst with its filename line
// pointing beside it, and the client wears that copy. One file at a time, through the editor's upload.
struct Share {
    QString name;
    QString fstName;
    QString remoteFolder;
    QStringList fstLines;
    int modelLine { -1 };
    bool modelIsLocal { false };
    QStringList files;  // local paths, the model first
    int next { 0 };
};
using SharePointer = std::shared_ptr<Share>;

// The web copy is worn only once its .fst reads back from the web: a failed avatar load falls back to the default
// without a word, and the files host sent every .fst labelled gzip over plain text until metaverse 19f3c7d (10-13).
void wearOnceItReadsBack(const SharePointer& share, const QString& url) {
    QNetworkRequest request { QUrl(url) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = NetworkAccessManager::getInstance().get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, share, url]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || !body.contains("filename")) {
            const QString why = reply->error() != QNetworkReply::NoError ? reply->errorString()
                                                                         : QString("it came back different");
            warn(share->name + " went up to My Files, but its avatar file does not read back from the web (" + why +
                 "), so you are still wearing the copy on this computer.");
            return;
        }
        qCDebug(interfaceapp) << "Import Avatar:" << share->name << "reads back from" << url;
        wearUrl(url, share->name);
        inform(share->name + " is in My Files now, and you are wearing that copy, so everyone sees it.");
    });
}

void shareNext(const SharePointer& share) {
    const bool fstNow = share->next >= share->files.size();
    auto upload = new EditorUpload();
    QString error;
    bool started;
    if (!fstNow) {
        started = upload->start(share->files[share->next], share->remoteFolder, error);
        if (started && share->next == 0 && share->modelIsLocal) {
            share->fstLines[share->modelLine] = "filename = " + upload->fileName();
        }
        share->next++;
    } else {
        started = upload->startData(share->fstLines.join('\n').toUtf8(), share->fstName, share->remoteFolder,
                                    "text/plain", error);
    }
    if (!started) {
        delete upload;
        warn("Could not put " + share->name + " in My Files: " + error + ".");
        return;
    }
    QObject::connect(upload, &EditorUpload::failed, upload, [upload, share](const QString& error) {
        upload->deleteLater();
        warn("Could not put " + share->name + " in My Files: " + error + ".");
    });
    QObject::connect(upload, &EditorUpload::finished, upload, [upload, share, fstNow](const QString& url) {
        upload->deleteLater();
        if (!fstNow) {
            shareNext(share);
            return;
        }
        qCDebug(interfaceapp) << "Import Avatar:" << share->name << "is in My Files at" << url;
        wearOnceItReadsBack(share, url);
    });
}

void shareAvatar(const QString& fstPath, const QString& name) {
    QFile fst(fstPath);
    if (!fst.open(QIODevice::ReadOnly)) {
        warn("Could not read " + fstPath + ".");
        return;
    }
    auto share = std::make_shared<Share>();
    share->name = name;
    share->fstName = QFileInfo(fstPath).fileName();
    share->remoteFolder = "/avatars/" + name + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QString modelPath;
    VRMInfo vrm;
    for (const QString& line : QString::fromUtf8(fst.readAll()).split('\n')) {
        const QString key = line.section('=', 0, 0).trimmed();
        if (key == "texdir") {
            continue;  // the textures go up beside the model, where it looks for them anyway
        }
        if (key == "filename" && share->modelLine < 0) {
            const QString value = line.section('=', 1).trimmed();
            const QUrl url(value);
            if (url.isLocalFile()) {
                modelPath = url.toLocalFile();
            } else if (url.isRelative()) {
                modelPath = QFileInfo(fstPath).dir().absoluteFilePath(value);
            }
            share->modelLine = share->fstLines.size();
        }
        share->fstLines.append(line);
    }
    if (!modelPath.isEmpty()) {
        QFile model(modelPath);
        if (!model.open(QIODevice::ReadOnly)) {
            warn("Could not read " + modelPath + ".");
            return;
        }
        const QByteArray bytes = model.readAll();
        vrm = readVRM(bytes);
        const QFileInfo modelInfo(modelPath);
        share->files.append(modelInfo.absoluteFilePath());
        share->modelIsLocal = true;
        for (const QFileInfo& file : modelInfo.dir().entryInfoList(QDir::Files)) {
            if (file != modelInfo && file.suffix().toLower() != "fst" && bytes.contains(file.fileName().toUtf8())) {
                share->files.append(file.absoluteFilePath());
            }
        }
    }
    // Everyone can see what's in My Files and download it, so only what the user may share goes there
    QString terms = "Everyone in Darkride will see it, and its files can be downloaded. Only put an avatar there if you "
                    "made it or its licence lets you share it." + credit(vrm, name);
    if (vrm.isVRM() && (vrm.onlyAuthorMayWear || !vrm.redistributionAllowed)) {
        terms += QString("\n\nIts licence says %1, so unless you made it, keep it on this computer.")
                     .arg(vrm.onlyAuthorMayWear && !vrm.redistributionAllowed ? "only its author may wear it and it may not be shared"
                          : vrm.onlyAuthorMayWear ? "only its author may wear it" : "it may not be shared");
    }
    if (ask("Put " + name + " in My Files?", terms,
            { { "I may share it", QMessageBox::AcceptRole }, { "Cancel", QMessageBox::RejectRole } }) != 0) {
        return;
    }
    qCDebug(interfaceapp) << "Import Avatar: putting" << name << "in My Files," << share->files.size()
                          << "files and the .fst, into" << share->remoteFolder;
    shareNext(share);
}

void offerToWear(const QString& fstPath, const QString& name, const QString& what) {
    const int answer = ask(what, "Only you see it while it is on this computer. In My Files, everyone sees it.",
                           { { "Wear it", QMessageBox::AcceptRole },
                             { "Wear it and put it in My Files", QMessageBox::ActionRole },
                             { "Not now", QMessageBox::RejectRole } });
    if (answer == 0 || answer == 1) {
        wear(fstPath, name);
    }
    if (answer == 1) {
        shareAvatar(fstPath, name);
    }
}

// The rigging and saving happen off the main thread; the answer comes back to it.
void autoRig(const ImportPointer& import, float scale) {
    QtConcurrent::run([import, scale]() {
        autorig::Body body = import->body;
        if (scale != 1.0f) {
            body.scale(scale);
        }
        const autorig::Result result = autorig::rig(body);
        autorig::WrittenAvatar written;
        if (result.ok) {
            written = autorig::writeAvatar(*import->model, body, result, import->path, avatarFolder(import->name), import->name);
        }
        for (const QString& note : written.notes) {
            qCDebug(interfaceapp) << "Import Avatar:" << import->name << "note:" << note;
        }
        qCDebug(interfaceapp) << "Import Avatar:" << import->name
                              << (!result.ok ? "not rigged:" : !written.ok ? "rigged, not saved:" : "rigged and saved as")
                              << (!result.ok ? result.error : !written.ok ? written.error : written.fstPath);
        QMetaObject::invokeMethod(qApp, [import, result, written]() {
            if (!result.ok) {
                warn("Could not rig " + import->name + ": " + result.error + ".");
            } else if (!written.ok) {
                warn("Could not save " + import->name + ": " + written.error + ".");
            } else {
                offerToWear(written.fstPath, import->name,
                            import->name + " is rigged with Darkride's standard skeleton. Wear it now?\n\nSaved in " +
                                QFileInfo(written.fstPath).absolutePath() + ".");
            }
        }, Qt::QueuedConnection);
    });
}

void checkHeightThenRig(const ImportPointer& import) {
    const float height = import->body.maximum.y - import->body.minimum.y;
    if (height > 2.4f || height < 0.6f) {
        const int answer = ask(QString("%1 is %2 m tall.").arg(import->name).arg(height, 0, 'f', 2),
                               "That looks like a units mistake in its file.",
                               { { QString("Make it %1 m tall").arg(USUAL_HEIGHT, 0, 'f', 2), QMessageBox::AcceptRole },
                                 { QString("Keep it %1 m tall").arg(height, 0, 'f', 2), QMessageBox::ActionRole },
                                 { "Cancel", QMessageBox::RejectRole } });
        if (answer == 0) {
            autoRig(import, USUAL_HEIGHT / height);
        } else if (answer == 1) {
            autoRig(import, 1.0f);
        }
    } else {
        autoRig(import, 1.0f);
    }
}

void decide(const ImportPointer& import) {
    if (!import->model || import->body.triangles.empty()) {
        warn("Could not read " + QFileInfo(import->path).fileName() + " as a character.");
        return;
    }
    switch (import->rig) {
        case RigKind::Standard: {
            const int answer = ask(import->name + " already has a skeleton Darkride's animations can drive.",
                                   "Keep its own rig, which keeps its face movements too, or replace it with an automatic "
                                   "one. The file itself is not changed." + credit(import->vrm, import->name),
                                   { { "Keep existing rig", QMessageBox::AcceptRole },
                                     { "Replace rig", QMessageBox::ActionRole },
                                     { "Cancel", QMessageBox::RejectRole } });
            if (answer == 0) {
                const QString folder = avatarFolder(import->name);
                QDir().mkpath(folder);
                const QString fstPath = QDir(folder).filePath(import->name + ".fst");
                if (autorig::writeFst(fstPath, import->name, QUrl::fromLocalFile(import->path).toString())) {
                    wear(fstPath, import->name);
                } else {
                    warn("Could not write " + fstPath + ".");
                }
            } else if (answer == 1) {
                checkHeightThenRig(import);
            }
            break;
        }
        case RigKind::Other:
            if (ask(import->name + " has a skeleton of its own, but Darkride's animations can't drive it.",
                    "Because " + import->why + ". Its rig can be replaced with Darkride's standard skeleton; the file "
                    "itself is not changed." + credit(import->vrm, import->name),
                    { { "Replace rig", QMessageBox::AcceptRole }, { "Cancel", QMessageBox::RejectRole } }) == 0) {
                checkHeightThenRig(import);
            }
            break;
        case RigKind::None:
            if (ask(import->name + " has no skeleton yet.", "Darkride can rig it with the standard skeleton." +
                        credit(import->vrm, import->name),
                    { { "Auto rig", QMessageBox::AcceptRole }, { "Cancel", QMessageBox::RejectRole } }) == 0) {
                checkHeightThenRig(import);
            }
            break;
    }
}

}

void AvatarImporter::importFile(const QString& path) {
    const QString name = avatarName(path);
    if (QFileInfo(path).suffix().toLower() == "fst") {
        offerToWear(path, name, "Wear " + name + "?");
        return;
    }
    auto import = std::make_shared<Import>();
    import->path = path;
    import->name = name;
    QtConcurrent::run([import]() {
        const QString suffix = QFileInfo(import->path).suffix().toLower();
        if (suffix == "vrm" || suffix == "glb") {
            QFile file(import->path);
            if (file.open(QIODevice::ReadOnly)) {
                import->vrm = readVRM(file.readAll());
            }
        }
        import->model = readModel(import->path);
        if (import->model) {
            import->body = autorig::gatherBody(*import->model);
            const QString eyelids = autorig::openEyelids(import->body, *import->model);
            if (!eyelids.isEmpty()) {
                qCDebug(interfaceapp) << "Import Avatar:" << eyelids;
            }
            import->rig = classify(*import->model, import->vrm, import->why);
        }
        qCDebug(interfaceapp) << "Import Avatar: read" << import->path << (import->model ? "" : "(not readable)");
        QMetaObject::invokeMethod(qApp, [import]() {
            decide(import);
        }, Qt::QueuedConnection);
    });
}

void AvatarImporter::putInMyFiles(const QUrl& fstUrl) {
    if (!fstUrl.isLocalFile()) {
        inform(fstUrl.scheme() == "qrc" ? QString("The default avatar is in every client already.")
                                         : QString("Your avatar is on the web already, so everyone sees it."));
        return;
    }
    shareAvatar(fstUrl.toLocalFile(), avatarName(fstUrl.toLocalFile()));
}
