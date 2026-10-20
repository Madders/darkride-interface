//
//  MetaObjectDescription.h
//  libraries/wasm-scripting/src
//
//  A QMetaObject as a plain record: the methods, signals, properties and enumerators a script can reach, the
//  same surface QtScript's newQObject exposes (public methods and slots, no constructors, deleteLater left out).
//  The record is what the `describe` host function returns to a module (as CBOR), what Script.dumpApi writes
//  (as JSON) and what the SDK generators read. One shape, three uses, so the three never disagree.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_MetaObjectDescription_h
#define hifi_MetaObjectDescription_h

#include <QtCore/QVariantMap>

struct QMetaObject;
class QObject;

namespace MetaObjectDescription {

struct Options {
    /// QObject's own members (objectName, destroyed, objectNameChanged); QtScript exposes them, so the default
    /// is to list them, each marked with its declaring class.
    bool includeQObjectMembers { true };
    /// deleteLater, which QtScript leaves out (ExcludeDeleteLater): a script must not delete the host's objects.
    bool includeDeleteLater { false };
};

/// The record:
///   { "class": name,
///     "methods":    [ { "name", "signature", "index", "kind": "slot" | "invokable", "returns", "declaringClass",
///                       "parameters": [ { "name", "type" } ] } ],
///     "signals":    [ { "name", "signature", "index", "declaringClass", "parameters": [...] } ],
///     "properties": [ { "name", "type", "index", "readable", "writable", "constant", "declaringClass",
///                       "notify"?, "enum"? } ],
///     "enums":      [ { "name", "values": { key: value } } ] }
/// "index" is the member's index in the meta-object (what QMetaObject::method and ::property take).
QVariantMap describe(const QMetaObject* meta, const Options& options = Options());
QVariantMap describe(const QObject* object, const Options& options = Options());

} // namespace MetaObjectDescription

#endif // hifi_MetaObjectDescription_h
