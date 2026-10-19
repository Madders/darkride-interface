//
//  AvatarMenu.h
//  libraries/avatars-renderer/src/avatars-renderer
//
//  Created on 2026-10-14 for Darkride's import of avatars from Unity packages.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#ifndef hifi_AvatarMenu_h
#define hifi_AvatarMenu_h

#include <functional>
#include <mutex>
#include <vector>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariantList>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <hfm/HFM.h>

class AnimSkeleton;

// An avatar's menu, the parameters it sets, and the effects layer's state machines those parameters drive, from the
// .fst's "avatarMenu" line (the avatar import writes it; unity-import's AvatarMenuImport.h has the
// format). Each layer follows its transitions as the parameters change, and each state's effect sets what the avatar's
// animator would: objects shown or hidden, the model's own blend shapes, and joints moved, turned or scaled. A state
// that writes defaults first puts back what its layer animates. What a transition changes moves there over its
// duration: blend shapes and joints blend, an object shows or hides halfway through. Exit times count from entering
// the state.
class AvatarMenu {
public:
    struct JointPose {
        bool hasTranslation { false };
        bool hasRotation { false };
        bool hasScale { false };
        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale { 1.0f };
    };

    // Reads the data and resolves it against the avatar's model and skeleton; empty data clears.
    void setup(const QJsonObject& data, const hfm::Model& model, const AnimSkeleton& skeleton);
    void clear();
    bool isActive() const;

    // For scripts and the menu: the menu as the data has it, and each parameter with its value.
    QJsonObject getMenu() const;
    QJsonArray getEmotes() const;  // the emote layer's emotes the menu picks (unity-import's AvatarMenuImport.h)
    QVariantList getParameters() const;
    bool setParameter(const QString& name, float value);
    float getParameter(const QString& name, float fallback = 0.0f) const;

    // The parameters others' clients need, by name: those the avatar marks synced, and the built-in gesture ones. The
    // avatar's own client sends them (MyAvatar); others' clients set them from what arrives (Avatar).
    QJsonObject getSyncedParameters() const;
    void setSyncedParameters(const QJsonObject& values);

    // Follows the layers' transitions; true when what the avatar shows changed since the last call.
    bool update(float deltaTime);
    // Touches: an avatar's contact receivers set their parameters when someone's sender overlaps them (unity-import's
    // AvatarMenuImport.h has the data). Senders are spheres or capsules in the world, each with its avatar.
    struct ContactSender {
        QUuid owner;
        glm::vec3 a, b;  // a capsule's ends; one point for a sphere
        float radius { 0.0f };
        QStringList tags;
    };
    using JointWorld = std::function<bool(int joint, glm::vec3& position, glm::quat& rotation)>;
    void setLocal(bool local);  // the owner's own avatar runs what only its owner runs; set before setup
    bool hasContactReceivers() const;
    // the avatar's own senders and its receivers, from its joints in the world; scale turns its meters into the world's
    void addContactSenders(const QUuid& owner, float scale, const JointWorld& jointWorld,
                           std::vector<ContactSender>& senders) const;
    void updateContacts(const QUuid& owner, float scale, const JointWorld& jointWorld,
                        const std::vector<ContactSender>& senders);

    QSet<int> getHiddenParts() const;
    QHash<int, float> getOwnBlendshapes() const;           // index into the model's own blend shapes -> 0 to 1
    QHash<int, JointPose> getJointPoses() const;           // skeleton joint -> the parts the layers set

private:
    struct Condition {
        int parameter { -1 };
        int mode { 0 };  // 1 if, 2 ifNot, 3 greater, 4 less, 6 equals, 7 notEqual
        float threshold { 0.0f };
    };
    struct Transition {
        int to { -1 };   // -1: the state machine's exit, back to its entry
        std::vector<Condition> conditions;
        bool hasExitTime { false };
        float exitTime { 0.0f };
        float duration { 0.0f };  // seconds what it changes takes
    };
    struct Effect {
        std::vector<std::pair<int, bool>> active;            // object -> shown
        std::vector<std::pair<int, float>> blendshapes;      // own blend shape -> weight
        std::vector<std::pair<int, JointPose>> joints;       // joint -> parts set
    };
    struct Drive {  // what a state sets on entering
        int type { 0 };  // 0 set, 1 add, 2 random, 3 copy
        int parameter { -1 };
        int source { -1 };
        float value { 0.0f };
        float min { 0.0f };
        float max { 0.0f };
        float chance { 0.0f };
        bool convertRange { false };
        float sourceMin { 0.0f };
        float sourceMax { 0.0f };
        float destMin { 0.0f };
        float destMax { 0.0f };
        bool localOnly { false };
    };
    struct State {
        bool writeDefaults { false };
        Effect effect;
        std::vector<Transition> transitions;
        std::vector<Drive> drives;
    };
    struct Layer {
        int defaultState { -1 };
        std::vector<Transition> entry;
        std::vector<Transition> anyState;
        std::vector<State> states;
        int current { -1 };
        float timeInState { 0.0f };
        QSet<int> animatedObjects, animatedBlendshapes, animatedJoints;  // what any of its states sets
    };
    struct Parameter {
        QString name;
        QString type;
        float value { 0.0f };
        float defaultValue { 0.0f };
        bool saved { false };
        bool synced { false };
        bool builtin { false };
    };

    struct Contact {
        bool receiver { true };
        int joint { -1 };
        glm::vec3 position;  // in the joint's frame, in the avatar's meters
        glm::quat rotation;
        bool capsule { false };
        float radius { 0.0f };
        float height { 0.0f };
        QStringList tags;
        bool allowSelf { true };
        bool allowOthers { true };
        bool localOnly { false };
        int type { 0 };  // 0 while touching, 1 on touching, 2 by how close to the middle
        int parameter { -1 };
        bool touching { false };
    };
    static bool placeContact(const Contact& contact, float scale, const JointWorld& jointWorld, glm::vec3& a, glm::vec3& b,
                             float& radius);
    static bool isSynced(const Parameter& parameter);
    void assign(Parameter& parameter, float value);  // with its type's rounding and range; the caller holds the lock
    bool holds(const Transition& transition, float timeInState) const;
    int enterFromEntry(const Layer& layer) const;
    void enter(Layer& layer, int state);
    // what the layers set, which what the avatar shows follows over the entering transition's duration
    void setObject(int object, bool active);
    void setBlendshape(int shape, float value);
    void setJoint(int joint, const JointPose* pose);  // nullptr: none of the layers' poses
    void advanceFades(float deltaTime);

    mutable std::mutex _mutex;
    QJsonObject _menu;
    QJsonArray _emotes;
    std::vector<Parameter> _parameters;
    QHash<QString, int> _parameterIndex;
    std::vector<Layer> _layers;
    std::vector<std::vector<int>> _objectParts;
    // what the layers set now, before any layer wrote, and what the avatar shows on its way to what the layers set
    std::vector<bool> _objectActive, _baseObjectActive, _shownObjectActive;
    QHash<int, float> _blendshapes, _baseBlendshapes, _shownBlendshapes;
    QHash<int, JointPose> _joints, _baseJoints, _shownJoints;
    QHash<int, JointPose> _restJoints;  // the skeleton's own local poses, where a joint's fade starts or ends unset
    struct Fade {
        float from { 0.0f };  // a blend shape's weight when the fade began
        JointPose fromPose;   // a joint's pose when the fade began
        float elapsed { 0.0f };
        float duration { 0.0f };
    };
    QHash<int, Fade> _objectFades, _blendshapeFades, _jointFades;
    float _fadeDuration { 0.0f };  // the duration of the transition being entered
    std::vector<Contact> _contacts;
    bool _local { true };
    bool _changed { false };
    bool _parametersChanged { false };
};

#endif // hifi_AvatarMenu_h
