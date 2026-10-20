//
//  FunctionTwins.h
//  libraries/wasm-scripting/src
//
//  A few globals of the QtScript engine are native functions rather than objects (the constructors of the
//  QML windows, for instance), registered by a function pointer. A module cannot call such a function;
//  what it can reach is an object. This registry maps a function pointer to a factory of the object that
//  stands in for it on the WebAssembly engine, so that the library owning the function (which cannot see
//  the engine) registers its twin at load time, and the engine, given the pointer at registration, makes the
//  twin and registers it under the same name.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_FunctionTwins_h
#define hifi_FunctionTwins_h

#include <functional>

class QObject;

namespace FunctionTwins {

using Factory = std::function<QObject*()>;

/// Registers the twin's factory for a function, by the function's address. Safe from any thread.
void add(const void* function, Factory factory);
/// The twin's factory for a function, or an empty function when there is none.
Factory find(const void* function);

} // namespace FunctionTwins

#endif // hifi_FunctionTwins_h
