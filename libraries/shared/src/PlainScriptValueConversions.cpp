//
//  PlainScriptValueConversions.cpp
//  libraries/shared/src
//
//  Each function follows its QtScript twin in RegisteredMetaTypes.cpp line by line, less the prototypes a
//  script object carries.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PlainScriptValueConversions.h"

#include <cfloat>

#include <QtCore/QVariant>
#include <QtGui/QColor>

namespace {

// A member by its name or, failing that, its aliases (x, r, red), as vec3FromScriptValue reads one
PlainScriptValue member(const PlainScriptValue& object, const char* name, const char* alias, const char* longAlias) {
    PlainScriptValue value = object.property(QString::fromLatin1(name));
    if (!value.isValid()) {
        value = object.property(QString::fromLatin1(alias));
    }
    if (!value.isValid()) {
        value = object.property(QString::fromLatin1(longAlias));
    }
    return value;
}

}  // namespace

PlainScriptValue vec2ToScriptValue(PlainScriptEngine* engine, const glm::vec2& vec2) {
    PlainScriptValue value = engine->newObject();
    value.setProperty("x", vec2.x);
    value.setProperty("y", vec2.y);
    return value;
}

void vec2FromScriptValue(const PlainScriptValue& object, glm::vec2& vec2) {
    if (object.isNumber()) {
        vec2 = glm::vec2(object.toVariant().toFloat());
    } else if (object.isArray()) {
        QVariantList list = object.toVariant().toList();
        if (list.length() == 2) {
            vec2.x = list[0].toFloat();
            vec2.y = list[1].toFloat();
        }
    } else {
        PlainScriptValue x = object.property("x");
        if (!x.isValid()) {
            x = object.property("u");
        }
        PlainScriptValue y = object.property("y");
        if (!y.isValid()) {
            y = object.property("v");
        }
        vec2.x = x.toVariant().toFloat();
        vec2.y = y.toVariant().toFloat();
    }
}

PlainScriptValue vec3ToScriptValue(PlainScriptEngine* engine, const glm::vec3& vec3) {
    PlainScriptValue value = engine->newObject();
    value.setProperty("x", vec3.x);
    value.setProperty("y", vec3.y);
    value.setProperty("z", vec3.z);
    return value;
}

PlainScriptValue vec3ColorToScriptValue(PlainScriptEngine* engine, const glm::vec3& vec3) {
    PlainScriptValue value = engine->newObject();
    value.setProperty("red", vec3.x);
    value.setProperty("green", vec3.y);
    value.setProperty("blue", vec3.z);
    return value;
}

void vec3FromScriptValue(const PlainScriptValue& object, glm::vec3& vec3) {
    if (object.isNumber()) {
        vec3 = glm::vec3(object.toVariant().toFloat());
    } else if (object.isString()) {
        QColor qColor(object.toString());
        if (qColor.isValid()) {
            vec3.x = qColor.red();
            vec3.y = qColor.green();
            vec3.z = qColor.blue();
        }
    } else if (object.isArray()) {
        QVariantList list = object.toVariant().toList();
        if (list.length() == 3) {
            vec3.x = list[0].toFloat();
            vec3.y = list[1].toFloat();
            vec3.z = list[2].toFloat();
        }
    } else {
        vec3.x = member(object, "x", "r", "red").toVariant().toFloat();
        vec3.y = member(object, "y", "g", "green").toVariant().toFloat();
        vec3.z = member(object, "z", "b", "blue").toVariant().toFloat();
    }
}

PlainScriptValue u8vec3ToScriptValue(PlainScriptEngine* engine, const glm::u8vec3& vec3) {
    PlainScriptValue value = engine->newObject();
    value.setProperty("x", vec3.x);
    value.setProperty("y", vec3.y);
    value.setProperty("z", vec3.z);
    return value;
}

PlainScriptValue u8vec3ColorToScriptValue(PlainScriptEngine* engine, const glm::u8vec3& vec3) {
    PlainScriptValue value = engine->newObject();
    value.setProperty("red", vec3.x);
    value.setProperty("green", vec3.y);
    value.setProperty("blue", vec3.z);
    return value;
}

void u8vec3FromScriptValue(const PlainScriptValue& object, glm::u8vec3& vec3) {
    if (object.isNumber()) {
        vec3 = glm::vec3(object.toVariant().toUInt());
    } else if (object.isString()) {
        QColor qColor(object.toString());
        if (qColor.isValid()) {
            vec3.x = (uint8_t)qColor.red();
            vec3.y = (uint8_t)qColor.green();
            vec3.z = (uint8_t)qColor.blue();
        }
    } else if (object.isArray()) {
        QVariantList list = object.toVariant().toList();
        if (list.length() == 3) {
            vec3.x = list[0].toUInt();
            vec3.y = list[1].toUInt();
            vec3.z = list[2].toUInt();
        }
    } else {
        vec3.x = member(object, "x", "r", "red").toVariant().toUInt();
        vec3.y = member(object, "y", "g", "green").toVariant().toUInt();
        vec3.z = member(object, "z", "b", "blue").toVariant().toUInt();
    }
}

PlainScriptValue quatToScriptValue(PlainScriptEngine* engine, const glm::quat& quat) {
    PlainScriptValue obj = engine->newObject();
    if (quat.x != quat.x || quat.y != quat.y || quat.z != quat.z || quat.w != quat.w) {
        // if quat contains a NaN don't try to convert it
        return obj;
    }
    obj.setProperty("x", quat.x);
    obj.setProperty("y", quat.y);
    obj.setProperty("z", quat.z);
    obj.setProperty("w", quat.w);
    return obj;
}

void quatFromScriptValue(const PlainScriptValue& object, glm::quat& quat) {
    quat.x = object.property("x").toVariant().toFloat();
    quat.y = object.property("y").toVariant().toFloat();
    quat.z = object.property("z").toVariant().toFloat();
    quat.w = object.property("w").toVariant().toFloat();

    // enforce normalized quaternion
    float length = glm::length(quat);
    if (length > FLT_EPSILON) {
        quat /= length;
    } else {
        quat = glm::quat();
    }
}

PlainScriptValue qRectToScriptValue(PlainScriptEngine* engine, const QRect& rect) {
    PlainScriptValue obj = engine->newObject();
    obj.setProperty("x", rect.x());
    obj.setProperty("y", rect.y());
    obj.setProperty("width", rect.width());
    obj.setProperty("height", rect.height());
    return obj;
}

void qRectFromScriptValue(const PlainScriptValue& object, QRect& rect) {
    rect.setX(object.property("x").toVariant().toInt());
    rect.setY(object.property("y").toVariant().toInt());
    rect.setWidth(object.property("width").toVariant().toInt());
    rect.setHeight(object.property("height").toVariant().toInt());
}

PlainScriptValue aaCubeToScriptValue(PlainScriptEngine* engine, const AACube& aaCube) {
    PlainScriptValue obj = engine->newObject();
    const glm::vec3& corner = aaCube.getCorner();
    obj.setProperty("x", corner.x);
    obj.setProperty("y", corner.y);
    obj.setProperty("z", corner.z);
    obj.setProperty("scale", aaCube.getScale());
    return obj;
}

void aaCubeFromScriptValue(const PlainScriptValue& object, AACube& aaCube) {
    glm::vec3 corner;
    corner.x = object.property("x").toVariant().toFloat();
    corner.y = object.property("y").toVariant().toFloat();
    corner.z = object.property("z").toVariant().toFloat();
    float scale = object.property("scale").toVariant().toFloat();
    aaCube.setBox(corner, scale);
}

PlainScriptValue qVectorVec3ToScriptValue(PlainScriptEngine* engine, const QVector<glm::vec3>& vector) {
    PlainScriptValue array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array.setProperty(i, vec3ToScriptValue(engine, vector.at(i)));
    }
    return array;
}

PlainScriptValue qVectorVec3ColorToScriptValue(PlainScriptEngine* engine, const QVector<glm::vec3>& vector) {
    PlainScriptValue array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array.setProperty(i, vec3ColorToScriptValue(engine, vector.at(i)));
    }
    return array;
}

void qVectorVec3FromScriptValue(const PlainScriptValue& array, QVector<glm::vec3>& vector) {
    int length = array.property("length").toInteger();
    for (int i = 0; i < length; i++) {
        glm::vec3 newVec3 = glm::vec3();
        vec3FromScriptValue(array.property(i), newVec3);
        vector << newVec3;
    }
}

QVector<glm::vec3> qVectorVec3FromScriptValue(const PlainScriptValue& array) {
    QVector<glm::vec3> newVector;
    qVectorVec3FromScriptValue(array, newVector);
    return newVector;
}

PlainScriptValue qVectorQuatToScriptValue(PlainScriptEngine* engine, const QVector<glm::quat>& vector) {
    PlainScriptValue array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array.setProperty(i, quatToScriptValue(engine, vector.at(i)));
    }
    return array;
}

void qVectorQuatFromScriptValue(const PlainScriptValue& array, QVector<glm::quat>& vector) {
    int length = array.property("length").toInteger();
    for (int i = 0; i < length; i++) {
        glm::quat newQuat = glm::quat();
        quatFromScriptValue(array.property(i), newQuat);
        vector << newQuat;
    }
}

QVector<glm::quat> qVectorQuatFromScriptValue(const PlainScriptValue& array) {
    QVector<glm::quat> newVector;
    qVectorQuatFromScriptValue(array, newVector);
    return newVector;
}

PlainScriptValue qVectorBoolToScriptValue(PlainScriptEngine* engine, const QVector<bool>& vector) {
    PlainScriptValue array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array.setProperty(i, vector.at(i));
    }
    return array;
}

void qVectorBoolFromScriptValue(const PlainScriptValue& array, QVector<bool>& vector) {
    int length = array.property("length").toInteger();
    for (int i = 0; i < length; i++) {
        vector << array.property(i).toBool();
    }
}

QVector<bool> qVectorBoolFromScriptValue(const PlainScriptValue& array) {
    QVector<bool> newVector;
    qVectorBoolFromScriptValue(array, newVector);
    return newVector;
}

PlainScriptValue qVectorFloatToScriptValue(PlainScriptEngine* engine, const QVector<float>& vector) {
    PlainScriptValue array = engine->newArray();
    for (int i = 0; i < vector.size(); i++) {
        array.setProperty(i, PlainScriptValue(vector.at(i)));
    }
    return array;
}

void qVectorFloatFromScriptValue(const PlainScriptValue& array, QVector<float>& vector) {
    int length = array.property("length").toInteger();
    for (int i = 0; i < length; i++) {
        vector << array.property(i).toVariant().toFloat();
    }
}

// As the QtScript twin: nothing for a value that is not an array, and only the elements that are numbers
QVector<float> qVectorFloatFromScriptValue(const PlainScriptValue& array) {
    if (!array.isArray()) {
        return QVector<float>();
    }
    QVector<float> newVector;
    int length = array.property("length").toInteger();
    newVector.reserve(length);
    for (int i = 0; i < length; i++) {
        if (array.property(i).isNumber()) {
            newVector << array.property(i).toNumber();
        }
    }
    return newVector;
}
