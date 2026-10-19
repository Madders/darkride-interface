//
//  WasmRuntimeTests.cpp
//  tests/wasm-scripting/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WasmRuntimeTests.h"

#include <thread>

#include <QtCore/QElapsedTimer>

#include <WasmRuntime.h>

QTEST_MAIN(WasmRuntimeTests)

namespace {

const QString ADD_MODULE = R"((module
  (func (export "add") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)))";

std::shared_ptr<WasmRuntime> makeRuntime() {
    WasmRuntime::Settings settings;
    settings.epochTick = std::chrono::milliseconds(5);
    return std::make_shared<WasmRuntime>(settings);
}

// An instantiated instance of a module given as text, or null with a failed assertion.
std::unique_ptr<WasmInstance> makeInstance(std::shared_ptr<WasmRuntime> runtime, const QString& text,
                                           const WasmInstance::Limits& limits) {
    QString error;
    auto module = runtime->compileText(text, error);
    if (!module) {
        qWarning() << "compile failed:" << error;
        return nullptr;
    }
    auto instance = std::make_unique<WasmInstance>(module, limits);
    if (!instance->instantiate(error)) {
        qWarning() << "instantiate failed:" << error;
        return nullptr;
    }
    return instance;
}

std::unique_ptr<WasmInstance> makeInstance(std::shared_ptr<WasmRuntime> runtime, const QString& text) {
    return makeInstance(std::move(runtime), text, WasmInstance::Limits());
}

} // namespace

void WasmRuntimeTests::testCompileAndCall() {
    auto runtime = makeRuntime();
    auto instance = makeInstance(runtime, ADD_MODULE);
    QVERIFY(instance);
    QVERIFY(instance->hasExport("add"));
    QVERIFY(!instance->hasExport("subtract"));
    std::vector<WasmValue> results;
    QString error;
    QVERIFY2(instance->call("add", { int32_t(2), int32_t(3) }, results, error), qPrintable(error));
    QCOMPARE(results.size(), size_t(1));
    QCOMPARE(std::get<int32_t>(results[0]), 5);
    // wrong arity is an error, not a crash
    QVERIFY(!instance->call("add", { int32_t(2) }, results, error));
    QVERIFY(error.contains("takes 2 arguments"));
    QVERIFY(!instance->call("subtract", {}, results, error));
    QVERIFY(error.contains("no exported function"));
}

void WasmRuntimeTests::testCompileErrorIsReported() {
    auto runtime = makeRuntime();
    QString error;
    auto module = runtime->compileText("(module (func (export \"broken\") i32.add))", error);
    QVERIFY(!module);
    QVERIFY(!error.isEmpty());
    error.clear();
    module = runtime->compile(QByteArray("not a module"), error);
    QVERIFY(!module);
    QVERIFY(!error.isEmpty());
}

void WasmRuntimeTests::testTrapIsAnError() {
    auto runtime = makeRuntime();
    auto instance = makeInstance(runtime, R"((module (func (export "boom") unreachable)))");
    QVERIFY(instance);
    std::vector<WasmValue> results;
    QString error;
    QVERIFY(!instance->call("boom", {}, results, error));
    QVERIFY2(error.contains("unreachable"), qPrintable(error));
    // the instance is still usable after a trap
    QVERIFY(instance->hasExport("boom"));
}

void WasmRuntimeTests::testHostFunction() {
    auto runtime = makeRuntime();
    QString error;
    auto module = runtime->compileText(R"((module
      (import "test" "twice" (func $twice (param i32) (result i32)))
      (import "test" "scale" (func $scale (param f64) (result f64)))
      (func (export "callTwice") (param i32) (result i32)
        local.get 0
        call $twice)
      (func (export "callScale") (param f64) (result f64)
        local.get 0
        call $scale)))", error);
    QVERIFY2(module, qPrintable(error));
    WasmInstance instance(module);
    int calls = 0;
    instance.defineHostFunction("test", "twice", { { WasmType::I32 }, { WasmType::I32 } },
        [&calls](WasmInstance&, const std::vector<WasmValue>& args, std::vector<WasmValue>& results) {
            ++calls;
            results.push_back(int32_t(std::get<int32_t>(args[0]) * 2));
            return QString();
        });
    instance.defineHostFunction("test", "scale", { { WasmType::F64 }, { WasmType::F64 } },
        [](WasmInstance&, const std::vector<WasmValue>& args, std::vector<WasmValue>& results) {
            results.push_back(std::get<double>(args[0]) * 1.5);
            return QString();
        });
    QVERIFY2(instance.instantiate(error), qPrintable(error));
    std::vector<WasmValue> results;
    QVERIFY2(instance.call("callTwice", { int32_t(21) }, results, error), qPrintable(error));
    QCOMPARE(std::get<int32_t>(results[0]), 42);
    QCOMPARE(calls, 1);
    QVERIFY2(instance.call("callScale", { 2.0 }, results, error), qPrintable(error));
    QCOMPARE(std::get<double>(results[0]), 3.0);
}

void WasmRuntimeTests::testHostFunctionError() {
    auto runtime = makeRuntime();
    QString error;
    auto module = runtime->compileText(R"((module
      (import "test" "fail" (func $fail))
      (func (export "callFail") call $fail)))", error);
    QVERIFY2(module, qPrintable(error));
    WasmInstance instance(module);
    instance.defineHostFunction("test", "fail", {},
        [](WasmInstance&, const std::vector<WasmValue>&, std::vector<WasmValue>&) {
            return QString("the host said no");
        });
    QVERIFY2(instance.instantiate(error), qPrintable(error));
    std::vector<WasmValue> results;
    QVERIFY(!instance.call("callFail", {}, results, error));
    QVERIFY2(error.contains("the host said no"), qPrintable(error));
}

void WasmRuntimeTests::testInfiniteLoopIsInterrupted() {
    auto runtime = makeRuntime();
    WasmInstance::Limits limits;
    limits.callTimeout = std::chrono::milliseconds(50);
    auto instance = makeInstance(runtime, R"((module (func (export "forever") (loop br 0))))", limits);
    QVERIFY(instance);
    std::vector<WasmValue> results;
    QString error;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(!instance->call("forever", {}, results, error));
    qint64 elapsed = timer.elapsed();
    QVERIFY2(error.contains("longer than its limit"), qPrintable(error));
    QVERIFY2(elapsed < 500, qPrintable(QString("took %1 ms").arg(elapsed)));
    // a call within the limit still works afterwards
    auto quick = makeInstance(runtime, ADD_MODULE, limits);
    QVERIFY(quick);
    QVERIFY2(quick->call("add", { int32_t(1), int32_t(1) }, results, error), qPrintable(error));
}

void WasmRuntimeTests::testRequestStop() {
    auto runtime = makeRuntime();
    WasmInstance::Limits limits;
    limits.callTimeout = std::chrono::milliseconds(10000);
    auto instance = makeInstance(runtime, R"((module (func (export "forever") (loop br 0))))", limits);
    QVERIFY(instance);
    WasmInstance* raw = instance.get();
    std::thread stopper([raw] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        raw->requestStop();
    });
    std::vector<WasmValue> results;
    QString error;
    QElapsedTimer timer;
    timer.start();
    QVERIFY(!instance->call("forever", {}, results, error));
    qint64 elapsed = timer.elapsed();
    stopper.join();
    QVERIFY2(error.contains("stopped"), qPrintable(error));
    QVERIFY2(elapsed < 500, qPrintable(QString("took %1 ms").arg(elapsed)));
}

void WasmRuntimeTests::testMemoryLimit() {
    auto runtime = makeRuntime();
    WasmInstance::Limits limits;
    limits.memoryBytes = 16 * 1024 * 1024;
    // grows one page at a time until growth is refused, and says how many pages it got
    auto instance = makeInstance(runtime, R"((module
      (memory (export "memory") 1)
      (func (export "grow") (result i32)
        (local $grown i32)
        (block $done
          (loop $again
            (br_if $done (i32.eq (memory.grow (i32.const 1)) (i32.const -1)))
            (local.set $grown (i32.add (local.get $grown) (i32.const 1)))
            (br $again)))
        (local.get $grown))))", limits);
    QVERIFY(instance);
    QCOMPARE(instance->memorySize(), size_t(64 * 1024));
    std::vector<WasmValue> results;
    QString error;
    QVERIFY2(instance->call("grow", {}, results, error), qPrintable(error));
    QCOMPARE(std::get<int32_t>(results[0]), 255); // 1 page to start, 256 pages is 16 MB
    QCOMPARE(instance->memorySize(), size_t(16 * 1024 * 1024));
}

void WasmRuntimeTests::testMemoryReadWrite() {
    auto runtime = makeRuntime();
    auto instance = makeInstance(runtime, R"((module
      (memory (export "memory") 1)
      (data (i32.const 8) "hello")
      (func (export "byteAt") (param i32) (result i32)
        local.get 0
        i32.load8_u)))");
    QVERIFY(instance);
    QCOMPARE(instance->readMemory(8, 5), QByteArray("hello"));
    QVERIFY(instance->writeMemory(8, QByteArray("HELLO")));
    QCOMPARE(instance->readMemory(8, 5), QByteArray("HELLO"));
    std::vector<WasmValue> results;
    QString error;
    QVERIFY2(instance->call("byteAt", { int32_t(8) }, results, error), qPrintable(error));
    QCOMPARE(std::get<int32_t>(results[0]), int32_t('H'));
    // out of bounds is refused, not read
    QVERIFY(instance->readMemory(64 * 1024 - 2, 5).isEmpty());
    QVERIFY(!instance->writeMemory(64 * 1024 - 2, QByteArray("12345")));
    QVERIFY(instance->readMemory(64 * 1024, 0).isEmpty());
}

void WasmRuntimeTests::testWasiImports() {
    auto runtime = makeRuntime();
    QString error;
    const QString text = R"((module
      (import "wasi_snapshot_preview1" "proc_exit" (func $exit (param i32)))
      (memory (export "memory") 1)
      (func (export "noop"))))";
    auto module = runtime->compileText(text, error);
    QVERIFY2(module, qPrintable(error));
    // without WASI the import is unknown
    WasmInstance bare(module);
    QVERIFY(!bare.instantiate(error));
    QVERIFY2(error.contains("wasi_snapshot_preview1"), qPrintable(error));
    // with it the module links and runs
    WasmInstance wasi(module);
    wasi.enableWasi();
    QVERIFY2(wasi.instantiate(error), qPrintable(error));
    std::vector<WasmValue> results;
    QVERIFY2(wasi.call("noop", {}, results, error), qPrintable(error));
}

void WasmRuntimeTests::testImportsAndExports() {
    auto runtime = makeRuntime();
    QString error;
    auto module = runtime->compileText(R"((module
      (import "darkride" "log" (func (param i32 i32)))
      (import "darkride" "now" (func (result f64)))
      (memory (export "memory") 1)
      (func (export "script_start"))
      (func (export "alloc") (param i32) (result i32) i32.const 0)))", error);
    QVERIFY2(module, qPrintable(error));
    QCOMPARE(module->imports(), QStringList({ "darkride.log", "darkride.now" }));
    QCOMPARE(module->exports(), QStringList({ "memory", "script_start", "alloc" }));
}

// A host function that calls an export of the module that called it (what the host interface does to have
// the guest allocate the memory for a result), with the module's memory read and written on the way.
void WasmRuntimeTests::testHostFunctionMayCallBackIn() {
    auto runtime = makeRuntime();
    QString error;
    auto module = runtime->compileText(R"((module
      (import "host" "ask" (func $ask (param i32) (result i32)))
      (memory (export "memory") 1)
      (global $next (mut i32) (i32.const 1024))
      (func (export "alloc") (param $size i32) (result i32)
        (local $ptr i32)
        global.get $next
        local.set $ptr
        global.get $next
        local.get $size
        i32.add
        global.set $next
        local.get $ptr)
      (func (export "run") (param i32) (result i32)
        local.get 0
        call $ask)
      (func (export "loop") (result i32)
        (loop $forever br $forever)
        i32.const 0)))", error);
    QVERIFY2(module, qPrintable(error));
    WasmInstance::Limits limits;
    limits.callTimeout = std::chrono::milliseconds(100);
    WasmInstance instance(module, limits);
    int depth = 0;
    instance.defineHostFunction("host", "ask", { { WasmType::I32 }, { WasmType::I32 } },
        [&](WasmInstance& self, const std::vector<WasmValue>& args, std::vector<WasmValue>& results) -> QString {
            ++depth;
            std::vector<WasmValue> out;
            QString callError;
            // the guest allocates, the host writes the answer there and returns the pointer
            if (!self.call("alloc", { int32_t(4) }, out, callError)) {
                return "alloc failed: " + callError;
            }
            int32_t pointer = std::get<int32_t>(out[0]);
            int32_t value = std::get<int32_t>(args[0]) * 2;
            self.writeMemory(uint64_t(pointer), QByteArray(reinterpret_cast<const char*>(&value), 4));
            results.push_back(pointer);
            return QString();
        });
    QVERIFY2(instance.instantiate(error), qPrintable(error));
    std::vector<WasmValue> results;
    QVERIFY2(instance.call("run", { int32_t(21) }, results, error), qPrintable(error));
    int32_t pointer = std::get<int32_t>(results[0]);
    QCOMPARE(pointer, 1024);
    QByteArray answer = instance.readMemory(uint64_t(pointer), 4);
    QCOMPARE(*reinterpret_cast<const int32_t*>(answer.constData()), 42);
    QCOMPARE(depth, 1);
    // a second call allocates after the first
    QVERIFY(instance.call("run", { int32_t(5) }, results, error));
    QCOMPARE(std::get<int32_t>(results[0]), 1028);
    // the time limit still holds after reentrant calls
    QElapsedTimer timer;
    timer.start();
    QVERIFY(!instance.call("loop", {}, results, error));
    QVERIFY(timer.elapsed() < 1000);
    QVERIFY(error.contains("limit"));
}
