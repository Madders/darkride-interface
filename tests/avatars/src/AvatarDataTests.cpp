//
//  AvatarDataTests.cpp
//  tests/avatars/src
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
//

#include "AvatarDataTests.h"

#include <glm/gtc/quaternion.hpp>

#include <AvatarData.h>
#include <BlendshapeConstants.h>
#include <GLMHelpers.h>

QTEST_MAIN(AvatarDataTests)

namespace {

float coefficientAt(int index) {
    return (float)(index % 100) / 100.0f;
}

const glm::quat JOINT_ROTATION = glm::angleAxis(0.5f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));

// avatar data as a packet carries it: the flags, the face with numCoefficients coefficients, then one joint whose
// rotation is JOINT_ROTATION
QByteArray avatarDataWithFace(int numCoefficients) {
    QByteArray buffer;
    const AvatarDataPacket::HasFlags flags =
        AvatarDataPacket::PACKET_HAS_FACE_TRACKER_INFO | AvatarDataPacket::PACKET_HAS_JOINT_DATA;
    buffer.append(reinterpret_cast<const char*>(&flags), sizeof(flags));

    AvatarDataPacket::FaceTrackerInfo face {};
    face.numBlendshapeCoefficients = (uint8_t)numCoefficients;
    buffer.append(reinterpret_cast<const char*>(&face), sizeof(face));
    for (int i = 0; i < numCoefficients; i++) {
        const float coefficient = coefficientAt(i);
        buffer.append(reinterpret_cast<const char*>(&coefficient), sizeof(coefficient));
    }

    // one joint: its count, its rotation's validity bit, the rotation in six bytes, no translation, the translations' scale
    unsigned char joint[1 + 1 + 6 + 1 + sizeof(float)] {};
    joint[0] = 1;
    joint[1] = 1;
    packOrientationQuatToSixBytes(joint + 2, JOINT_ROTATION);
    joint[8] = 0;
    const float maxTranslationDimension = 1.0f;
    memcpy(joint + 9, &maxTranslationDimension, sizeof(maxTranslationDimension));
    buffer.append(reinterpret_cast<const char*>(joint), sizeof(joint));
    return buffer;
}

void verifyParsed(int numCoefficients) {
    const QByteArray buffer = avatarDataWithFace(numCoefficients);
    AvatarData avatar;
    QCOMPARE(avatar.parseDataFromBuffer(buffer), buffer.size());

    const QVector<float>& coefficients = avatar.getHeadData()->getBlendshapeCoefficients();
    QCOMPARE(coefficients.size(), std::min(numCoefficients, (int)Blendshapes::BlendshapeCount));
    for (int i = 0; i < coefficients.size(); i++) {
        QCOMPARE(coefficients[i], coefficientAt(i));
    }

    // the joint after the face is read from where it is
    QVERIFY(fabsf(glm::dot(avatar.getJointRotation(0), JOINT_ROTATION)) > 0.9999f);
}

}

void AvatarDataTests::faceCoefficientsTest() {
    verifyParsed((int)Blendshapes::BlendshapeCount);
}

void AvatarDataTests::moreFaceCoefficientsThanTheHeadHoldsTest() {
    // the head keeps the first BlendshapeCount, the rest are skipped
    verifyParsed(255);
}

void AvatarDataTests::noFaceCoefficientsTest() {
    verifyParsed(0);
}
