//
//  AvatarMenu.cpp
//  libraries/avatars-renderer/src/avatars-renderer
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "AvatarMenu.h"

#include <QJsonArray>
#include <QVariantMap>

#include <AnimSkeleton.h>
#include <SharedUtil.h>

namespace {

int conditionMode(const QString& mode) {
    if (mode == "if") return 1;
    if (mode == "ifNot") return 2;
    if (mode == "greater") return 3;
    if (mode == "less") return 4;
    if (mode == "equals") return 6;
    if (mode == "notEqual") return 7;
    return 0;
}

bool readVector(const QJsonValue& value, int size, float* out) {
    const QJsonArray array = value.toArray();
    if (array.size() != size) {
        return false;
    }
    for (int i = 0; i < size; i++) {
        out[i] = (float)array[i].toDouble();
    }
    return true;
}

AvatarMenu::JointPose readPose(const QJsonObject& json) {
    AvatarMenu::JointPose pose;
    float v[4];
    if ((pose.hasTranslation = readVector(json.value("translation"), 3, v))) {
        pose.translation = glm::vec3(v[0], v[1], v[2]);
    }
    if ((pose.hasRotation = readVector(json.value("rotation"), 4, v))) {
        pose.rotation = glm::normalize(glm::quat(v[3], v[0], v[1], v[2]));
    }
    if ((pose.hasScale = readVector(json.value("scale"), 3, v))) {
        pose.scale = glm::vec3(v[0], v[1], v[2]);
    }
    return pose;
}

// the closest two points of two segments are apart (an end pair alike is a point)
float segmentDistance(const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2, const glm::vec3& q2) {
    const float EPSILON = 1.0e-8f;
    const glm::vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = glm::dot(d1, d1), e = glm::dot(d2, d2), f = glm::dot(d2, r);
    float s = 0.0f, t = 0.0f;
    if (a <= EPSILON && e <= EPSILON) {
        return glm::length(r);
    }
    if (a <= EPSILON) {
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        const float c = glm::dot(d1, r);
        if (e <= EPSILON) {
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            const float b = glm::dot(d1, d2);
            const float denominator = a * e - b * b;
            s = denominator > EPSILON ? glm::clamp((b * f - c * e) / denominator, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    return glm::length((p1 + d1 * s) - (p2 + d2 * t));
}

bool shareTag(const QStringList& a, const QStringList& b) {
    for (const QString& tag : a) {
        if (b.contains(tag, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool samePose(const AvatarMenu::JointPose& a, const AvatarMenu::JointPose& b) {
    return a.hasTranslation == b.hasTranslation && a.hasRotation == b.hasRotation && a.hasScale == b.hasScale &&
        (!a.hasTranslation || a.translation == b.translation) && (!a.hasRotation || a.rotation == b.rotation) &&
        (!a.hasScale || a.scale == b.scale);
}

}  // namespace

void AvatarMenu::clear() {
    std::lock_guard<std::mutex> lock(_mutex);
    _menu = QJsonObject();
    _emotes = QJsonArray();
    _parameters.clear();
    _parameterIndex.clear();
    _layers.clear();
    _objectParts.clear();
    _objectActive.clear();
    _baseObjectActive.clear();
    _shownObjectActive.clear();
    _blendshapes.clear();
    _baseBlendshapes.clear();
    _shownBlendshapes.clear();
    _joints.clear();
    _baseJoints.clear();
    _shownJoints.clear();
    _restJoints.clear();
    _objectFades.clear();
    _blendshapeFades.clear();
    _jointFades.clear();
    _fadeDuration = 0.0f;
    _contacts.clear();
    _changed = true;
    _parametersChanged = false;
}

bool AvatarMenu::isActive() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return !_layers.empty() || !_menu.isEmpty();
}

void AvatarMenu::setup(const QJsonObject& data, const hfm::Model& model, const AnimSkeleton& skeleton) {
    clear();
    if (data.isEmpty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _menu = data.value("menu").toObject();
    _emotes = data.value("emotes").toArray();
    for (const QJsonValue& entry : data.value("parameters").toArray()) {
        const QJsonObject p = entry.toObject();
        Parameter parameter;
        parameter.name = p.value("name").toString();
        parameter.type = p.value("type").toString();
        parameter.defaultValue = parameter.value = (float)p.value("default").toDouble();
        parameter.saved = p.value("saved").toBool();
        parameter.synced = p.value("synced").toBool();
        parameter.builtin = p.value("builtin").toBool();
        if (!parameter.name.isEmpty() && !_parameterIndex.contains(parameter.name)) {
            _parameterIndex.insert(parameter.name, (int)_parameters.size());
            _parameters.push_back(parameter);
        }
    }
    for (const QJsonValue& entry : data.value("objects").toArray()) {
        const QJsonObject object = entry.toObject();
        std::vector<int> parts;
        for (const QJsonValue& part : object.value("parts").toArray()) {
            parts.push_back(part.toInt());
        }
        _objectParts.push_back(parts);
        _baseObjectActive.push_back(object.value("active").toBool(true));
    }
    QHash<QString, int> ownBlendshape;
    for (int i = 0; i < model.ownBlendshapeNames.size(); i++) {
        ownBlendshape.insert(model.ownBlendshapeNames[i], i);
    }
    const QJsonObject base = data.value("base").toObject();
    const QJsonObject baseShapes = base.value("blendshapes").toObject();
    for (auto it = baseShapes.begin(); it != baseShapes.end(); ++it) {
        auto found = ownBlendshape.find(it.key());
        if (found != ownBlendshape.end()) {
            _baseBlendshapes.insert(found.value(), (float)it.value().toDouble());
        }
    }
    const QJsonObject baseJoints = base.value("joints").toObject();
    for (auto it = baseJoints.begin(); it != baseJoints.end(); ++it) {
        const int joint = skeleton.nameToJointIndex(it.key());
        if (joint >= 0) {
            _baseJoints.insert(joint, readPose(it.value().toObject()));
        }
    }

    for (const QJsonValue& entry : data.value("contacts").toArray()) {
        const QJsonObject c = entry.toObject();
        Contact contact;
        contact.receiver = c.value("type").toString() == "receiver";
        contact.joint = skeleton.nameToJointIndex(c.value("joint").toString());
        float v[4];
        if (readVector(c.value("position"), 3, v)) {
            contact.position = glm::vec3(v[0], v[1], v[2]);
        }
        if (readVector(c.value("rotation"), 4, v)) {
            contact.rotation = glm::normalize(glm::quat(v[3], v[0], v[1], v[2]));
        }
        contact.capsule = c.value("shapeType").toInt() == 1;
        contact.radius = (float)c.value("radius").toDouble();
        contact.height = (float)c.value("height").toDouble();
        for (const QJsonValue& tag : c.value("collisionTags").toArray()) {
            contact.tags << tag.toString();
        }
        contact.allowSelf = c.value("allowSelf").toBool(true);
        contact.allowOthers = c.value("allowOthers").toBool(true);
        contact.localOnly = c.value("localOnly").toBool();
        contact.type = c.value("receiverType").toInt();
        contact.parameter = _parameterIndex.value(c.value("parameter").toString(), -1);
        if (contact.joint >= 0 && (!contact.receiver || contact.parameter >= 0)) {
            _contacts.push_back(contact);
        }
    }

    auto readDrives = [&](const QJsonArray& array) {
        std::vector<Drive> drives;
        for (const QJsonValue& entry : array) {
            const QJsonObject d = entry.toObject();
            const QString type = d.value("type").toString();
            Drive drive;
            drive.type = type == "add" ? 1 : type == "random" ? 2 : type == "copy" ? 3 : 0;
            drive.parameter = _parameterIndex.value(d.value("name").toString(), -1);
            drive.source = _parameterIndex.value(d.value("source").toString(), -1);
            drive.value = (float)d.value("value").toDouble();
            drive.min = (float)d.value("min").toDouble();
            drive.max = (float)d.value("max").toDouble();
            drive.chance = (float)d.value("chance").toDouble();
            drive.convertRange = d.value("convertRange").toBool();
            drive.sourceMin = (float)d.value("sourceMin").toDouble();
            drive.sourceMax = (float)d.value("sourceMax").toDouble();
            drive.destMin = (float)d.value("destMin").toDouble();
            drive.destMax = (float)d.value("destMax").toDouble();
            drive.localOnly = d.value("localOnly").toBool();
            if (drive.parameter >= 0) {
                drives.push_back(drive);
            }
        }
        return drives;
    };
    auto readTransitions = [&](const QJsonArray& array) {
        std::vector<Transition> transitions;
        for (const QJsonValue& entry : array) {
            const QJsonObject t = entry.toObject();
            Transition transition;
            transition.to = t.value("to").toInt(-1);
            for (const QJsonValue& c : t.value("conditions").toArray()) {
                const QJsonArray condition = c.toArray();
                Condition parsed;
                parsed.parameter = _parameterIndex.value(condition.at(0).toString(), -1);
                parsed.mode = conditionMode(condition.at(1).toString());
                parsed.threshold = (float)condition.at(2).toDouble();
                transition.conditions.push_back(parsed);
            }
            transition.hasExitTime = t.value("exitTime").isDouble();
            transition.duration = (float)t.value("duration").toDouble();
            transition.exitTime = (float)t.value("exitTime").toDouble();
            if (transition.conditions.empty() && !transition.hasExitTime) {
                continue;  // would fire every frame
            }
            transitions.push_back(transition);
        }
        return transitions;
    };
    for (const QJsonValue& entry : data.value("layers").toArray()) {
        const QJsonObject l = entry.toObject();
        Layer layer;
        if (l.value("weight").toDouble(1.0) <= 0.0) {
            continue;  // a layer at weight 0 shows nothing
        }
        layer.defaultState = l.value("default").toInt(-1);
        layer.entry = readTransitions(l.value("entry").toArray());
        layer.anyState = readTransitions(l.value("anyState").toArray());
        for (const QJsonValue& s : l.value("states").toArray()) {
            const QJsonObject st = s.toObject();
            State state;
            state.writeDefaults = st.value("writeDefaults").toBool();
            state.transitions = readTransitions(st.value("transitions").toArray());
            state.drives = readDrives(st.value("drive").toArray());
            const QJsonObject effect = st.value("effect").toObject();
            for (const QJsonValue& a : effect.value("active").toArray()) {
                const int object = a.toArray().at(0).toInt(-1);
                if (object >= 0 && object < (int)_objectParts.size()) {
                    state.effect.active.emplace_back(object, a.toArray().at(1).toBool());
                    layer.animatedObjects.insert(object);
                }
            }
            const QJsonObject shapes = effect.value("blendshapes").toObject();
            for (auto it = shapes.begin(); it != shapes.end(); ++it) {
                auto found = ownBlendshape.find(it.key());
                if (found != ownBlendshape.end()) {
                    state.effect.blendshapes.emplace_back(found.value(), (float)it.value().toDouble());
                    layer.animatedBlendshapes.insert(found.value());
                    if (!_baseBlendshapes.contains(found.value())) {
                        _baseBlendshapes.insert(found.value(), model.ownBlendshapeDefaults.value(found.value(), 0.0f));
                    }
                }
            }
            const QJsonObject joints = effect.value("joints").toObject();
            for (auto it = joints.begin(); it != joints.end(); ++it) {
                const int joint = skeleton.nameToJointIndex(it.key());
                if (joint >= 0) {
                    state.effect.joints.emplace_back(joint, readPose(it.value().toObject()));
                    layer.animatedJoints.insert(joint);
                }
            }
            layer.states.push_back(state);
        }
        if (layer.defaultState < 0 || layer.defaultState >= (int)layer.states.size()) {
            continue;
        }
        _layers.push_back(layer);
    }

    // Start as the animator starts: the base, then each layer entering its first state, all at once.
    for (const Layer& layer : _layers) {
        for (int joint : layer.animatedJoints) {
            const AnimPose& rest = skeleton.getRelativeDefaultPose(joint);
            JointPose pose;
            pose.translation = rest.trans();
            pose.rotation = rest.rot();
            pose.scale = rest.scale();
            _restJoints.insert(joint, pose);
        }
    }
    _objectActive = _shownObjectActive = _baseObjectActive;
    _blendshapes = _shownBlendshapes = _baseBlendshapes;
    _joints = _shownJoints = _baseJoints;
    for (Layer& layer : _layers) {
        enter(layer, enterFromEntry(layer));
    }
    _parametersChanged = true;
    _changed = true;
}

QJsonObject AvatarMenu::getMenu() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _menu;
}

QJsonArray AvatarMenu::getEmotes() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _emotes;
}

QVariantList AvatarMenu::getParameters() const {
    std::lock_guard<std::mutex> lock(_mutex);
    QVariantList list;
    for (const Parameter& p : _parameters) {
        list.append(QVariantMap { { "name", p.name }, { "type", p.type }, { "value", p.value }, { "default", p.defaultValue },
                                  { "saved", p.saved }, { "synced", p.synced }, { "builtin", p.builtin } });
    }
    return list;
}

bool AvatarMenu::setParameter(const QString& name, float value) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _parameterIndex.find(name);
    if (found == _parameterIndex.end()) {
        return false;
    }
    assign(_parameters[found.value()], value);
    return true;
}

QJsonObject AvatarMenu::getSyncedParameters() const {
    std::lock_guard<std::mutex> lock(_mutex);
    QJsonObject values;
    for (const Parameter& parameter : _parameters) {
        if (isSynced(parameter)) {
            values.insert(parameter.name, parameter.value);
        }
    }
    return values;
}

void AvatarMenu::setSyncedParameters(const QJsonObject& values) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        auto found = _parameterIndex.find(it.key());
        if (found == _parameterIndex.end() || !isSynced(_parameters[found.value()]) || !it.value().isDouble()) {
            continue;  // only what the avatar shares: anything else stays as this client has it
        }
        const float value = (float)it.value().toDouble();
        if (std::isfinite(value)) {
            assign(_parameters[found.value()], value);
        }
    }
}

bool AvatarMenu::isSynced(const Parameter& parameter) {
    // the built-in parameters the avatar's own client sets (gestures.js) and others' need for the faces that answer them
    static const QStringList SYNCED_BUILTINS { "GestureLeft", "GestureRight", "GestureLeftWeight", "GestureRightWeight" };
    return parameter.synced || (parameter.builtin && SYNCED_BUILTINS.contains(parameter.name));
}

void AvatarMenu::assign(Parameter& parameter, float value) {
    if (parameter.type == "bool") {
        value = value != 0.0f ? 1.0f : 0.0f;
    } else if (parameter.type == "int") {
        value = glm::round(value);
        if (!parameter.builtin) {
            value = glm::clamp(value, 0.0f, 255.0f);  // the ranges an avatar's own parameters have
        }
    } else if (!parameter.builtin) {
        value = glm::clamp(value, -1.0f, 1.0f);
    }
    if (parameter.value != value) {
        parameter.value = value;
        _parametersChanged = true;
    }
}

float AvatarMenu::getParameter(const QString& name, float fallback) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto found = _parameterIndex.find(name);
    return found == _parameterIndex.end() ? fallback : _parameters[found.value()].value;
}

bool AvatarMenu::holds(const Transition& transition, float timeInState) const {
    if (transition.hasExitTime && timeInState < transition.exitTime) {
        return false;  // exit times as seconds: a state's clip length is not carried yet
    }
    for (const Condition& condition : transition.conditions) {
        if (condition.parameter < 0) {
            return false;
        }
        const float value = _parameters[condition.parameter].value;
        bool ok = false;
        switch (condition.mode) {
            case 1: ok = value != 0.0f; break;
            case 2: ok = value == 0.0f; break;
            case 3: ok = value > condition.threshold; break;
            case 4: ok = value < condition.threshold; break;
            case 6: ok = value == condition.threshold; break;
            case 7: ok = value != condition.threshold; break;
            default: break;
        }
        if (!ok) {
            return false;
        }
    }
    return true;
}

int AvatarMenu::enterFromEntry(const Layer& layer) const {
    for (const Transition& transition : layer.entry) {
        if (transition.to >= 0 && holds(transition, 0.0f)) {
            return transition.to;
        }
    }
    return layer.defaultState;
}

void AvatarMenu::enter(Layer& layer, int stateIndex) {
    if (stateIndex < 0 || stateIndex >= (int)layer.states.size()) {
        return;
    }
    layer.current = stateIndex;
    layer.timeInState = 0.0f;
    const State& state = layer.states[stateIndex];
    if (state.writeDefaults) {
        // what the layer animates and this state doesn't goes back to the base
        QSet<int> objects = layer.animatedObjects, shapes = layer.animatedBlendshapes, joints = layer.animatedJoints;
        for (const auto& a : state.effect.active) {
            objects.remove(a.first);
        }
        for (const auto& b : state.effect.blendshapes) {
            shapes.remove(b.first);
        }
        for (const auto& j : state.effect.joints) {
            joints.remove(j.first);
        }
        for (int object : objects) {
            if (_objectActive[object] != _baseObjectActive[object]) {
                setObject(object, _baseObjectActive[object]);
            }
        }
        for (int shape : shapes) {
            const float value = _baseBlendshapes.value(shape, 0.0f);
            if (_blendshapes.value(shape, -1.0f) != value) {
                setBlendshape(shape, value);
            }
        }
        for (int joint : joints) {
            setJoint(joint, _baseJoints.contains(joint) ? &_baseJoints[joint] : nullptr);
        }
    }
    for (const auto& a : state.effect.active) {
        if (_objectActive[a.first] != a.second) {
            setObject(a.first, a.second);
        }
    }
    for (const auto& b : state.effect.blendshapes) {
        if (_blendshapes.value(b.first, -1.0f) != b.second) {
            setBlendshape(b.first, b.second);
        }
    }
    for (const auto& j : state.effect.joints) {
        JointPose pose = _joints.value(j.first);
        if (j.second.hasTranslation) {
            pose.hasTranslation = true;
            pose.translation = j.second.translation;
        }
        if (j.second.hasRotation) {
            pose.hasRotation = true;
            pose.rotation = j.second.rotation;
        }
        if (j.second.hasScale) {
            pose.hasScale = true;
            pose.scale = j.second.scale;
        }
        if (!_joints.contains(j.first) || !samePose(_joints[j.first], pose)) {
            setJoint(j.first, &pose);
        }
    }
    for (const Drive& drive : state.drives) {
        if (drive.localOnly && !_local) {
            continue;
        }
        Parameter& parameter = _parameters[drive.parameter];
        if (drive.type == 0) {
            assign(parameter, drive.value);
        } else if (drive.type == 1) {
            assign(parameter, parameter.value + drive.value);
        } else if (drive.type == 2) {
            // a random pick is its owner's: others take it from the parameter, when the avatar shares it
            if (_local) {
                if (parameter.type == "bool") {
                    assign(parameter, randFloat() < drive.chance ? 1.0f : 0.0f);
                } else if (parameter.type == "int") {
                    assign(parameter, glm::floor(drive.min + randFloat() * (drive.max - drive.min + 1.0f)));
                } else {
                    assign(parameter, drive.min + randFloat() * (drive.max - drive.min));
                }
            }
        } else if (drive.source >= 0) {
            float value = _parameters[drive.source].value;
            if (drive.convertRange && drive.sourceMax != drive.sourceMin) {
                value = drive.destMin + (value - drive.sourceMin) / (drive.sourceMax - drive.sourceMin) *
                                            (drive.destMax - drive.destMin);
            }
            assign(parameter, value);
        }
    }
}

void AvatarMenu::setLocal(bool local) {
    std::lock_guard<std::mutex> lock(_mutex);
    _local = local;
}

bool AvatarMenu::hasContactReceivers() const {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const Contact& contact : _contacts) {
        if (contact.receiver) {
            return true;
        }
    }
    return false;
}

bool AvatarMenu::placeContact(const Contact& contact, float scale, const JointWorld& jointWorld, glm::vec3& a, glm::vec3& b,
                              float& radius) {
    glm::vec3 position;
    glm::quat rotation;
    if (!jointWorld(contact.joint, position, rotation)) {
        return false;
    }
    const glm::vec3 center = position + rotation * (contact.position * scale);
    radius = contact.radius * scale;
    a = b = center;
    if (contact.capsule) {  // along its own up axis, its height counting the round ends
        const float half = glm::max(0.0f, 0.5f * contact.height * scale - radius);
        const glm::vec3 axis = rotation * contact.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        a = center - axis * half;
        b = center + axis * half;
    }
    return true;
}

void AvatarMenu::addContactSenders(const QUuid& owner, float scale, const JointWorld& jointWorld,
                                   std::vector<ContactSender>& senders) const {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const Contact& contact : _contacts) {
        ContactSender sender;
        if (!contact.receiver && placeContact(contact, scale, jointWorld, sender.a, sender.b, sender.radius)) {
            sender.owner = owner;
            sender.tags = contact.tags;
            senders.push_back(sender);
        }
    }
}

void AvatarMenu::updateContacts(const QUuid& owner, float scale, const JointWorld& jointWorld,
                                const std::vector<ContactSender>& senders) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (Contact& contact : _contacts) {
        glm::vec3 a, b;
        float radius;
        if (!contact.receiver || (contact.localOnly && !_local) || !placeContact(contact, scale, jointWorld, a, b, radius) ||
            radius <= 0.0f) {
            continue;
        }
        bool touching = false;
        float closeness = 0.0f;
        for (const ContactSender& sender : senders) {
            const bool own = sender.owner == owner;
            if ((own ? !contact.allowSelf : !contact.allowOthers) || !shareTag(contact.tags, sender.tags)) {
                continue;
            }
            const float distance = segmentDistance(a, b, sender.a, sender.b);
            if (distance < radius + sender.radius) {
                touching = true;
                // 1 with the sender reaching the receiver's middle, 0 at its edge
                closeness = glm::max(closeness, 1.0f - glm::clamp((distance - sender.radius) / radius, 0.0f, 1.0f));
            }
        }
        const float value = contact.type == 2 ? closeness
                          : contact.type == 1 ? (touching && !contact.touching ? 1.0f : 0.0f)
                                              : (touching ? 1.0f : 0.0f);
        contact.touching = touching;
        assign(_parameters[contact.parameter], value);
    }
}

void AvatarMenu::setObject(int object, bool active) {
    _objectActive[object] = active;
    if (_fadeDuration > 0.0f && _shownObjectActive[object] != active) {
        Fade fade;
        fade.duration = _fadeDuration;
        _objectFades[object] = fade;
    } else {
        _objectFades.remove(object);
        if (_shownObjectActive[object] != active) {
            _shownObjectActive[object] = active;
            _changed = true;
        }
    }
}

void AvatarMenu::setBlendshape(int shape, float value) {
    _blendshapes[shape] = value;
    const float shown = _shownBlendshapes.value(shape, _baseBlendshapes.value(shape, 0.0f));
    if (_fadeDuration > 0.0f && shown != value) {
        Fade fade;
        fade.from = shown;
        fade.duration = _fadeDuration;
        _blendshapeFades[shape] = fade;
    } else {
        _blendshapeFades.remove(shape);
        if (_shownBlendshapes.value(shape, -1.0f) != value) {
            _shownBlendshapes[shape] = value;
            _changed = true;
        }
    }
}

void AvatarMenu::setJoint(int joint, const JointPose* pose) {
    if (pose) {
        _joints[joint] = *pose;
    } else {
        _joints.remove(joint);
    }
    const bool shown = _shownJoints.contains(joint);
    if (_fadeDuration > 0.0f && (shown || pose)) {
        Fade fade;
        fade.fromPose = shown ? _shownJoints[joint] : JointPose();
        fade.duration = _fadeDuration;
        _jointFades[joint] = fade;
    } else {
        _jointFades.remove(joint);
        if (pose) {
            _shownJoints[joint] = *pose;
        } else {
            _shownJoints.remove(joint);
        }
        _changed = true;
    }
}

void AvatarMenu::advanceFades(float deltaTime) {
    for (auto it = _objectFades.begin(); it != _objectFades.end();) {
        it->elapsed += deltaTime;
        if (it->elapsed < 0.5f * it->duration) {
            ++it;
            continue;
        }
        _shownObjectActive[it.key()] = _objectActive[it.key()];  // shown or hidden halfway through
        _changed = true;
        it = _objectFades.erase(it);
    }
    for (auto it = _blendshapeFades.begin(); it != _blendshapeFades.end();) {
        it->elapsed += deltaTime;
        const float t = glm::min(1.0f, it->elapsed / it->duration);
        _shownBlendshapes[it.key()] = glm::mix(it->from, _blendshapes.value(it.key(), 0.0f), t);
        _changed = true;
        it = t >= 1.0f ? _blendshapeFades.erase(it) : std::next(it);
    }
    for (auto it = _jointFades.begin(); it != _jointFades.end();) {
        it->elapsed += deltaTime;
        const float t = glm::min(1.0f, it->elapsed / it->duration);
        const int joint = it.key();
        auto target = _joints.find(joint);
        _changed = true;
        if (t >= 1.0f) {
            if (target != _joints.end()) {
                _shownJoints[joint] = target.value();
            } else {
                _shownJoints.remove(joint);
            }
            it = _jointFades.erase(it);
            continue;
        }
        // a part only one end sets blends from or to the skeleton's own
        const JointPose rest = _restJoints.value(joint);
        const JointPose& from = it->fromPose;
        const JointPose to = target != _joints.end() ? target.value() : JointPose();
        JointPose pose;
        pose.hasTranslation = from.hasTranslation || to.hasTranslation;
        pose.translation = glm::mix(from.hasTranslation ? from.translation : rest.translation,
                                    to.hasTranslation ? to.translation : rest.translation, t);
        pose.hasRotation = from.hasRotation || to.hasRotation;
        pose.rotation = glm::slerp(from.hasRotation ? from.rotation : rest.rotation,
                                   to.hasRotation ? to.rotation : rest.rotation, t);
        pose.hasScale = from.hasScale || to.hasScale;
        pose.scale = glm::mix(from.hasScale ? from.scale : rest.scale, to.hasScale ? to.scale : rest.scale, t);
        _shownJoints[joint] = pose;
        ++it;
    }
}

bool AvatarMenu::update(float deltaTime) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (Layer& layer : _layers) {
        layer.timeInState += deltaTime;
        for (int hop = 0; hop < 8; hop++) {
            const Transition* next = nullptr;
            for (const Transition& transition : layer.anyState) {
                if (transition.to != layer.current && holds(transition, layer.timeInState)) {
                    next = &transition;
                    break;
                }
            }
            if (!next && layer.current >= 0) {
                for (const Transition& transition : layer.states[layer.current].transitions) {
                    if (holds(transition, layer.timeInState)) {
                        next = &transition;
                        break;
                    }
                }
            }
            if (!next) {
                break;
            }
            _fadeDuration = next->duration;
            enter(layer, next->to >= 0 ? next->to : enterFromEntry(layer));
            _fadeDuration = 0.0f;
        }
    }
    advanceFades(deltaTime);
    _parametersChanged = false;
    const bool changed = _changed;
    _changed = false;
    return changed;
}

QSet<int> AvatarMenu::getHiddenParts() const {
    std::lock_guard<std::mutex> lock(_mutex);
    QSet<int> hidden;
    for (int object = 0; object < (int)_shownObjectActive.size(); object++) {
        if (!_shownObjectActive[object]) {
            for (int part : _objectParts[object]) {
                hidden.insert(part);
            }
        }
    }
    return hidden;
}

QHash<int, float> AvatarMenu::getOwnBlendshapes() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _shownBlendshapes;
}

QHash<int, AvatarMenu::JointPose> AvatarMenu::getJointPoses() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _shownJoints;
}
