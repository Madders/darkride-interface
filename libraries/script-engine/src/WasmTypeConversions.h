//
//  WasmTypeConversions.h
//  libraries/script-engine/src
//
//  The conversions the WebAssembly script engine's codec (ValueCodec, libraries/wasm-scripting) uses for the
//  types the script API carries that have only QtScript conversions: the event types, the entity and avatar
//  types, the controller types, the audio and mesh types. Each gives a module the shape the QtScript conversion
//  gives a script, member for member and in the same order, and reads one back as the QtScript conversion
//  reads it; the QtScript original is named beside each. An object the QtScript engine hands over with
//  ScriptOwnership (an audio injector, a sound, an avatar's data) is owned by the engine's object table.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_WasmTypeConversions_h
#define hifi_WasmTypeConversions_h

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVector>

#include <AnimationCache.h>

namespace WasmTypeConversions {

/// Registers the conversions with the codec, once per process. Safe from any thread.
void registerAll();

}

/// An animation resource as a module sees it (AnimationCache.getAnimation): what the QtScript engine shows through
/// AnimationObject, the prototype it gives an AnimationPointer. Read when asked, so a script that fetched the
/// animation before it loaded sees its frames once it has.
class AnimationWrapper : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList jointNames READ getJointNames)
    Q_PROPERTY(QVector<HFMAnimationFrame> frames READ getFrames)
public:
    explicit AnimationWrapper(const AnimationPointer& animation) : _animation(animation) {}

    Q_INVOKABLE QStringList getJointNames() const { return _animation ? _animation->getJointNames() : QStringList(); }
    Q_INVOKABLE QVector<HFMAnimationFrame> getFrames() const { return _animation ? _animation->getFrames() : QVector<HFMAnimationFrame>(); }

private:
    AnimationPointer _animation;
};

#endif // hifi_WasmTypeConversions_h
