//
//  PlainScriptValueConversions.h
//  libraries/shared/src
//
//  The plain-value twins of the QtScript conversions in RegisteredMetaTypes.h, under the same names, so that
//  code written against them compiles for a QScriptValue and for a PlainScriptValue alike. Each gives a plain
//  value the members the QtScript conversion gives a script, in the same order (a vec3 {x, y, z}, a colour
//  {red, green, blue}, a quaternion {x, y, z, w} or {} when it holds a NaN, a rectangle {x, y, width, height},
//  a cube {x, y, z, scale}, lists as arrays), and reads one back by the QtScript conversion's rules: a number
//  fills every component, text names a colour, an array gives the components in order, and otherwise the
//  members are read by their names and their aliases (x, r, red), an absent one as zero.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PlainScriptValueConversions_h
#define hifi_PlainScriptValueConversions_h

#include <QtCore/QRect>
#include <QtCore/QVector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AACube.h"
#include "PlainScriptValue.h"

PlainScriptValue vec2ToScriptValue(PlainScriptEngine* engine, const glm::vec2& vec2);
void vec2FromScriptValue(const PlainScriptValue& object, glm::vec2& vec2);

PlainScriptValue vec3ToScriptValue(PlainScriptEngine* engine, const glm::vec3& vec3);
PlainScriptValue vec3ColorToScriptValue(PlainScriptEngine* engine, const glm::vec3& vec3);
void vec3FromScriptValue(const PlainScriptValue& object, glm::vec3& vec3);

PlainScriptValue u8vec3ToScriptValue(PlainScriptEngine* engine, const glm::u8vec3& vec3);
PlainScriptValue u8vec3ColorToScriptValue(PlainScriptEngine* engine, const glm::u8vec3& vec3);
void u8vec3FromScriptValue(const PlainScriptValue& object, glm::u8vec3& vec3);

PlainScriptValue quatToScriptValue(PlainScriptEngine* engine, const glm::quat& quat);
void quatFromScriptValue(const PlainScriptValue& object, glm::quat& quat);

PlainScriptValue qRectToScriptValue(PlainScriptEngine* engine, const QRect& rect);
void qRectFromScriptValue(const PlainScriptValue& object, QRect& rect);

PlainScriptValue aaCubeToScriptValue(PlainScriptEngine* engine, const AACube& aaCube);
void aaCubeFromScriptValue(const PlainScriptValue& object, AACube& aaCube);

PlainScriptValue qVectorVec3ToScriptValue(PlainScriptEngine* engine, const QVector<glm::vec3>& vector);
PlainScriptValue qVectorVec3ColorToScriptValue(PlainScriptEngine* engine, const QVector<glm::vec3>& vector);
void qVectorVec3FromScriptValue(const PlainScriptValue& array, QVector<glm::vec3>& vector);
QVector<glm::vec3> qVectorVec3FromScriptValue(const PlainScriptValue& array);

PlainScriptValue qVectorQuatToScriptValue(PlainScriptEngine* engine, const QVector<glm::quat>& vector);
void qVectorQuatFromScriptValue(const PlainScriptValue& array, QVector<glm::quat>& vector);
QVector<glm::quat> qVectorQuatFromScriptValue(const PlainScriptValue& array);

PlainScriptValue qVectorBoolToScriptValue(PlainScriptEngine* engine, const QVector<bool>& vector);
void qVectorBoolFromScriptValue(const PlainScriptValue& array, QVector<bool>& vector);
QVector<bool> qVectorBoolFromScriptValue(const PlainScriptValue& array);

PlainScriptValue qVectorFloatToScriptValue(PlainScriptEngine* engine, const QVector<float>& vector);
void qVectorFloatFromScriptValue(const PlainScriptValue& array, QVector<float>& vector);
QVector<float> qVectorFloatFromScriptValue(const PlainScriptValue& array);

#endif // hifi_PlainScriptValueConversions_h
