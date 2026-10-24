//
//  PlainScriptValue.h
//  libraries/shared/src
//
//  A script value made of plain data: undefined, null, a boolean, a number, text, an array, or an object whose
//  properties keep the order they were set in. It offers the part of QScriptValue's and QScriptEngine's interface
//  that the entity-property conversions use (property, setProperty, the type tests, toVariant, toString,
//  toNumber, toBool, toInteger, newObject, newArray, construction from the primitives), so that the one source of
//  those conversions compiles against QtScript and against plain values: a module's CBOR, a world file,
//  avatar-entity data. It is not a script engine: no prototypes, no functions, no getters.
//
//  What QtScript does, this does: a number that is an int32 value (and not -0) is an int and any other a double,
//  which is what toVariant() gives, type for type (JavaScriptCore keeps such numbers as int32 immediates);
//  undefined and null are invalid QVariants; an object's toVariant() is a QVariantMap, so its keys come sorted,
//  where toCbor() keeps the order the properties were set in; property() on anything but an object is invalid, as
//  is an absent property; setProperty with an invalid value removes the property. An object or an array is
//  shared by its copies, as a QScriptValue's is: a property set through one copy is seen through every other.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_PlainScriptValue_h
#define hifi_PlainScriptValue_h

#include <memory>

#include <QtCore/QCborValue>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

class PlainScriptValue {
public:
    PlainScriptValue();  // invalid, as a default QScriptValue
    PlainScriptValue(bool value);
    PlainScriptValue(int value);
    PlainScriptValue(uint value);
    PlainScriptValue(double value);
    PlainScriptValue(const QString& value);
    PlainScriptValue(const char* value);

    static PlainScriptValue undefinedValue();
    static PlainScriptValue nullValue();
    static PlainScriptValue newObject();
    static PlainScriptValue newArray();

    bool isValid() const { return _kind != Kind::Invalid; }
    bool isUndefined() const { return _kind == Kind::Undefined; }
    bool isNull() const { return _kind == Kind::Null; }
    bool isBool() const { return _kind == Kind::Bool; }
    bool isNumber() const { return _kind == Kind::Number; }
    bool isString() const { return _kind == Kind::String; }
    bool isArray() const { return _kind == Kind::Array; }
    /// An object or an array, as QScriptValue::isObject.
    bool isObject() const { return _kind == Kind::Object || _kind == Kind::Array; }

    /// A property of an object, or an element or the length of an array; invalid when absent or when this is
    /// not an object.
    PlainScriptValue property(const QString& name) const;
    PlainScriptValue property(quint32 index) const;
    /// Sets a property of an object, or an element or the length of an array; an invalid value removes the
    /// property; nothing happens on a value that is not an object.
    void setProperty(const QString& name, const PlainScriptValue& value);
    void setProperty(quint32 index, const PlainScriptValue& value);
    /// An object's property names in the order they were set; an array's indices as text.
    QStringList propertyNames() const;

    bool toBool() const;
    double toNumber() const;
    double toInteger() const;
    qint32 toInt32() const;
    quint32 toUInt32() const;
    quint16 toUInt16() const;
    QString toString() const;
    QVariant toVariant() const;

    /// The value as CBOR: an object a map of its properties in order, a number an integer or a double as it is held.
    QCborValue toCbor() const;
    /// A value from CBOR: a map an object with its keys in the map's order (a key that is not text is skipped,
    /// but for an integer, which is used as text), a byte string the text of its base64.
    static PlainScriptValue fromCbor(const QCborValue& value);
    /// A value from a QVariant, with the rules of variantToScriptValue (VariantMapToScriptValue.h), which makes
    /// a QScriptValue from one for a world file or avatar-entity data: a boolean, an int, a double, text (a URL
    /// as its text), a map an object with the map's keys in the map's order, a list an array, any other value
    /// that converts to a float (an unsigned or 64-bit integer, a float) as that number, and the rest invalid,
    /// so that a null and a value of any other type are left out of the object or array.
    static PlainScriptValue fromVariant(const QVariant& value);

    /// JavaScript's text of a number ("1", "1.5", "1e+21", "NaN"), as a script engine's toString gives it.
    static QString numberToString(double value);

    /// The value as JSON text with JSON.stringify's rules: an object's properties in their order, numbers in
    /// JavaScript's format (a NaN or an infinity as null), an undefined property left out and an undefined
    /// element null, text escaped as JSON.stringify escapes it; with an indent, the layout of
    /// JSON.stringify(value, null, indent). The entity server's world file is written this way.
    QString toJson(const QString& indent = QString()) const;

private:
    enum class Kind { Invalid, Undefined, Null, Bool, Number, String, Array, Object };
    struct Container;

    PlainScriptValue(Kind kind) : _kind(kind) {}

    void appendJson(QString& out, const QString& indent, const QString& current) const;

    Kind _kind { Kind::Invalid };
    QVariant _primitive;                    // the boolean, the int or the double, or the text
    std::shared_ptr<Container> _container;  // the object's properties or the array's elements
};

/// What the conversions ask an engine for: new objects and arrays.
class PlainScriptEngine {
public:
    PlainScriptValue newObject() const { return PlainScriptValue::newObject(); }
    PlainScriptValue newArray() const { return PlainScriptValue::newArray(); }
};

#endif // hifi_PlainScriptValue_h
