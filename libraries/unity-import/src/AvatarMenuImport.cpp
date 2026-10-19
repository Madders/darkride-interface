//
//  AvatarMenuImport.cpp
//  libraries/unity-import/src
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "AvatarMenuImport.h"

#include <functional>

#include <QJsonArray>
#include <QJsonDocument>

#include "UnityIds.h"
#include "UnityPackage.h"
#include "UnityTransforms.h"
#include "UnityYaml.h"

namespace avatarimport {

namespace {

// The built-in parameters, as they stand for someone standing still on a desktop.
const QHash<QString, double>& builtinDefaults() {
    static const QHash<QString, double> DEFAULTS {
        { "IsLocal", 1 }, { "Grounded", 1 }, { "Upright", 1 }, { "TrackingType", 1 }, { "AvatarVersion", 3 },
        { "IsAnimatorEnabled", 1 }, { "GestureLeft", 0 }, { "GestureRight", 0 }, { "GestureLeftWeight", 0 },
        { "GestureRightWeight", 0 }, { "Viseme", 0 }, { "Voice", 0 }, { "AFK", 0 }, { "Seated", 0 }, { "InStation", 0 },
        { "VRMode", 0 }, { "MuteSelf", 0 }, { "Earmuffs", 0 }, { "AngularY", 0 }, { "VelocityX", 0 }, { "VelocityY", 0 },
        { "VelocityZ", 0 }, { "VelocityMagnitude", 0 }, { "ScaleModified", 0 }, { "ScaleFactor", 1 },
        { "ScaleFactorInverse", 1 }, { "EyeHeightAsMeters", 1 }, { "EyeHeightAsPercent", 0.2 }, { "IsOnFriendsList", 0 },
    };
    return DEFAULTS;
}

const unity::YamlObject* firstObject(const unity::Package& package, const QString& guid, int classID) {
    for (const auto& object : package.objects(guid)) {
        if (object.classID == classID && !object.stripped) {
            return &object;
        }
    }
    return nullptr;
}

QString conditionMode(int mode) {
    switch (mode) {
        case 1: return "if";
        case 2: return "ifNot";
        case 3: return "greater";
        case 4: return "less";
        case 6: return "equals";
        case 7: return "notEqual";
        default: return "unknown";
    }
}

QJsonArray numbers(std::initializer_list<float> values) {
    QJsonArray array;
    for (float v : values) {
        array.append(QString::number(v, 'g', 7).toDouble());
    }
    return array;
}

}  // namespace

AvatarMenuData avatarMenuData(const unity::Package& package, const QVariantMap& descriptor, const AvatarMenuInputs& in) {
    AvatarMenuData data;
    const hfm::Model& model = *in.model;
    QString effectsController;
    for (const QVariant& entry : descriptor.value("baseAnimationLayers").toList()) {
        const QVariantMap layer = entry.toMap();
        if ((int)unity::toNumber(layer.value("type")) == 5 && unity::toNumber(layer.value("isDefault")) == 0.0) {
            effectsController = unity::guidOf(layer.value("animatorController"));
        }
    }
    const unity::YamlObject* controller = effectsController.isEmpty() ? nullptr : firstObject(package, effectsController, 91);
    const QString menuGuid = unity::guidOf(descriptor.value("expressionsMenu"));
    const QString parametersGuid = unity::guidOf(descriptor.value("expressionParameters"));
    if (!controller && menuGuid.isEmpty()) {
        return data;
    }

    // Parameters: the menu's parameters, then what else the effects layer reads (built-in ones, or the layer's own).
    QJsonArray parameters;
    QSet<QString> named;
    if (const unity::YamlObject* asset = parametersGuid.isEmpty() ? nullptr : firstObject(package, parametersGuid, 114)) {
        for (const QVariant& entry : asset->fields.value("parameters").toList()) {
            const QVariantMap p = entry.toMap();
            const QString name = p.value("name").toString();
            if (name.isEmpty() || named.contains(name)) {
                continue;
            }
            named.insert(name);
            const int type = (int)unity::toNumber(p.value("valueType"));
            parameters.append(QJsonObject {
                { "name", name },
                { "type", type == 0 ? "int" : type == 1 ? "float" : "bool" },
                { "default", unity::toNumber(p.value("defaultValue")) },
                { "saved", unity::toNumber(p.value("saved"), 1.0) != 0.0 },
                { "synced", unity::toNumber(p.value("networkSynced"), 1.0) != 0.0 },
                { "builtin", false },
            });
        }
    }
    if (controller) {
        for (const QVariant& entry : controller->fields.value("m_AnimatorParameters").toList()) {
            const QVariantMap p = entry.toMap();
            const QString name = p.value("m_Name").toString();
            if (name.isEmpty() || named.contains(name)) {
                continue;
            }
            named.insert(name);
            const int type = (int)unity::toNumber(p.value("m_Type"));
            const bool builtin = builtinDefaults().contains(name);
            const double value = builtin ? builtinDefaults().value(name)
                : type == 1 ? unity::toNumber(p.value("m_DefaultFloat"))
                : type == 3 ? unity::toNumber(p.value("m_DefaultInt"))
                : unity::toNumber(p.value("m_DefaultBool"));
            parameters.append(QJsonObject {
                { "name", name },
                { "type", type == 1 ? "float" : type == 3 ? "int" : "bool" },
                { "default", value },
                { "saved", false },
                { "synced", false },
                { "builtin", builtin },
            });
        }
    }

    // The menu, with its icons copied beside the .fst.
    std::function<QJsonArray(const QString&, int)> menuControls = [&](const QString& guid, int depth) {
        QJsonArray controls;
        const unity::YamlObject* menu = depth > 8 || guid.isEmpty() ? nullptr : firstObject(package, guid, 114);
        if (!menu) {
            return controls;
        }
        for (const QVariant& entry : menu->fields.value("controls").toList()) {
            const QVariantMap c = entry.toMap();
            const int type = (int)unity::toNumber(c.value("type"));
            QJsonObject control {
                { "name", c.value("name").toString() },
                { "type", type == 101 ? "button" : type == 102 ? "toggle" : type == 103 ? "submenu"
                        : type == 201 ? "twoAxis" : type == 202 ? "fourAxis" : type == 203 ? "radial" : "unknown" },
                { "parameter", c.value("parameter").toMap().value("name").toString() },
                { "value", unity::toNumber(c.value("value"), 1.0) },
            };
            const QString icon = in.copyIcon ? in.copyIcon(unity::guidOf(c.value("icon"))) : QString();
            if (!icon.isEmpty()) {
                control["icon"] = icon;
            }
            QJsonArray sub;
            for (const QVariant& s : c.value("subParameters").toList()) {
                const QString name = s.toMap().value("name").toString();
                if (!name.isEmpty()) {
                    sub.append(name);
                }
            }
            if (!sub.isEmpty()) {
                control["subParameters"] = sub;
            }
            if (type == 103) {
                control["controls"] = menuControls(unity::guidOf(c.value("subMenu")), depth + 1);
            }
            controls.append(control);
            data.controls++;
        }
        return controls;
    };
    const QJsonObject menu { { "controls", menuControls(menuGuid, 0) } };

    // The layers. Objects the clips switch on and off are gathered as they come.
    QJsonArray objects;
    QHash<QString, int> objectIndex;
    auto objectOf = [&](const QString& path) {
        auto found = objectIndex.find(path);
        if (found != objectIndex.end()) {
            return found.value();
        }
        const int joint = in.paths.indexOf(path);
        QJsonArray parts;
        if (joint >= 0) {
            for (int i = 0; i < (int)model.shapes.size(); i++) {
                for (int j = (int)model.shapes[i].joint, hops = 0; j >= 0 && j < (int)model.joints.size() && hops < 256;
                     j = model.joints[j].parentIndex, hops++) {
                    if (j == joint) {
                        parts.append(i);
                        data.controlledParts.insert(i);
                        break;
                    }
                }
            }
        }
        const int index = objects.size();
        objects.append(QJsonObject {
            { "path", path },
            { "parts", parts },
            { "active", joint >= 0 && in.prefabActive ? in.prefabActive(joint) : true },
        });
        objectIndex.insert(path, index);
        return index;
    };
    auto skeletonName = [&](int joint) {
        const QString name = model.joints[joint].name;
        return in.skeletonNames.value(name, name);
    };
    QJsonObject baseBlendshapes, baseJoints;
    QSet<QString> unknownPaths;
    int materialCurves = 0, otherCurves = 0;
    QHash<QString, QJsonObject> effects;  // by clip GUID
    auto effectOf = [&](const QString& clipGuid) {
        auto found = effects.find(clipGuid);
        if (found != effects.end()) {
            return found.value();
        }
        QJsonObject effect;
        const unity::YamlObject* clip = clipGuid.isEmpty() ? nullptr : firstObject(package, clipGuid, 74);
        if (clip) {
            QJsonArray active;
            QJsonObject blendshapes, joints;
            for (const QVariant& entry : clip->fields.value("m_FloatCurves").toList()) {
                const QVariantMap curve = entry.toMap();
                const QVariantList keys = curve.value("curve").toMap().value("m_Curve").toList();
                if (keys.isEmpty()) {
                    continue;
                }
                const QString attribute = curve.value("attribute").toString();
                const QString path = curve.value("path").toString();
                const double value = unity::toNumber(keys.first().toMap().value("value"));
                if (attribute == "m_IsActive") {
                    active.append(QJsonArray { objectOf(path), value > 0.5 });
                } else if (attribute.startsWith("blendShape.")) {
                    const QString name = attribute.mid(11);
                    blendshapes[name] = qBound(0.0, value / 100.0, 1.0);
                    if (!baseBlendshapes.contains(name)) {
                        baseBlendshapes[name] = in.prefabBlendshapes.value(name, 0.0f);
                    }
                } else if (attribute.startsWith("material.")) {
                    materialCurves++;
                } else {
                    otherCurves++;
                }
            }
            auto jointCurves = [&](const char* key, const char* part) {
                for (const QVariant& entry : clip->fields.value(key).toList()) {
                    const QVariantMap curve = entry.toMap();
                    const QVariantList keys = curve.value("curve").toMap().value("m_Curve").toList();
                    if (keys.isEmpty()) {
                        continue;
                    }
                    const QString path = curve.value("path").toString();
                    const int joint = in.paths.indexOf(path);
                    if (joint < 0) {
                        unknownPaths.insert(path);
                        continue;
                    }
                    const QVariantMap v = keys.first().toMap().value("value").toMap();
                    const glm::vec3 xyz((float)unity::toNumber(v.value("x")), (float)unity::toNumber(v.value("y")),
                                        (float)unity::toNumber(v.value("z")));
                    QJsonObject pose = joints.value(skeletonName(joint)).toObject();
                    if (QString(part) == "translation") {
                        const glm::vec3 t = unity::positionFromUnity(xyz, in.metersPerUnit);
                        pose["translation"] = numbers({ t.x, t.y, t.z });
                    } else if (QString(part) == "scale") {
                        pose["scale"] = numbers({ xyz.x, xyz.y, xyz.z });
                    } else if (QString(part) == "euler") {
                        const glm::quat q = unity::rotationFromUnityEuler(xyz);
                        pose["rotation"] = numbers({ q.x, q.y, q.z, q.w });
                    } else {
                        const glm::quat q = unity::rotationFromUnity(glm::normalize(glm::quat(
                            (float)unity::toNumber(v.value("w"), 1.0), xyz.x, xyz.y, xyz.z)));
                        pose["rotation"] = numbers({ q.x, q.y, q.z, q.w });
                    }
                    joints[skeletonName(joint)] = pose;
                    if (!baseJoints.contains(skeletonName(joint)) && in.prefabPose) {
                        baseJoints[skeletonName(joint)] = in.prefabPose(joint);
                    }
                }
            };
            jointCurves("m_PositionCurves", "translation");
            jointCurves("m_ScaleCurves", "scale");
            jointCurves("m_EulerCurves", "euler");
            jointCurves("m_RotationCurves", "rotation");
            if (!clip->fields.value("m_PPtrCurves").toList().isEmpty()) {
                materialCurves++;
            }
            if (!active.isEmpty()) {
                effect["active"] = active;
            }
            if (!blendshapes.isEmpty()) {
                effect["blendshapes"] = blendshapes;
            }
            if (!joints.isEmpty()) {
                effect["joints"] = joints;
            }
        }
        effects.insert(clipGuid, effect);
        return effect;
    };

    QJsonArray layers;
    if (controller) {
        QHash<qint64, const unity::YamlObject*> byID;
        for (const auto& object : package.objects(effectsController)) {
            byID.insert(object.fileID, &object);
        }
        // a state's motion: its clip, or a blend tree's first clip
        auto clipOf = [&](const QVariant& motion) {
            QVariant m = motion;
            for (int depth = 0; depth < 4; depth++) {
                const QString guid = unity::guidOf(m);
                if (!guid.isEmpty()) {
                    return guid;
                }
                const unity::YamlObject* tree = byID.value(unity::fileIDOf(m));
                if (!tree || tree->classID != 206) {
                    break;
                }
                const QVariantList children = tree->fields.value("m_Childs").toList();
                m = children.isEmpty() ? QVariant() : children.first().toMap().value("m_Motion");
            }
            return QString();
        };
        int subMachines = 0;
        for (const QVariant& layerEntry : controller->fields.value("m_AnimatorLayers").toList()) {
            const QVariantMap layer = layerEntry.toMap();
            const unity::YamlObject* machine = byID.value(unity::fileIDOf(layer.value("m_StateMachine")));
            if (!machine) {
                continue;
            }
            subMachines += machine->fields.value("m_ChildStateMachines").toList().size();
            QVector<qint64> stateIDs;
            for (const QVariant& child : machine->fields.value("m_ChildStates").toList()) {
                stateIDs.append(unity::fileIDOf(child.toMap().value("m_State")));
            }
            auto transitionsOf = [&](const QVariantList& references) {
                QJsonArray transitions;
                for (const QVariant& reference : references) {
                    const unity::YamlObject* t = byID.value(unity::fileIDOf(reference));
                    if (!t) {
                        continue;
                    }
                    const qint64 destination = unity::fileIDOf(t->fields.value("m_DstState"));
                    const int to = destination ? stateIDs.indexOf(destination) : -1;
                    if (destination && to < 0) {
                        continue;  // into a sub-state machine: not yet
                    }
                    QJsonArray conditions;
                    for (const QVariant& c : t->fields.value("m_Conditions").toList()) {
                        const QVariantMap condition = c.toMap();
                        conditions.append(QJsonArray { condition.value("m_ConditionEvent").toString(),
                                                       conditionMode((int)unity::toNumber(condition.value("m_ConditionMode"))),
                                                       unity::toNumber(condition.value("m_EventTreshold")) });
                    }
                    const bool exitTime = unity::toNumber(t->fields.value("m_HasExitTime")) != 0.0;
                    transitions.append(QJsonObject {
                        { "to", to },
                        { "conditions", conditions },
                        { "exitTime", exitTime ? QJsonValue(unity::toNumber(t->fields.value("m_ExitTime"), 1.0)) : QJsonValue() },
                        // seconds; a duration relative to the clip counts as none, the effects layer's clips being a
                        // frame or two long
                        { "duration", unity::toNumber(t->fields.value("m_HasFixedDuration"), 1.0) != 0.0
                                            ? unity::toNumber(t->fields.value("m_TransitionDuration")) : 0.0 },
                    });
                }
                return transitions;
            };
            // what a state sets on entering: its parameter drivers
            auto drivesOf = [&](const unity::YamlObject* state) {
                static const char* TYPES[] = { "set", "add", "random", "copy" };
                QJsonArray drives;
                for (const QVariant& reference : state->fields.value("m_StateMachineBehaviours").toList()) {
                    const unity::YamlObject* behaviour = byID.value(unity::fileIDOf(reference));
                    if (!behaviour || unity::componentClassName(unity::fileIDOf(behaviour->fields.value("m_Script"))) !=
                                          QLatin1String("VRCAvatarParameterDriver")) {
                        continue;
                    }
                    const bool localOnly = unity::toNumber(behaviour->fields.value("localOnly")) != 0.0;
                    for (const QVariant& entry : behaviour->fields.value("parameters").toList()) {
                        const QVariantMap p = entry.toMap();
                        const int type = (int)unity::toNumber(p.value("type"));
                        drives.append(QJsonObject {
                            { "type", type >= 0 && type < 4 ? TYPES[type] : "set" },
                            { "name", p.value("name").toString() },
                            { "value", unity::toNumber(p.value("value")) },
                            { "min", unity::toNumber(p.value("valueMin")) },
                            { "max", unity::toNumber(p.value("valueMax")) },
                            { "chance", unity::toNumber(p.value("chance")) },
                            { "source", p.value("source").toString() },
                            { "convertRange", unity::toNumber(p.value("convertRange")) != 0.0 },
                            { "sourceMin", unity::toNumber(p.value("sourceMin")) },
                            { "sourceMax", unity::toNumber(p.value("sourceMax")) },
                            { "destMin", unity::toNumber(p.value("destMin")) },
                            { "destMax", unity::toNumber(p.value("destMax")) },
                            { "localOnly", localOnly },
                        });
                    }
                }
                return drives;
            };
            QJsonArray states;
            for (qint64 id : stateIDs) {
                const unity::YamlObject* state = byID.value(id);
                if (!state) {
                    states.append(QJsonObject { { "name", "" }, { "effect", QJsonObject() }, { "transitions", QJsonArray() } });
                    continue;
                }
                states.append(QJsonObject {
                    { "name", state->fields.value("m_Name").toString() },
                    { "writeDefaults", unity::toNumber(state->fields.value("m_WriteDefaultValues"), 1.0) != 0.0 },
                    { "effect", effectOf(clipOf(state->fields.value("m_Motion"))) },
                    { "transitions", transitionsOf(state->fields.value("m_Transitions").toList()) },
                    { "drive", drivesOf(state) },
                });
                data.states++;
            }
            layers.append(QJsonObject {
                { "name", layer.value("m_Name").toString() },
                { "weight", unity::toNumber(layer.value("m_DefaultWeight"), 1.0) },
                { "default", stateIDs.indexOf(unity::fileIDOf(machine->fields.value("m_DefaultState"))) },
                { "entry", transitionsOf(machine->fields.value("m_EntryTransitions").toList()) },
                { "anyState", transitionsOf(machine->fields.value("m_AnyStateTransitions").toList()) },
                { "states", states },
            });
            data.layers++;
        }
        if (subMachines > 0) {
            data.notes << QString("not yet: %1 sub-state machines in the effects layer").arg(subMachines);
        }
    }
    if (materialCurves > 0) {
        data.notes << QString("not yet: %1 effects clips that animate materials").arg(materialCurves);
    }
    if (!unknownPaths.isEmpty()) {
        data.notes << QString("the effects layer moves %1 objects the model does not have").arg(unknownPaths.size());
    }
    Q_UNUSED(otherCurves);

    // Emotes: the emote layer's states that a whole-number parameter set from the menu picks, one per value, standing
    // and seated apart. Their clips are often stock ones the package doesn't carry; the engine's own emote animations
    // stand in for those, by name.
    QString actionController;
    for (const QVariant& entry : descriptor.value("baseAnimationLayers").toList()) {
        const QVariantMap layer = entry.toMap();
        if ((int)unity::toNumber(layer.value("type")) == 4 && unity::toNumber(layer.value("isDefault")) == 0.0) {
            actionController = unity::guidOf(layer.value("animatorController"));
        }
    }
    QJsonArray emotes;
    if (!actionController.isEmpty()) {
        QSet<QString> menuParameters;
        std::function<void(const QJsonArray&)> collect = [&](const QJsonArray& controls) {
            for (const QJsonValue& value : controls) {
                menuParameters.insert(value.toObject().value("parameter").toString());
                collect(value.toObject().value("controls").toArray());
            }
        };
        collect(menu.value("controls").toArray());
        QHash<qint64, const unity::YamlObject*> byID;
        QHash<qint64, QString> leaving;  // transition -> the state it leaves
        for (const auto& object : package.objects(actionController)) {
            byID.insert(object.fileID, &object);
            if (object.classID == 1102) {
                for (const QVariant& transition : object.fields.value("m_Transitions").toList()) {
                    leaving.insert(unity::fileIDOf(transition), object.fields.value("m_Name").toString());
                }
            }
        }
        std::vector<QJsonObject> found;
        QSet<QString> seen;
        int own = 0;
        for (const auto& object : package.objects(actionController)) {
            const unity::YamlObject* state = object.classID == 1101 ? byID.value(unity::fileIDOf(object.fields.value("m_DstState"))) : nullptr;
            if (!state || state->classID != 1102) {
                continue;
            }
            for (const QVariant& c : object.fields.value("m_Conditions").toList()) {
                const QVariantMap condition = c.toMap();
                const QString parameter = condition.value("m_ConditionEvent").toString();
                if ((int)unity::toNumber(condition.value("m_ConditionMode")) != 6 || !menuParameters.contains(parameter)) {
                    continue;  // only "equals" on a parameter the menu sets
                }
                const int value = (int)unity::toNumber(condition.value("m_EventTreshold"));
                const QString name = state->fields.value("m_Name").toString();
                const bool seated = name.contains("seated", Qt::CaseInsensitive) ||
                                    leaving.value(object.fileID).contains("sit", Qt::CaseInsensitive);
                const QString key = QString("%1/%2/%3").arg(parameter).arg(value).arg(seated);
                if (seen.contains(key)) {
                    continue;
                }
                seen.insert(key);
                const QString clipGuid = unity::guidOf(state->fields.value("m_Motion"));
                const unity::YamlObject* clip = clipGuid.isEmpty() ? nullptr : firstObject(package, clipGuid, 74);
                bool loop = name.contains("loop", Qt::CaseInsensitive);
                if (clip) {
                    loop = unity::toNumber(clip->fields.value("m_AnimationClipSettings").toMap().value("m_LoopTime")) != 0.0;
                    own++;
                }
                found.push_back(QJsonObject {
                    { "parameter", parameter }, { "value", value }, { "name", name }, { "seated", seated }, { "loop", loop },
                    { "hold", name.contains("hold", Qt::CaseInsensitive) }, { "own", clip != nullptr },
                });
            }
        }
        std::sort(found.begin(), found.end(), [](const QJsonObject& a, const QJsonObject& b) {
            return a.value("seated").toBool() != b.value("seated").toBool() ? !a.value("seated").toBool()
                                                                            : a.value("value").toInt() < b.value("value").toInt();
        });
        for (const QJsonObject& emote : found) {
            emotes.append(emote);
        }
        if (!found.empty()) {
            data.notes << QString("emotes: %1 in the emote layer; %2 use the avatar's own animations (not played yet), the "
                                  "rest the engine's own where it has one").arg(found.size()).arg(own);
        }
    }

    data.json = QJsonObject {
        { "version", 1 },
        { "parameters", parameters },
        { "menu", menu },
        { "objects", objects },
        { "layers", layers },
        { "emotes", emotes },
        { "contacts", in.contacts },
        { "base", QJsonObject { { "blendshapes", baseBlendshapes }, { "joints", baseJoints } } },
    };
    return data;
}

}  // namespace avatarimport
