//
//  ScriptCallback.h
//  libraries/shared/src
//
//  A function of a script on the WebAssembly script engine, held by the id the script's module gave it and
//  by the engine (the target) that can call it: what crosses the host interface as {"@callback": id}. A
//  value type, so that any library (the controllers, the animation rig, the entities) can keep one and call
//  the function back without knowing the engine. The codec (libraries/wasm-scripting) makes them.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptCallback_h
#define hifi_ScriptCallback_h

#include <cstdint>
#include <functional>
#include <memory>

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVariantList>

/// What calls a guest's function back: the engine implements it. `invoke` runs the function and returns its
/// result; it must be called on the engine's thread. `post` queues the call from any thread and returns at
/// once. `request` queues the call from any thread and hands its result to `onResult` on the engine's thread.
class CallbackTarget {
public:
    virtual ~CallbackTarget() = default;
    virtual bool invoke(int32_t callback, const QVariantList& arguments, QVariant& result, QString& error) = 0;
    virtual void post(int32_t callback, const QVariantList& arguments) = 0;
    virtual void request(int32_t callback, const QVariantList& arguments, std::function<void(const QVariant&)> onResult) = 0;
};

struct ScriptCallback {
    int32_t id { 0 };
    std::weak_ptr<CallbackTarget> target;
    bool isValid() const { return id != 0; }
    bool operator==(const ScriptCallback& other) const { return id == other.id; }
    /// Calls the function and returns its result (on the engine's thread); an invalid QVariant when the
    /// engine is gone or the call failed.
    QVariant call(const QVariantList& arguments = QVariantList()) const;
    /// Queues the call, from any thread.
    void post(const QVariantList& arguments = QVariantList()) const;
    /// Queues the call from any thread; `onResult` gets its result on the engine's thread.
    void request(const QVariantList& arguments, std::function<void(const QVariant&)> onResult) const;
};
Q_DECLARE_METATYPE(ScriptCallback)

#endif // hifi_ScriptCallback_h
