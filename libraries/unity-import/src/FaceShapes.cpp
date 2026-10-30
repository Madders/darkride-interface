//
//  FaceShapes.cpp
//  libraries/unity-import/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "FaceShapes.h"

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

namespace unity {

namespace {

struct Choice {
    const char* shape;
    float weight;
};

// ARKit's names to the engine's, one to one (ARKit's set came from Faceshift's), the procedurally driven ones left out
const char* const ARKIT_TO_ENGINE[][2] = {
    { "eyeSquintLeft", "EyeSquint_L" }, { "eyeSquintRight", "EyeSquint_R" },
    { "eyeLookDownLeft", "EyeDown_L" }, { "eyeLookDownRight", "EyeDown_R" },
    { "eyeLookInLeft", "EyeIn_L" }, { "eyeLookInRight", "EyeIn_R" },
    { "eyeLookOutLeft", "EyeOut_L" }, { "eyeLookOutRight", "EyeOut_R" },
    { "eyeLookUpLeft", "EyeUp_L" }, { "eyeLookUpRight", "EyeUp_R" },
    { "browDownLeft", "BrowsD_L" }, { "browDownRight", "BrowsD_R" },
    { "jawForward", "JawFwd" }, { "jawLeft", "JawLeft" }, { "jawRight", "JawRight" },
    { "mouthLeft", "MouthLeft" }, { "mouthRight", "MouthRight" },
    { "mouthFrownLeft", "MouthFrown_L" }, { "mouthFrownRight", "MouthFrown_R" },
    { "mouthDimpleLeft", "MouthDimple_L" }, { "mouthDimpleRight", "MouthDimple_R" },
    { "mouthStretchLeft", "LipsStretch_L" }, { "mouthStretchRight", "LipsStretch_R" },
    { "mouthRollLower", "LipsLowerClose" },
    { "mouthPucker", "LipsPucker" },
    { "cheekPuff", "Puff" },
    { "cheekSquintLeft", "CheekSquint_L" }, { "cheekSquintRight", "CheekSquint_R" },
    { "mouthClose", "MouthClose" },
    { "mouthUpperUpLeft", "MouthUpperUp_L" }, { "mouthUpperUpRight", "MouthUpperUp_R" },
    { "mouthLowerDownLeft", "MouthLowerDown_L" }, { "mouthLowerDownRight", "MouthLowerDown_R" },
    { "mouthPressLeft", "MouthPress_L" }, { "mouthPressRight", "MouthPress_R" },
    { "mouthShrugLower", "MouthShrugLower" }, { "mouthShrugUpper", "MouthShrugUpper" },
    { "noseSneerLeft", "NoseSneer_L" }, { "noseSneerRight", "NoseSneer_R" },
    { "tongueOut", "TongueOut" },
};

// a left and right pair: a shape per side, else one shape for both at half weight each (any of them may be empty)
struct Sides {
    const char* engineLeft;
    const char* engineRight;
    const char* left;
    const char* right;
    const char* both;
    float weight;
};

const Sides SIDES[] = {
    { "EyeSquint_L", "EyeSquint_R", "Fcl_EYE_Joy_L", "Fcl_EYE_Joy_R", "Fcl_EYE_Joy", 0.6f },          // smiling eyes
    { "EyeSquint_L", "EyeSquint_R", "eye_happy_left", "eye_happy_right", "eye_happy", 0.6f },
    { "EyeSquint_L", "EyeSquint_R", "", "", "笑い", 0.6f },                                            // MMD: smiling
    { "BrowsD_L", "BrowsD_R", "", "", "Fcl_BRW_Angry", 1.0f },                                       // brows down
    { "BrowsD_L", "BrowsD_R", "eyebrow_angry_left", "eyebrow_angry_right", "eyebrow_angry", 1.0f },
    { "BrowsD_L", "BrowsD_R", "", "", "怒り", 1.0f },                                                 // MMD: angry
    { "MouthFrown_L", "MouthFrown_R", "", "", "Fcl_MTH_Sorrow", 1.0f },                              // corners down
    { "MouthFrown_L", "MouthFrown_R", "mouth_sad_left", "mouth_sad_right", "mouth_sad", 1.0f },
    { "MouthFrown_L", "MouthFrown_R", "", "", "口角下げ", 1.0f },                                       // MMD: corners down
    { "LipsStretch_L", "LipsStretch_R", "", "", "Fcl_MTH_I", 0.8f },                                 // a wide mouth, "ee"
    { "LipsStretch_L", "LipsStretch_R", "mouth_wide_left", "mouth_wide_right", "mouth_i", 0.8f },
    { "LipsStretch_L", "LipsStretch_R", "", "", "い", 0.8f },
    { "LipsStretch_L", "LipsStretch_R", "", "", "vrc.v.ih", 0.8f },
    { "LipsStretch_L", "LipsStretch_R", "", "", "vrc.v_ih", 0.8f },
};

struct Single {
    const char* engine;
    Choice choices[8];
};

const Single SINGLES[] = {
    { "LipsPucker", { { "Fcl_MTH_U", 1.0f }, { "mouth_u", 1.0f }, { "う", 1.0f }, { "vrc.v.ou", 1.0f }, { "vrc.v_ou", 1.0f },
                      { "U", 1.0f }, { "ou", 1.0f } } },
    { "TongueOut", { { "Fcl_MTH_Tongue", 1.0f }, { "mouth_tang", 1.0f }, { "ぺろっ", 1.0f }, { "tongue_out", 1.0f } } },
    { "Puff", { { "cheek_puff", 1.0f }, { "Fcl_MTH_Puff", 1.0f } } },
};

// the twelve shapes the procedural face drives, for faceTrackingOnlyShapeLines
const char* const ARKIT_TO_ENGINE_TRACKED_ONLY[][2] = {
    { "eyeBlinkLeft", "EyeBlink_L" }, { "eyeBlinkRight", "EyeBlink_R" },
    { "eyeWideLeft", "EyeOpen_L" }, { "eyeWideRight", "EyeOpen_R" },
    { "browInnerUp", "BrowsU_C" }, { "browOuterUpLeft", "BrowsU_L" }, { "browOuterUpRight", "BrowsU_R" },
    { "jawOpen", "JawOpen" },
    { "mouthSmileLeft", "MouthSmile_L" }, { "mouthSmileRight", "MouthSmile_R" },
    { "mouthFunnel", "LipsFunnel" }, { "mouthRollUpper", "LipsUpperClose" },
};

const Sides SIDES_TRACKED_ONLY[] = {
    { "EyeBlink_L", "EyeBlink_R", "Fcl_EYE_Close_L", "Fcl_EYE_Close_R", "Fcl_EYE_Close", 1.0f },
    { "EyeBlink_L", "EyeBlink_R", "eye_close_left", "eye_close_right", "eye_close", 1.0f },
    { "EyeBlink_L", "EyeBlink_R", "Blink_L", "Blink_R", "Blink", 1.0f },
    { "EyeBlink_L", "EyeBlink_R", "blink_left", "blink_right", "blink", 1.0f },
    { "EyeBlink_L", "EyeBlink_R", "vrc.blink_left", "vrc.blink_right", "", 1.0f },
    { "EyeBlink_L", "EyeBlink_R", "ウィンク", "ウィンク右", "まばたき", 1.0f },                          // MMD: winks, blink
    { "EyeOpen_L", "EyeOpen_R", "", "", "Fcl_EYE_Surprised", 1.0f },                                // wide eyes
    { "EyeOpen_L", "EyeOpen_R", "eye_open_left", "eye_open_right", "eye_surprise", 1.0f },
    { "EyeOpen_L", "EyeOpen_R", "", "", "びっくり", 1.0f },                                            // MMD: surprised
    { "BrowsU_L", "BrowsU_R", "", "", "Fcl_BRW_Surprised", 1.0f },                                  // brows up
    { "BrowsU_L", "BrowsU_R", "eyebrow_up_left", "eyebrow_up_right", "eyebrow_up", 1.0f },
    { "BrowsU_L", "BrowsU_R", "", "", "上", 1.0f },                                                   // MMD: brows up
    { "MouthSmile_L", "MouthSmile_R", "", "", "Fcl_MTH_Joy", 1.0f },                                // corners up
    { "MouthSmile_L", "MouthSmile_R", "mouth_smile_left", "mouth_smile_right", "mouth_smile", 1.0f },
    { "MouthSmile_L", "MouthSmile_R", "mouth_smile2_left", "mouth_smile2_right", "", 1.0f },
    { "MouthSmile_L", "MouthSmile_R", "", "", "口角上げ", 1.0f },                                       // MMD: corners up
};

const Single SINGLES_TRACKED_ONLY[] = {
    { "BrowsU_C", { { "Fcl_BRW_Sorrow", 1.0f }, { "eyebrow_trouble", 1.0f }, { "eyebrow_sad", 1.0f }, { "困る", 1.0f } } },
    { "JawOpen", { { "Fcl_MTH_A", 1.0f }, { "mouth_a", 1.0f }, { "あ", 1.0f }, { "vrc.v.aa", 1.0f }, { "vrc.v_aa", 1.0f },
                   { "A", 1.0f }, { "aa", 1.0f } } },
    { "LipsFunnel", { { "Fcl_MTH_O", 1.0f }, { "mouth_o", 1.0f }, { "お", 1.0f }, { "vrc.v.oh", 1.0f }, { "vrc.v_oh", 1.0f },
                      { "O", 1.0f }, { "oh", 1.0f } } },
};

// the same shape key under small differences of writing: case, spaces, dots, dashes and underscores. Unlike the
// importer's findShape this keeps letters outside a to z, for the Japanese names
QString fold(const QString& name) {
    static const QRegularExpression SEPARATORS("[\\s._\\-]");
    QString key = name.toLower();
    key.remove(SEPARATORS);
    return key;
}

// the lines `<field> = <engine name> = <avatar's shape> = <weight>` from the three tables, each engine name once and none
// that alreadyMapped names
template <size_t A, size_t S, size_t N>
QStringList shapeLines(const QString& field, const char* const (&arkitToEngine)[A][2], const Sides (&sidesTable)[S],
                       const Single (&singles)[N], const QSet<QString>& shapes, const QStringList& alreadyMapped) {
    QHash<QString, QString> folded;
    for (const QString& shape : shapes) {
        folded.insert(fold(shape), shape);
    }
    auto find = [&](const char* name) -> QString {
        if (!name || !*name) {
            return QString();
        }
        const QString wanted = QString::fromUtf8(name);
        return shapes.contains(wanted) ? wanted : folded.value(fold(wanted));
    };

    QSet<QString> mapped;
    for (const QString& line : alreadyMapped) {
        mapped.insert(line.section('=', 1, 1).trimmed());
    }
    QStringList lines;
    auto add = [&](const QString& engine, const QString& shape, float weight) {
        if (!mapped.contains(engine) && !shape.isEmpty()) {
            lines << QString("%1 = %2 = %3 = %4").arg(field, engine, shape).arg(weight);
            mapped.insert(engine);
        }
    };
    for (const auto& pair : arkitToEngine) {
        add(pair[1], find(pair[0]), 1.0f);
    }
    for (const Sides& sides : sidesTable) {
        const QString left = find(sides.left);
        const QString right = find(sides.right);
        if (!left.isEmpty() && !right.isEmpty()) {
            add(sides.engineLeft, left, sides.weight);
            add(sides.engineRight, right, sides.weight);
        } else {
            const QString both = find(sides.both);
            add(sides.engineLeft, both, sides.weight * 0.5f);
            add(sides.engineRight, both, sides.weight * 0.5f);
        }
    }
    for (const Single& single : singles) {
        for (const Choice& choice : single.choices) {
            if (!choice.shape) {
                break;
            }
            const QString shape = find(choice.shape);
            if (!shape.isEmpty()) {
                add(single.engine, shape, choice.weight);
                break;
            }
        }
    }
    return lines;
}

}  // namespace

QStringList faceTrackingShapeLines(const QSet<QString>& shapes, const QStringList& alreadyMapped) {
    return shapeLines("bs", ARKIT_TO_ENGINE, SIDES, SINGLES, shapes, alreadyMapped);
}

QStringList faceTrackingOnlyShapeLines(const QSet<QString>& shapes, const QStringList& alreadyMapped) {
    return shapeLines("faceTrackingBs", ARKIT_TO_ENGINE_TRACKED_ONLY, SIDES_TRACKED_ONLY, SINGLES_TRACKED_ONLY, shapes,
                      alreadyMapped);
}

int countFaceShapes(const QStringList& lines) {
    QSet<QString> engineNames;
    for (const QString& line : lines) {
        if (line.startsWith("bs ") || line.startsWith("faceTrackingBs ")) {
            engineNames.insert(line.section('=', 1, 1).trimmed());
        }
    }
    return engineNames.size();
}

}  // namespace unity
