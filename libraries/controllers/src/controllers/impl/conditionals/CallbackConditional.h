//
//  CallbackConditional.h
//  libraries/controllers/src/controllers/impl/conditionals
//
//  A route condition that is a function of a script on the WebAssembly engine: what ScriptConditional is for
//  a QtScript function. The function is asked for a truth value on the engine's thread; the mapper reads the
//  last answer.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Controllers_CallbackConditional_h
#define hifi_Controllers_CallbackConditional_h

#include <QtCore/QObject>

#include <ScriptCallback.h>

#include "../Conditional.h"

namespace controller {

class CallbackConditional : public QObject, public Conditional {
    Q_OBJECT;
public:
    CallbackConditional(const ScriptCallback& callback) : _callback(callback) {}
    virtual bool satisfied() override;

protected:
    Q_INVOKABLE void updateValue();

private:
    ScriptCallback _callback;
    bool _lastValue { false };
};

} // namespace controller

#endif // hifi_Controllers_CallbackConditional_h
