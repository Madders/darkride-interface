//
//  engine-choice-helper.js
//  tests-manual/wasm-scripting
//
//  The script engine-choice.js moves between the two engines: it says which one runs it (QtScript is ES5, so it
//  has no Symbol).
//
//  Distributed under the Apache License, Version 2.0.
//

print("ENGINE-CHOICE helper running on " + (typeof Symbol === "function" ? "WebAssembly" : "QtScript"));
