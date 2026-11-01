//
//  PlainScriptValue.cpp
//  libraries/shared/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "PlainScriptValue.h"

#include <cmath>
#include <limits>

#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QLocale>
#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtCore/QVector>

struct PlainScriptValue::Container {
    bool isArray { false };
    QStringList names;                 // an object's property names, in the order they were set
    QHash<QString, int> positions;     // a name's position in names and values
    QVector<PlainScriptValue> values;  // an object's values by position; an array's elements by index

    int position(const QString& name) const { return positions.value(name, -1); }

    void set(const QString& name, const PlainScriptValue& value) {
        int at = position(name);
        if (at >= 0) {
            values[at] = value;  // a property set again keeps its place, as in JavaScript
            return;
        }
        positions.insert(name, names.size());
        names.append(name);
        values.append(value);
    }

    void remove(const QString& name) {
        int at = position(name);
        if (at < 0) {
            return;
        }
        names.removeAt(at);
        values.removeAt(at);
        positions.clear();
        for (int i = 0; i < names.size(); ++i) {
            positions.insert(names.at(i), i);
        }
    }
};

namespace {

const double TWO_TO_THE_32 = 4294967296.0;
const quint32 LARGEST_ARRAY_LENGTH = 1u << 26;  // beyond what any conversion builds; a wild index is ignored

// JavaScriptCore keeps a number that is an int32 value as an int32, except -0; the rest are doubles
QVariant numberVariant(double value) {
    if (value >= std::numeric_limits<qint32>::min() && value <= std::numeric_limits<qint32>::max() &&
        value == std::trunc(value) && !(value == 0 && std::signbit(value))) {
        return QVariant(static_cast<int>(value));
    }
    return QVariant(value);
}

// JavaScript's reading of text as a number (StringToNumber): blank is 0, hexadecimal is read, anything else that
// is not a decimal number is NaN
double stringToNumber(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return 0.0;
    }
    if (trimmed == QLatin1String("Infinity") || trimmed == QLatin1String("+Infinity")) {
        return std::numeric_limits<double>::infinity();
    }
    if (trimmed == QLatin1String("-Infinity")) {
        return -std::numeric_limits<double>::infinity();
    }
    if (trimmed.startsWith(QLatin1String("0x")) || trimmed.startsWith(QLatin1String("0X"))) {
        bool ok = false;
        const qulonglong hex = trimmed.mid(2).toULongLong(&ok, 16);
        return ok ? static_cast<double>(hex) : std::numeric_limits<double>::quiet_NaN();
    }
    // QString::toDouble reads "inf" and "nan", which JavaScript does not
    const QString lower = trimmed.toLower();
    if (lower.contains(QLatin1String("inf")) || lower.contains(QLatin1String("nan"))) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    return ok ? value : std::numeric_limits<double>::quiet_NaN();
}

// JavaScript's ToInteger: NaN is 0, infinities stay, the rest truncate toward zero
double toIntegerOf(double value) {
    if (std::isnan(value)) {
        return 0.0;
    }
    if (std::isinf(value)) {
        return value;
    }
    return std::trunc(value);
}

// JavaScript's ToUint32: modulo 2^32 of the truncated value
quint32 toUInt32Of(double value) {
    if (std::isnan(value) || std::isinf(value)) {
        return 0;
    }
    double remainder = std::fmod(std::trunc(value), TWO_TO_THE_32);
    if (remainder < 0) {
        remainder += TWO_TO_THE_32;
    }
    return static_cast<quint32>(remainder);
}

// A canonical array index as JavaScript reads one from a property name: digits with no leading zero, below 2^32 - 1
bool arrayIndexOf(const QString& name, quint32& index) {
    if (name.isEmpty() || name.size() > 10 || (name.size() > 1 && name.at(0) == QLatin1Char('0'))) {
        return false;
    }
    for (const QChar& c : name) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
            return false;
        }
    }
    bool ok = false;
    const qulonglong value = name.toULongLong(&ok);
    if (!ok || value >= 0xFFFFFFFFull) {
        return false;
    }
    index = static_cast<quint32>(value);
    return true;
}

}  // namespace

PlainScriptValue::PlainScriptValue() {
}

PlainScriptValue::PlainScriptValue(bool value) : _kind(Kind::Bool), _primitive(value) {
}

PlainScriptValue::PlainScriptValue(int value) : _kind(Kind::Number), _primitive(value) {
}

PlainScriptValue::PlainScriptValue(uint value) : _kind(Kind::Number), _primitive(numberVariant(value)) {
}

PlainScriptValue::PlainScriptValue(double value) : _kind(Kind::Number), _primitive(numberVariant(value)) {
}

PlainScriptValue::PlainScriptValue(const QString& value) : _kind(Kind::String), _primitive(value) {
}

PlainScriptValue::PlainScriptValue(const char* value) : _kind(Kind::String), _primitive(QString::fromUtf8(value)) {
}

PlainScriptValue PlainScriptValue::undefinedValue() {
    return PlainScriptValue(Kind::Undefined);
}

PlainScriptValue PlainScriptValue::nullValue() {
    return PlainScriptValue(Kind::Null);
}

PlainScriptValue PlainScriptValue::newObject() {
    PlainScriptValue value(Kind::Object);
    value._container = std::make_shared<Container>();
    return value;
}

PlainScriptValue PlainScriptValue::newArray() {
    PlainScriptValue value(Kind::Array);
    value._container = std::make_shared<Container>();
    value._container->isArray = true;
    return value;
}

// --- properties -------------------------------------------------------------------------------------------------

PlainScriptValue PlainScriptValue::property(const QString& name) const {
    if (!_container) {
        return PlainScriptValue();
    }
    if (_container->isArray) {
        if (name == QLatin1String("length")) {
            return PlainScriptValue(static_cast<uint>(_container->values.size()));
        }
        quint32 index = 0;
        return arrayIndexOf(name, index) ? property(index) : PlainScriptValue();
    }
    const int at = _container->position(name);
    return at >= 0 ? _container->values.at(at) : PlainScriptValue();
}

PlainScriptValue PlainScriptValue::property(quint32 index) const {
    if (!_container) {
        return PlainScriptValue();
    }
    if (!_container->isArray) {
        return property(QString::number(index));
    }
    // an element never set, or removed, is a hole: absent, as QtScript reports it
    return index < static_cast<quint32>(_container->values.size()) ? _container->values.at(index) : PlainScriptValue();
}

void PlainScriptValue::setProperty(const QString& name, const PlainScriptValue& value) {
    if (!_container) {
        return;
    }
    if (_container->isArray) {
        quint32 index = 0;
        if (name == QLatin1String("length")) {
            const quint32 length = value.isValid() ? value.toUInt32() : 0;
            if (length <= LARGEST_ARRAY_LENGTH) {
                _container->values.resize(static_cast<int>(length));
            }
        } else if (arrayIndexOf(name, index)) {
            setProperty(index, value);
        }
        return;  // a named property on an array is not kept
    }
    if (value.isValid()) {
        _container->set(name, value);
    } else {
        _container->remove(name);
    }
}

void PlainScriptValue::setProperty(quint32 index, const PlainScriptValue& value) {
    if (!_container) {
        return;
    }
    if (!_container->isArray) {
        setProperty(QString::number(index), value);
        return;
    }
    if (index >= LARGEST_ARRAY_LENGTH) {
        return;
    }
    const quint32 size = static_cast<quint32>(_container->values.size());
    if (!value.isValid()) {
        if (index < size) {
            _container->values[static_cast<int>(index)] = PlainScriptValue();  // a hole; the length stays
        }
        return;
    }
    if (index >= size) {
        _container->values.resize(static_cast<int>(index) + 1);
    }
    _container->values[static_cast<int>(index)] = value;
}

QStringList PlainScriptValue::propertyNames() const {
    if (!_container) {
        return QStringList();
    }
    if (!_container->isArray) {
        return _container->names;
    }
    QStringList names;
    for (int i = 0; i < _container->values.size(); ++i) {
        if (_container->values.at(i).isValid()) {
            names.append(QString::number(i));
        }
    }
    return names;
}

// --- conversions ------------------------------------------------------------------------------------------------

bool PlainScriptValue::toBool() const {
    switch (_kind) {
        case Kind::Invalid:
        case Kind::Undefined:
        case Kind::Null:
            return false;
        case Kind::Bool:
            return _primitive.toBool();
        case Kind::Number: {
            const double number = _primitive.toDouble();
            return number != 0.0 && !std::isnan(number);
        }
        case Kind::String:
            return !_primitive.toString().isEmpty();
        case Kind::Array:
        case Kind::Object:
            return true;
    }
    return false;
}

double PlainScriptValue::toNumber() const {
    switch (_kind) {
        case Kind::Invalid:
        case Kind::Null:
            return 0.0;
        case Kind::Undefined:
            return std::numeric_limits<double>::quiet_NaN();
        case Kind::Bool:
            return _primitive.toBool() ? 1.0 : 0.0;
        case Kind::Number:
            return _primitive.toDouble();
        case Kind::String:
            return stringToNumber(_primitive.toString());
        case Kind::Array:
            return stringToNumber(toString());  // through its text, as JavaScript does
        case Kind::Object:
            return std::numeric_limits<double>::quiet_NaN();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

double PlainScriptValue::toInteger() const {
    return toIntegerOf(toNumber());
}

qint32 PlainScriptValue::toInt32() const {
    return static_cast<qint32>(toUInt32Of(toNumber()));
}

quint32 PlainScriptValue::toUInt32() const {
    return toUInt32Of(toNumber());
}

quint16 PlainScriptValue::toUInt16() const {
    return static_cast<quint16>(toUInt32Of(toNumber()) & 0xFFFF);
}

QString PlainScriptValue::toString() const {
    switch (_kind) {
        case Kind::Invalid:
            return QString();
        case Kind::Undefined:
            return QStringLiteral("undefined");
        case Kind::Null:
            return QStringLiteral("null");
        case Kind::Bool:
            return _primitive.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case Kind::Number:
            return numberToString(_primitive.toDouble());
        case Kind::String:
            return _primitive.toString();
        case Kind::Array: {
            // the elements' texts joined by commas, a hole, null or undefined as nothing
            QStringList texts;
            for (const PlainScriptValue& element : _container->values) {
                texts.append(element.isNull() || element.isUndefined() ? QString() : element.toString());
            }
            return texts.join(QLatin1Char(','));
        }
        case Kind::Object:
            return QStringLiteral("[object Object]");
    }
    return QString();
}

QVariant PlainScriptValue::toVariant() const {
    switch (_kind) {
        case Kind::Invalid:
        case Kind::Undefined:
        case Kind::Null:
            return QVariant();
        case Kind::Bool:
        case Kind::Number:
        case Kind::String:
            return _primitive;
        case Kind::Array: {
            QVariantList list;
            list.reserve(_container->values.size());
            for (const PlainScriptValue& element : _container->values) {
                list.append(element.toVariant());
            }
            return list;
        }
        case Kind::Object: {
            QVariantMap map;
            for (int i = 0; i < _container->names.size(); ++i) {
                map.insert(_container->names.at(i), _container->values.at(i).toVariant());
            }
            return map;
        }
    }
    return QVariant();
}

QCborValue PlainScriptValue::toCbor() const {
    switch (_kind) {
        case Kind::Invalid:
        case Kind::Undefined:
            return QCborValue(QCborValue::Undefined);
        case Kind::Null:
            return QCborValue(QCborValue::Null);
        case Kind::Bool:
            return QCborValue(_primitive.toBool());
        case Kind::Number:
            if (_primitive.type() == QVariant::Int) {
                return QCborValue(static_cast<qint64>(_primitive.toInt()));
            }
            return QCborValue(_primitive.toDouble());
        case Kind::String:
            return QCborValue(_primitive.toString());
        case Kind::Array: {
            QCborArray array;
            for (const PlainScriptValue& element : _container->values) {
                array.append(element.toCbor());
            }
            return array;
        }
        case Kind::Object: {
            QCborMap map;
            for (int i = 0; i < _container->names.size(); ++i) {
                map.insert(_container->names.at(i), _container->values.at(i).toCbor());
            }
            return map;
        }
    }
    return QCborValue(QCborValue::Undefined);
}

PlainScriptValue PlainScriptValue::fromCbor(const QCborValue& value) {
    if (value.isUuid()) {
        return PlainScriptValue(value.toUuid().toString());  // with braces, as a script sees an id
    }
    if (value.isUrl()) {
        return PlainScriptValue(value.toUrl().toString());
    }
    if (value.isDateTime()) {
        return PlainScriptValue(value.toDateTime().toString(Qt::ISODateWithMs));
    }
    if (value.isTag()) {
        return fromCbor(value.taggedValue());
    }
    if (value.isInteger()) {
        const qint64 integer = value.toInteger();
        if (integer >= std::numeric_limits<qint32>::min() && integer <= std::numeric_limits<qint32>::max()) {
            return PlainScriptValue(static_cast<int>(integer));
        }
        return PlainScriptValue(static_cast<double>(integer));
    }
    if (value.isDouble()) {
        return PlainScriptValue(value.toDouble());
    }
    if (value.isBool()) {
        return PlainScriptValue(value.toBool());
    }
    if (value.isString()) {
        return PlainScriptValue(value.toString());
    }
    if (value.isByteArray()) {
        return PlainScriptValue(QString::fromLatin1(value.toByteArray().toBase64()));
    }
    if (value.isArray()) {
        PlainScriptValue array = newArray();
        quint32 index = 0;
        for (const QCborValue& element : value.toArray()) {
            array.setProperty(index++, fromCbor(element));
        }
        return array;
    }
    if (value.isMap()) {
        PlainScriptValue object = newObject();
        for (const auto& pair : value.toMap()) {
            QString key;
            if (pair.first.isString()) {
                key = pair.first.toString();
            } else if (pair.first.isInteger()) {
                key = QString::number(pair.first.toInteger());
            } else {
                continue;
            }
            object.setProperty(key, fromCbor(pair.second));
        }
        return object;
    }
    if (value.isNull()) {
        return nullValue();
    }
    return undefinedValue();
}

PlainScriptValue PlainScriptValue::fromVariant(const QVariant& value) {
    switch (value.type()) {
        case QVariant::Bool:
            return PlainScriptValue(value.toBool());
        case QVariant::Int:
            return PlainScriptValue(value.toInt());
        case QVariant::Double:
            return PlainScriptValue(value.toDouble());
        case QVariant::String:
        case QVariant::Url:
            return PlainScriptValue(value.toString());
        case QVariant::Map: {
            PlainScriptValue object = newObject();
            const QVariantMap map = value.toMap();
            for (auto it = map.begin(); it != map.end(); ++it) {
                object.setProperty(it.key(), fromVariant(it.value()));
            }
            return object;
        }
        case QVariant::List: {
            PlainScriptValue array = newArray();
            const QVariantList list = value.toList();
            for (int i = 0; i < list.size(); ++i) {
                array.setProperty(static_cast<quint32>(i), fromVariant(list.at(i)));
            }
            return array;
        }
        default:
            if (value.canConvert<float>()) {
                return PlainScriptValue(static_cast<double>(value.toFloat()));
            }
            return PlainScriptValue();
    }
}

namespace {

// The Quote operation of JSON.stringify: the two quote marks, the five named escapes, the rest of the control
// characters as \u00xx, and every other character as it is
void appendQuoted(QString& out, const QString& text) {
    out += QLatin1Char('"');
    for (const QChar c : text) {
        switch (c.unicode()) {
            case '"':
                out += QLatin1String("\\\"");
                break;
            case '\\':
                out += QLatin1String("\\\\");
                break;
            case '\b':
                out += QLatin1String("\\b");
                break;
            case '\f':
                out += QLatin1String("\\f");
                break;
            case '\n':
                out += QLatin1String("\\n");
                break;
            case '\r':
                out += QLatin1String("\\r");
                break;
            case '\t':
                out += QLatin1String("\\t");
                break;
            default:
                if (c.unicode() < 0x20) {
                    out += QLatin1String("\\u") + QString::number(c.unicode(), 16).rightJustified(4, QLatin1Char('0'));
                } else {
                    out += c;
                }
        }
    }
    out += QLatin1Char('"');
}

}  // namespace

QString PlainScriptValue::toJson(const QString& indent) const {
    QString out;
    appendJson(out, indent, QString());
    return out;
}

// JSON.stringify(value, null, indent): `current` is the indentation of the value's own line; an undefined value
// (or an invalid one, a hole) writes nothing, which an object leaves out and an array writes as null
void PlainScriptValue::appendJson(QString& out, const QString& indent, const QString& current) const {
    switch (_kind) {
        case Kind::Invalid:
        case Kind::Undefined:
            return;
        case Kind::Null:
            out += QLatin1String("null");
            return;
        case Kind::Bool:
            out += _primitive.toBool() ? QLatin1String("true") : QLatin1String("false");
            return;
        case Kind::Number: {
            const double number = _primitive.toDouble();
            out += std::isfinite(number) ? numberToString(number) : QStringLiteral("null");
            return;
        }
        case Kind::String:
            appendQuoted(out, _primitive.toString());
            return;
        case Kind::Array: {
            if (_container->values.isEmpty()) {
                out += QLatin1String("[]");
                return;
            }
            const QString inner = current + indent;
            out += QLatin1Char('[');
            for (int i = 0; i < _container->values.size(); ++i) {
                if (i > 0) {
                    out += QLatin1Char(',');
                }
                if (!indent.isEmpty()) {
                    out += QLatin1Char('\n') + inner;
                }
                const PlainScriptValue& element = _container->values.at(i);
                if (element.isValid() && !element.isUndefined()) {
                    element.appendJson(out, indent, inner);
                } else {
                    out += QLatin1String("null");
                }
            }
            if (!indent.isEmpty()) {
                out += QLatin1Char('\n') + current;
            }
            out += QLatin1Char(']');
            return;
        }
        case Kind::Object: {
            const QString inner = current + indent;
            bool any = false;
            for (int i = 0; i < _container->names.size(); ++i) {
                const PlainScriptValue& value = _container->values.at(i);
                if (!value.isValid() || value.isUndefined()) {
                    continue;
                }
                out += any ? QLatin1String(",") : QLatin1String("{");
                any = true;
                if (!indent.isEmpty()) {
                    out += QLatin1Char('\n') + inner;
                }
                appendQuoted(out, _container->names.at(i));
                out += indent.isEmpty() ? QLatin1String(":") : QLatin1String(": ");
                value.appendJson(out, indent, inner);
            }
            if (!any) {
                out += QLatin1String("{}");
                return;
            }
            if (!indent.isEmpty()) {
                out += QLatin1Char('\n') + current;
            }
            out += QLatin1Char('}');
            return;
        }
    }
}

// Number::toString of the language specification: the shortest digits that read back as the number, written as
// an integer up to 10^21, with a decimal point down to 10^-6, and with an exponent beyond
QString PlainScriptValue::numberToString(double value) {
    if (std::isnan(value)) {
        return QStringLiteral("NaN");
    }
    if (value == 0.0) {
        return QStringLiteral("0");
    }
    if (std::isinf(value)) {
        return value < 0 ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
    }
    const QString sign = value < 0 ? QStringLiteral("-") : QString();
    // "d.ddde+xx": the shortest digits and the exponent
    const QString scientific = QString::number(std::fabs(value), 'e', QLocale::FloatingPointShortest);
    const int e = scientific.indexOf(QLatin1Char('e'));
    QString digits = scientific.left(e);
    digits.remove(QLatin1Char('.'));
    while (digits.size() > 1 && digits.endsWith(QLatin1Char('0'))) {
        digits.chop(1);
    }
    const int k = digits.size();                      // the number of digits
    const int n = scientific.mid(e + 1).toInt() + 1;  // the value is 0.digits x 10^n
    QString text;
    if (k <= n && n <= 21) {
        text = digits + QString(n - k, QLatin1Char('0'));
    } else if (0 < n && n <= 21) {
        text = digits.left(n) + QLatin1Char('.') + digits.mid(n);
    } else if (-6 < n && n <= 0) {
        text = QStringLiteral("0.") + QString(-n, QLatin1Char('0')) + digits;
    } else {
        const int exponent = n - 1;
        text = (k == 1 ? digits : digits.left(1) + QLatin1Char('.') + digits.mid(1)) + QLatin1Char('e') +
               (exponent < 0 ? QLatin1Char('-') : QLatin1Char('+')) + QString::number(std::abs(exponent));
    }
    return sign + text;
}
