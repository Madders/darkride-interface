//
//  WasmScriptEngine.cpp
//  libraries/script-engine/src
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WasmScriptEngine.h"

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMetaEnum>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptValueIterator>

#include <AddressManager.h>
#include <AudioEffectOptions.h>
#include <EntityScriptingInterface.h>
#include <EntityTree.h>
#include <LocationScriptingInterface.h>
#include <NumericalConstants.h>
#include <OctreeConstants.h>
#include <PathUtils.h>
#include <PointerEvent.h>
#include <ResourceCache.h>
#include <ResourceManager.h>
#include <SettingHandle.h>
#include <SharedUtil.h>
#include <shared/LocalFileAccessGate.h>

#include <FunctionTwins.h>
#include <HostInterface.h>
#include <ObjectTable.h>
#include <SignalRelay.h>

#include "BatchLoader.h"
#include "HttpRequestObject.h"
#include "ScriptCache.h"
#include "ScriptEngineLogging.h"
#include "ScriptEngines.h"
#include "WasmTypeConversions.h"
#include "WebSocketObject.h"

namespace {

const QString WASM_SUFFIX = QStringLiteral("wasm");
const QString JS_SUFFIX = QStringLiteral("js");
const int MIN_TIMEOUT_FOR_COARSE_TIMER = 200;  // below this a Qt timer is made precise, as the QtScript engine does
const int MAX_SCRIPT_QUITTING_TIME_MS = 500;
const int MAX_SCRIPT_ENDING_TIME_MS = 3000;  // for the scriptEnding handlers, before the modules are interrupted
const int SHUTDOWN_POLL_MS = 5;
const int MAX_MODULE_ID_LENGTH = 4096;
const int MAX_DEBUG_VALUE_LENGTH = 80;

const QString ENTITY_LOAD_EXPORT = QStringLiteral("entity_load");
const QString ENTITY_UNLOAD_EXPORT = QStringLiteral("entity_unload");
const QString ENTITY_HAS_EXPORT = QStringLiteral("entity_has");
const QString ENTITY_CALL_EXPORT = QStringLiteral("entity_call");

Setting::Handle<bool> runJavaScriptOnWebAssemblySetting { "scripting/runJavaScriptOnWebAssembly", false };

// An instance as a callback target: a callback decoded by the instance's codec calls back through it. It
// outlives the instance as a weak reference in the callbacks; a gone instance answers nothing.
class InstanceCallbackTarget : public CallbackTarget {
public:
    InstanceCallbackTarget(WasmScriptEngine* engine, const std::weak_ptr<WasmScriptEngine::ModuleInstance>& instance) :
        _engine(engine), _instance(instance) {}
    bool invoke(int32_t callback, const QVariantList& arguments, QVariant& result, QString& error) override {
        auto instance = _instance.lock();
        if (!_engine || !instance) {
            error = "the script engine is gone";
            return false;
        }
        if (QThread::currentThread() != _engine->thread()) {
            error = "a callback can only be called for its result on the script's thread";
            return false;
        }
        return _engine->invokeCallback(instance, callback, arguments, result, error);
    }
    void post(int32_t callback, const QVariantList& arguments) override {
        QPointer<WasmScriptEngine> engine = _engine;
        auto instance = _instance;
        if (engine) {
            QMetaObject::invokeMethod(engine, [engine, instance, callback, arguments] {
                auto strong = instance.lock();
                if (engine && strong) {
                    engine->postCallback(strong, callback, arguments);
                }
            }, Qt::QueuedConnection);
        }
    }
    void request(int32_t callback, const QVariantList& arguments, std::function<void(const QVariant&)> onResult) override {
        QPointer<WasmScriptEngine> engine = _engine;
        auto instance = _instance;
        if (engine) {
            QMetaObject::invokeMethod(engine, [engine, instance, callback, arguments, onResult] {
                auto strong = instance.lock();
                QVariant result;
                QString error;
                if (engine && strong) {
                    engine->invokeCallback(strong, callback, arguments, result, error);
                }
                onResult(result);
            }, Qt::QueuedConnection);
        }
    }
private:
    QPointer<WasmScriptEngine> _engine;
    std::weak_ptr<WasmScriptEngine::ModuleInstance> _instance;
};

QString suffixOf(const QString& fileNameOrUrl) {
    QUrl url(fileNameOrUrl);
    QString path = url.isValid() && !url.scheme().isEmpty() ? url.path() : fileNameOrUrl;
    return QFileInfo(path).suffix().toLower();
}

QString errorRecordMessage(const QVariant& value) {
    QVariantMap record = value.toMap().value(HostInterface::ERROR_KEY).toMap();
    QString type = record.value("type").toString();
    QString message = record.value("message").toString();
    return type.isEmpty() ? message : type + ": " + message;
}

} // namespace

WasmScriptEngine::ModuleInstance::~ModuleInstance() {
    host.reset();
    instance.reset();
}

WasmScriptEngine::WasmScriptEngine(Context context, const QString& scriptContents, const QString& fileNameString) :
    ScriptEngine(context, scriptContents, fileNameString),
    _runsJavaScript(!isWasmScript(fileNameString)),
    _isEntitiesEngine(context == ENTITY_CLIENT_SCRIPT || context == ENTITY_SERVER_SCRIPT),
    _objects(new ObjectTable()),
    _relay(new SignalRelay(this)),
    _http(new HttpRequestFactory(this)),
    _sockets(new WebSocketFactory(this)),
    _audioEffectOptions(new AudioEffectOptionsFactory(this)),
    _resourceStates(new ResourceStates(this)) {
    WasmTypeConversions::registerAll();
    if (_isEntitiesEngine) {
        // an entities engine hosts every entity script of the world: more room
        _limits.memoryBytes = int64_t(1024) * 1024 * 1024;
    }
    if (_runsJavaScript) {
        // a JavaScript script may run as long as it did on QtScript, which had no limit; stop() still interrupts it
        _limits.callTimeout = std::chrono::milliseconds(0);
    }
}

WasmScriptEngine::~WasmScriptEngine() {
    stopTimers();
    std::lock_guard<std::mutex> lock(_instancesMutex);
    _entityModules.clear();
    _main.reset();
}

bool WasmScriptEngine::isWasmScript(const QString& fileNameOrUrl) {
    return suffixOf(fileNameOrUrl) == WASM_SUFFIX;
}

bool WasmScriptEngine::isJavaScriptFile(const QString& fileNameOrUrl) {
    return suffixOf(fileNameOrUrl) == JS_SUFFIX;
}

std::shared_ptr<WasmRuntime> WasmScriptEngine::runtime() {
    static std::shared_ptr<WasmRuntime> shared = std::make_shared<WasmRuntime>();
    return shared;
}

bool WasmScriptEngine::runJavaScriptOnWebAssembly() {
    // DARKRIDE_RUN_JS_ON_WASM=1 in the environment turns the setting on for one process (test runs, a server)
    if (qEnvironmentVariableIsSet("DARKRIDE_RUN_JS_ON_WASM")) {
        return qEnvironmentVariable("DARKRIDE_RUN_JS_ON_WASM") == "1";
    }
    return runJavaScriptOnWebAssemblySetting.get();
}

void WasmScriptEngine::setRunJavaScriptOnWebAssembly(bool on) {
    runJavaScriptOnWebAssemblySetting.set(on);
}

namespace {

std::mutex& chosenEnginesMutex() {
    static std::mutex mutex;
    return mutex;
}

QHash<QUrl, bool>& chosenEngines() {
    static QHash<QUrl, bool> chosen;
    return chosen;
}

} // namespace

void WasmScriptEngine::chooseEngineForScript(const QUrl& url, bool onWebAssembly) {
    std::lock_guard<std::mutex> lock(chosenEnginesMutex());
    chosenEngines()[url] = onWebAssembly;
}

bool WasmScriptEngine::engineChosenForScript(const QUrl& url, bool& onWebAssembly) {
    std::lock_guard<std::mutex> lock(chosenEnginesMutex());
    auto found = chosenEngines().constFind(url);
    if (found == chosenEngines().constEnd()) {
        return false;
    }
    onWebAssembly = found.value();
    return true;
}

QString WasmScriptEngine::javascriptGuestPath() {
    QString fromEnvironment = qEnvironmentVariable("DARKRIDE_JS_WASM");
    if (!fromEnvironment.isEmpty()) {
        return fromEnvironment;
    }
    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QStringList candidates {
        applicationDir + "/../Resources/wasm/darkride-js.wasm",  // an application bundle
        applicationDir + "/resources/wasm/darkride-js.wasm"      // a plain executable (the servers, Linux, Windows)
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return candidates.last();
}

bool WasmScriptEngine::hasJavaScriptGuest() {
    const QString path = javascriptGuestPath();
    if (QFileInfo::exists(path)) {
        return true;
    }
    static std::once_flag warned;
    std::call_once(warned, [&path] {
        qCWarning(scriptengine) << "No JavaScript guest module at" << path << "- JavaScript runs on the QtScript engine";
    });
    return false;
}

// The guest is one module for every JavaScript script of the process: compiled once, shared.
std::shared_ptr<WasmModule> WasmScriptEngine::javascriptGuestModule(QString& error) {
    static std::mutex mutex;
    static std::shared_ptr<WasmModule> cached;
    static QString cachedPath;
    std::lock_guard<std::mutex> lock(mutex);
    const QString path = javascriptGuestPath();
    if (cached && cachedPath == path) {
        return cached;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = "the JavaScript guest module is not at " + path;
        return nullptr;
    }
    auto started = usecTimestampNow();
    std::shared_ptr<WasmModule> module = runtime()->compile(file.readAll(), error);
    if (module) {
        qCDebug(scriptengine) << "JavaScript guest" << path << "compiled in" << (usecTimestampNow() - started) / 1000 << "ms";
        cached = module;
        cachedPath = path;
    }
    return module;
}

bool WasmScriptEngine::hasValidScriptSuffix(const QString& scriptFileName) {
    return isWasmScript(scriptFileName) || ScriptEngine::hasValidScriptSuffix(scriptFileName);
}

// --- registrations ---------------------------------------------------------------------------------------------

void WasmScriptEngine::registerGlobalObject(const QString& name, QObject* object) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "registerGlobalObject", Q_ARG(const QString&, name), Q_ARG(QObject*, object));
        return;
    }
    if (!object) {
        qCDebug(scriptengine) << "WasmScriptEngine: global" << name << "is null, not registered";
        return;
    }
    if (_objects->global(name) == ObjectTable::NULL_HANDLE) {
        _objects->registerGlobal(name, object);
    }
}

void WasmScriptEngine::registerGetterSetter(const QString& name, QScriptEngine::FunctionSignature getter,
                                            QScriptEngine::FunctionSignature setter, const QString& parent) {
    Q_UNUSED(setter);
    const QString fullName = parent.isEmpty() ? name : parent + "." + name;
    if (getter == &LocationScriptingInterface::locationGetter) {
        // `location` is the address manager behind a getter; a module gets the object itself
        registerGlobalObject(fullName, DependencyManager::get<AddressManager>().data());
        return;
    }
    qCDebug(scriptengine) << "WasmScriptEngine: the property" << fullName << "is a QtScript getter and setter, not available to a module";
}

void WasmScriptEngine::registerFunction(const QString& name, QScriptEngine::FunctionSignature fun, int numArguments) {
    Q_UNUSED(numArguments);
    // a library may have registered an object that stands in for the function (FunctionTwins)
    FunctionTwins::Factory factory = FunctionTwins::find(reinterpret_cast<const void*>(fun));
    if (factory) {
        QObject* twin = factory();
        if (twin) {
            twin->setParent(this);
            registerGlobalObject(name, twin);
            return;
        }
    }
    qCDebug(scriptengine) << "WasmScriptEngine: the function" << name << "is a QtScript function, not available to a module";
}

void WasmScriptEngine::registerFunction(const QString& parent, const QString& name, QScriptEngine::FunctionSignature fun,
                                        int numArguments) {
    Q_UNUSED(fun);
    Q_UNUSED(numArguments);
    qCDebug(scriptengine) << "WasmScriptEngine: the function" << parent + "." + name << "is a QtScript function, not available to a module";
}

void WasmScriptEngine::registerGlobalValue(const QString& name, const QVariant& value) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, name, value] { registerGlobalValue(name, value); });
        return;
    }
    if (!_globalValues.contains(name)) {
        _globalValues.insert(name, value);
    }
}

void WasmScriptEngine::registerValue(const QString& valueName, QScriptValue value) {
    Q_UNUSED(value);
    qCDebug(scriptengine) << "WasmScriptEngine: the value" << valueName << "is a QtScript value, not available to a module";
}

// --- loading ----------------------------------------------------------------------------------------------------

void WasmScriptEngine::loadURL(const QUrl& scriptURL, bool reload) {
    if (_isRunning) {
        return;
    }
    if (_runsJavaScript) {
        // the text, as the QtScript engine fetches it
        ScriptEngine::loadURL(scriptURL, reload);
        return;
    }
    QUrl url = expandScriptUrl(scriptURL);
    _fileNameString = url.toString();
    _isReloading = reload;
    if (!hasValidScriptSuffix(_fileNameString)) {
        scriptErrorMessage("File extension of file: " + _fileNameString + " is not a currently supported script type");
        emit errorLoadingScript(_fileNameString);
        return;
    }
    const int maxRetries = 0;
    auto scriptCache = DependencyManager::get<ScriptCache>();
    scriptCache->getScriptBytes(url.toString(), [this](const QString& url, const QByteArray& bytes, bool success, const QString& status) {
        qCDebug(scriptengine) << "loadURL (module)" << url << status << QThread::currentThread();
        if (!success || bytes.isEmpty()) {
            scriptErrorMessage("ERROR Loading module (" + status + "):" + url);
            emit errorLoadingScript(_fileNameString);
            return;
        }
        _moduleBytes = bytes;
        emit scriptLoaded(url);
    }, reload, maxRetries);
}

// An agent is handed its script as text by the assignment client; a module needs its bytes, fetched here from
// the script's URL, waited for on this thread.
bool WasmScriptEngine::fetchModuleBytes() {
    QUrl url(_fileNameString);
    if (!url.isValid() || url.scheme().isEmpty()) {
        return false;
    }
    bool done = false;
    bool ok = false;
    QString failure;
    auto scriptCache = DependencyManager::get<ScriptCache>();
    scriptCache->getScriptBytes(url.toString(), [this, &done, &ok, &failure](const QString&, const QByteArray& bytes, bool success, const QString& status) {
        done = true;
        ok = success && !bytes.isEmpty();
        failure = status;
        if (ok) {
            _moduleBytes = bytes;
        }
    }, false, 0);
    while (!done && !isStopping()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (!ok) {
        scriptErrorMessage("ERROR Loading module (" + failure + "):" + _fileNameString);
    }
    return ok;
}

// --- running ----------------------------------------------------------------------------------------------------

void WasmScriptEngine::run() {
    if (QThread::currentThread() != qApp->thread() && _context == Context::CLIENT_SCRIPT) {
        hifi::scripting::setLocalAccessSafeThread(true);
    }
    QSharedPointer<ScriptEngines> scriptEngines(_scriptEngines);
    if (!scriptEngines || scriptEngines->isStopped()) {
        return;
    }
    scriptInfoMessage((_runsJavaScript ? "JavaScript on the WebAssembly script engine starting:" : "WebAssembly script engine starting:") + getFilename());

    if (!_isInitialized) {
        // the QtScript engine's own initialization: it registers the standard globals (through this class's
        // registerGlobalObject) and the QtScript conversions on the carried engine, which the serializer uses
        init();
    }
    adoptScriptGlobals();
    // the network objects XMLHttpRequest and WebSocket are built on in the guest
    registerGlobalObject("Http", _http);
    registerGlobalObject("Sockets", _sockets);
    // two globals the QtScript engine gives as a native constructor and a plain object
    registerGlobalObject("AudioEffectOptions", _audioEffectOptions);
    registerGlobalObject("Resource", _resourceStates);
    // a constant the QtScript engine gives as a number in a variant object; a module gets the number
    registerGlobalValue("TREE_SCALE", QVariant(TREE_SCALE));

    _isRunning = true;
    emit runningStateChanged();

    if (startMain()) {
        loop();
    }
    stopAll();
    // the objects made for the script alone go with it, on this thread
    _objects->releaseOwned();

    emit finished(_fileNameString, qSharedPointerCast<ScriptEngine>(sharedFromThis()));
    hifi::scripting::setLocalAccessSafeThread(false);
    _isRunning = false;
    emit runningStateChanged();
    emit doneRunning();
}

// A few hosts put an object on the QtScript global object directly (MyAvatar: globalObject().setProperty with
// newQObject) rather than through registerGlobalObject. The carried engine received those; the module gets
// them too, under the same names, so it sees the globals a JavaScript script sees.
void WasmScriptEngine::adoptScriptGlobals() {
    QScriptValueIterator it(globalObject());
    int adopted = 0;
    while (it.hasNext()) {
        it.next();
        QScriptValue value = it.value();
        if (!value.isQObject()) {
            continue;
        }
        QObject* object = value.toQObject();
        if (object && _objects->global(it.name()) == ObjectTable::NULL_HANDLE) {
            _objects->registerGlobal(it.name(), object);
            ++adopted;
        }
    }
    qCDebug(scriptengine) << "WasmScriptEngine:" << _objects->globalNames().size() << "globals for the module," << adopted << "adopted from the QtScript global object";
}

// An instance of a module in this engine: its codec (with this engine's table, the serializer and the
// instance as the callback target), its host interface (logging into this script's log, failures stopping
// what they must), instantiated with WASI, its C runtime initialised, its script_start called.
WasmScriptEngine::ModuleInstancePointer WasmScriptEngine::makeInstance(const QString& name, const std::shared_ptr<WasmModule>& module,
                                                                       const WasmInstance::Limits& limits, QString& error) {
    auto instance = std::make_shared<ModuleInstance>();
    instance->name = name;
    instance->module = module;
    instance->codec = std::make_unique<ValueCodec>(_objects.get());
    installSerializer(*instance->codec);
    instance->target = std::make_shared<InstanceCallbackTarget>(this, instance);
    instance->codec->setCallbackTarget(instance->target);
    instance->instance = std::make_unique<WasmInstance>(module, limits);
    instance->host = std::make_unique<HostInterface>(*instance->instance, *_objects, *instance->codec, *_relay);
    instance->host->setLogger([this](int level, const QString& text) {
        switch (level) {
            case HostInterface::Print:
                scriptPrintedMessage(text);
                break;
            case HostInterface::Info:
                scriptInfoMessage(text);
                break;
            case HostInterface::Warning:
                scriptWarningMessage(text);
                break;
            case HostInterface::Error:
                scriptErrorMessage(text);
                break;
            default:
                qCDebug(scriptengine).noquote() << "[" + getFilename() + "]" << text;
                break;
        }
    });
    std::weak_ptr<ModuleInstance> weak = instance;
    instance->host->setFailureHandler([this, weak](const QString& entry, const QString& failure) {
        onModuleFailure(weak.lock(), entry, failure);
    });
    instance->host->install();
    instance->instance->enableWasi();
    if (!instance->instance->instantiate(error)) {
        error = "cannot instantiate " + name + ": " + error;
        return nullptr;
    }
    for (const char* required : { "alloc", "script_start" }) {
        if (!instance->instance->hasExport(required)) {
            error = QString("%1 does not export %2, which a script module must").arg(name, required);
            return nullptr;
        }
    }
    if (instance->instance->hasExport("_initialize")) {
        // a reactor's initializer (its C runtime's constructors), before anything else is called
        std::vector<WasmValue> results;
        ++_callDepth;
        bool initialized = instance->instance->call("_initialize", {}, results, error);
        --_callDepth;
        if (!initialized) {
            error = name + " failed to initialize: " + error;
            return nullptr;
        }
    }
    instance->running = true;
    ++_callDepth;
    bool started = instance->host->callScriptStart(error);
    --_callDepth;
    if (!started) {
        instance->running = false;
        return nullptr;
    }
    return instance;
}

bool WasmScriptEngine::startMain() {
    QString error;
    std::shared_ptr<WasmModule> module;
    if (_runsJavaScript) {
        if (_scriptContents.isEmpty() && !_isEntitiesEngine) {
            scriptErrorMessage("No script to run: " + _fileNameString);
            return false;
        }
        module = javascriptGuestModule(error);
        if (!module) {
            scriptErrorMessage("Cannot run " + getFilename() + " on WebAssembly: " + error);
            return false;
        }
    } else {
        if (_moduleBytes.isEmpty() && !fetchModuleBytes()) {
            scriptErrorMessage("No module to run: " + _fileNameString);
            return false;
        }
        module = runtime()->compile(_moduleBytes, error);
        if (!module) {
            scriptErrorMessage("Cannot compile " + getFilename() + ": " + error);
            return false;
        }
    }
    ModuleInstancePointer main = makeInstance(getFilename(), module, _limits, error);
    if (!main) {
        scriptErrorMessage(error);
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(_instancesMutex);
        _main = main;
    }
    return !_isFinished;
}

void WasmScriptEngine::loop() {
    using clock = std::chrono::steady_clock;
    const std::chrono::microseconds FRAME(USECS_PER_SECOND / SCRIPT_FPS + 1);
    QSharedPointer<ScriptEngines> scriptEngines(_scriptEngines);
    auto entityScriptingInterface = DependencyManager::get<EntityScriptingInterface>();
    clock::time_point startTime = clock::now();
    int thisFrame = 0;
    _lastUpdate = usecTimestampNow();

    while (!_isFinished) {
        // a steady SCRIPT_FPS: the next frame is on the schedule from the start, and a script that fell more
        // than a frame behind starts a new schedule rather than catching up in a burst
        clock::time_point target = startTime + (++thisFrame * FRAME);
        clock::time_point now = clock::now();
        if (now > target + FRAME) {
            startTime = now;
            thisFrame = 0;
            target = now;
        }
        auto sleepFor = std::chrono::duration_cast<std::chrono::milliseconds>(target - now);
        if (!scriptEngines->getBypassScriptThrottling() && sleepFor > std::chrono::milliseconds(0)) {
            // the sleep is an event loop: queued signals, timers and invocations from other threads run here
            QEventLoop eventLoop;
            QTimer timer;
            timer.setSingleShot(true);
            timer.setTimerType(Qt::PreciseTimer);
            connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
            timer.start(int(sleepFor.count()));
            eventLoop.exec();
        } else {
            QCoreApplication::processEvents();
        }
        if (_isFinished) {
            break;
        }
        // entity edits the script queued go out, as in the QtScript engine's loop
        auto packetSender = entityScriptingInterface->getEntityPacketSender();
        if (packetSender->serversExist()) {
            packetSender->releaseQueuedMessages();
            if (!packetSender->isThreaded()) {
                packetSender->process();
            }
        }
        qint64 nowUsecs = usecTimestampNow();
        if (_emitScriptUpdates() && _lastUpdate < nowUsecs) {
            float deltaTime = float(nowUsecs - _lastUpdate) / float(USECS_PER_SECOND);
            auto before = clock::now();
            ++_callDepth;
            emit update(deltaTime);
            --_callDepth;
            _updateMicroseconds += std::chrono::duration<double, std::micro>(clock::now() - before).count();
            ++_frames;
        }
        _lastUpdate = nowUsecs;
        if (thisFrame % SCRIPT_FPS == 0) {
            // about once a second: the objects the script let go of while they were busy, deleted once they are not
            _objects->sweepLingering();
        }
    }
}

void WasmScriptEngine::stopInstance(const ModuleInstancePointer& instance) {
    if (instance && instance->running && instance->host) {
        QString error;
        ++_callDepth;
        instance->host->callScriptStop(error);
        --_callDepth;
    }
    if (instance) {
        instance->running = false;
    }
}

void WasmScriptEngine::stopAll() {
    scriptInfoMessage("WebAssembly script engine stopping:" + getFilename());
    stopTimers();
    // the signal first, while the modules can still answer it; then their own stops; the entity scripts'
    // modules before the main one, which may host them
    ++_callDepth;
    emit scriptEnding();
    --_callDepth;
    for (const ModuleInstancePointer& instance : _entityModules) {
        stopInstance(instance);
    }
    stopInstance(_main);
    _relay->disconnectAll();
    _callbackHandlers.clear();
    _entityHosts.clear();
    {
        std::lock_guard<std::mutex> lock(_instancesMutex);
        _entityModules.clear();
        _main.reset();
    }
}

void WasmScriptEngine::onModuleFailure(const ModuleInstancePointer& instance, const QString& entry, const QString& error) {
    const QString name = instance ? instance->name : getFilename();
    if (error.contains("the script was stopped")) {
        // a stop requested from another thread interrupted a call: what was asked for, not a failure
        qCDebug(scriptengine) << name << "interrupted in" << entry << "by a stop";
        _isStopping = true;
        if (!_isFinished) {
            _isFinished = true;
            emit runningStateChanged();
        }
        return;
    }
    if (instance && instance != _main) {
        // an entity script's module failed: that module stops, the engine and the other scripts run on
        scriptErrorMessage(QString("%1 failed in %2: %3 (its entity scripts are stopped)").arg(name, entry, error));
        instance->running = false;
        return;
    }
    scriptErrorMessage(QString("%1 failed in %2: %3 (the script is stopped)").arg(name, entry, error));
    _isStopping = true;
    if (!_isFinished) {
        _isFinished = true;
        emit runningStateChanged();
    }
}

void WasmScriptEngine::stop(bool marshal) {
    _isStopping = true;
    if (marshal) {
        QMetaObject::invokeMethod(this, "stop");
        return;
    }
    if (!_isFinished) {
        _isFinished = true;
        emit runningStateChanged();
    }
    if (QThread::currentThread() != thread() && _callDepth > 0) {
        // a module is inside a call: make it return now rather than at its own pace
        std::lock_guard<std::mutex> lock(_instancesMutex);
        if (_main && _main->instance) {
            _main->instance->requestStop();
        }
        for (const ModuleInstancePointer& instance : _entityModules) {
            if (instance->instance) {
                instance->instance->requestStop();
            }
        }
    }
}

void WasmScriptEngine::waitTillDoneRunning() {
    stop();
    auto workerThread = thread();
    if (workerThread == QThread::currentThread()) {
        qCWarning(scriptengine) << "WasmScriptEngine::waitTillDoneRunning called on the script's own thread:" << getFilename();
        return;
    }
    if (_isThreaded && workerThread && workerThread->isRunning()) {
        workerThread->quit();
        // While the script stops, its scriptEnding handlers may call objects of this thread with a blocking
        // invocation (a tablet button removed, a menu item): those calls, and only those (no user input, no
        // timers), are served here, so the handlers finish rather than wait on a thread that waits on them.
        auto waitServingCalls = [workerThread](int milliseconds) {
            QElapsedTimer elapsed;
            elapsed.start();
            while (!workerThread->wait(SHUTDOWN_POLL_MS)) {
                QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
                if (elapsed.elapsed() >= milliseconds) {
                    return false;
                }
            }
            return true;
        };
        if (!waitServingCalls(MAX_SCRIPT_ENDING_TIME_MS)) {
            {
                std::lock_guard<std::mutex> lock(_instancesMutex);
                if (_main && _main->instance) {
                    _main->instance->requestStop();
                }
                for (const ModuleInstancePointer& instance : _entityModules) {
                    if (instance->instance) {
                        instance->instance->requestStop();
                    }
                }
            }
            if (!waitServingCalls(MAX_SCRIPT_QUITTING_TIME_MS)) {
                qCWarning(scriptengine) << "WebAssembly script engine did not stop, terminating its thread:" << getFilename();
                workerThread->terminate();
            }
        }
        scriptInfoMessage("Script Engine has stopped:" + getFilename());
    }
}

// --- the serializer: QtScript's conversions for the types that have no QVariant ones yet ----------------------

namespace {

// Each type the serializer converts is logged once per process, so a run shows what still crosses through QtScript.
void noteSerializedType(int type, const char* direction) {
    static std::mutex mutex;
    static QSet<QString> noted;
    const QString key = QString("%1 %2").arg(direction).arg(type);
    std::lock_guard<std::mutex> lock(mutex);
    if (!noted.contains(key)) {
        noted.insert(key);
        qCDebug(scriptengine) << "WasmScriptEngine: a" << QMetaType::typeName(type) << "value was" << direction << "through QtScript's conversion";
    }
}

} // namespace

void WasmScriptEngine::installSerializer(ValueCodec& codec) {
    const int scriptValueType = QMetaType::type("QScriptValue");  // registered by QtScript at run time
    codec.setFallback(
        [this, &codec, scriptValueType](const QVariant& value, QCborValue& out) {
            const int type = value.userType();
            if (type == scriptValueType) {
                return false;
            }
            QScriptValue converted = qScriptValueFromValue_helper(this, type, value.constData());
            if (!converted.isValid()) {
                return false;
            }
            QVariant plain = converted.toVariant();
            if (plain.userType() == type) {
                return false;  // no conversion registered: QtScript wrapped the value as it was
            }
            noteSerializedType(type, "encoded");
            out = codec.toCbor(plain);
            return true;
        },
        [this, &codec, scriptValueType](const QCborValue& value, int targetType, QVariant& out) {
            if (targetType == scriptValueType) {
                return false;
            }
            QVariant plain = codec.fromCbor(value);
            QScriptValue scriptValue = qScriptValueFromValue_helper(this, plain.userType(), plain.constData());
            if (!scriptValue.isValid()) {
                return false;
            }
            void* buffer = QMetaType::create(targetType);
            if (!buffer) {
                return false;
            }
            bool ok = qscriptvalue_cast_helper(scriptValue, targetType, buffer);
            if (ok) {
                noteSerializedType(targetType, "decoded");
                out = QVariant(targetType, buffer);
            }
            QMetaType::destroy(targetType, buffer);
            return ok;
        });
}

// --- timers -------------------------------------------------------------------------------------------------------

int WasmScriptEngine::addTimer(const ScriptCallback& callback, int intervalMS, bool singleShot) {
    QSharedPointer<ScriptEngines> scriptEngines(_scriptEngines);
    if (!scriptEngines || scriptEngines->isStopped()) {
        scriptWarningMessage("Script.setTimeout() while shutting down is ignored... parent script:" + getFilename());
        return 0;
    }
    if (!callback.isValid()) {
        scriptWarningMessage("Script.setTimeout(): no callback given");
        return 0;
    }
    auto timer = new QTimer(this);
    timer->setSingleShot(singleShot);
    if (intervalMS < MIN_TIMEOUT_FOR_COARSE_TIMER) {
        timer->setTimerType(Qt::PreciseTimer);
    }
    const int id = _nextTimer++;
    connect(timer, &QTimer::timeout, this, [this, id] {
        auto found = _timers.find(id);
        if (found == _timers.end()) {
            return;
        }
        Timer entry = found.value();
        if (entry.singleShot) {
            _timers.erase(found);
            entry.timer->deleteLater();
        }
        if (!_isFinished) {
            // the callback knows its instance; it is called on this thread, the engine's
            auto target = entry.callback.target.lock();
            if (target) {
                QVariant result;
                QString error;
                target->invoke(entry.callback.id, QVariantList(), result, error);
            }
        }
    });
    _timers.insert(id, Timer { timer, callback, singleShot });
    timer->start(intervalMS);
    return id;
}

void WasmScriptEngine::removeTimer(int id) {
    auto found = _timers.find(id);
    if (found == _timers.end()) {
        return;
    }
    found.value().timer->stop();
    found.value().timer->deleteLater();
    _timers.erase(found);
}

void WasmScriptEngine::stopTimers() {
    const QList<int> ids = _timers.keys();
    for (int id : ids) {
        removeTimer(id);
    }
}

int WasmScriptEngine::setTimeout(const ScriptCallback& callback, int timeoutMS) {
    return addTimer(callback, timeoutMS, true);
}

int WasmScriptEngine::setInterval(const ScriptCallback& callback, int intervalMS) {
    return addTimer(callback, intervalMS, false);
}

void WasmScriptEngine::clearTimeout(int timer) {
    removeTimer(timer);
}

void WasmScriptEngine::clearInterval(int timer) {
    removeTimer(timer);
}

// --- callbacks called from the host side -----------------------------------------------------------------------------

bool WasmScriptEngine::invokeCallback(const ModuleInstancePointer& instance, int32_t callback, const QVariantList& arguments,
                                      QVariant& result, QString& error) {
    if (!instance || !instance->running || !instance->host || _isFinished) {
        error = "the script is not running";
        return false;
    }
    ++_callDepth;
    bool ok = instance->host->callCallback(callback, arguments, result, error);
    --_callDepth;
    return ok;
}

void WasmScriptEngine::postCallback(const ModuleInstancePointer& instance, int32_t callback, const QVariantList& arguments) {
    if (instance && instance->running && instance->host && !_isFinished) {
        QString error;
        ++_callDepth;
        instance->host->deliverCallback(callback, arguments, error);
        --_callDepth;
    }
}

// --- entity scripts -------------------------------------------------------------------------------------------------

void WasmScriptEngine::loadEntityScript(const EntityItemID& entityID, const QString& entityScript, bool forceRedownload) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "loadEntityScript", Q_ARG(const EntityItemID&, entityID),
                                  Q_ARG(const QString&, entityScript), Q_ARG(bool, forceRedownload));
        return;
    }
    QSharedPointer<ScriptEngines> scriptEngines(_scriptEngines);
    if (isStopping() || !scriptEngines || scriptEngines->isStopped()) {
        return;
    }
    if (!hasEntityScriptDetails(entityID)) {
        updateEntityScriptStatus(entityID, EntityScriptStatus::PENDING, "...pending...");
    }
    EntityScriptDetails newDetails;
    newDetails.scriptText = entityScript;
    newDetails.status = EntityScriptStatus::LOADING;
    newDetails.definingSandboxURL = currentSandboxURL;
    setEntityScriptDetails(entityID, newDetails);

    auto scriptCache = DependencyManager::get<ScriptCache>();
    QWeakPointer<BaseScriptEngine> weakRef(sharedFromThis());
    if (isWasmScript(entityScript)) {
        scriptCache->getScriptBytes(entityScript, [this, weakRef, entityID](const QString& url, const QByteArray& bytes, bool success, const QString& status) {
            QSharedPointer<BaseScriptEngine> strongRef(weakRef);
            if (!strongRef || isStopping()) {
                return;
            }
            executeOnScriptThread([=] { entityScriptReady(entityID, url, bytes, success, status); });
        }, forceRedownload);
    } else {
        scriptCache->getScriptContents(entityScript, [this, weakRef, entityID](const QString& url, const QString& contents, bool isURL, bool success, const QString& status) {
            QSharedPointer<BaseScriptEngine> strongRef(weakRef);
            if (!strongRef || isStopping()) {
                return;
            }
            // an embedded script (the text itself as the "URL") is named as the QtScript engine names it
            const QString name = isURL ? url : QString("about:EmbeddedEntityScript");
            executeOnScriptThread([=] { entityScriptReady(entityID, name, contents, success, status); });
        }, forceRedownload);
    }
}

// The script's module or text has arrived: a .wasm entity script gets (or shares) the instance of its module;
// a JavaScript one goes to the guest with its source. Then the contract's entity_load, preload, and the
// status for the entity's record.
void WasmScriptEngine::entityScriptReady(const EntityItemID& entityID, const QString& url, const QVariant& sourceOrBytes,
                                         bool success, const QString& status) {
    if (isStopping() || !hasEntityScriptDetails(entityID)) {
        return;
    }
    EntityScriptDetails newDetails;
    {
        EntityScriptDetails current;
        getEntityScriptDetails(entityID, current);
        newDetails.scriptText = current.scriptText;
    }
    auto setError = [&](const QString& errorInfo, EntityScriptStatus failure) {
        newDetails.errorInfo = errorInfo;
        newDetails.status = failure;
        setEntityScriptDetails(entityID, newDetails);
        scriptErrorMessage(QString("entity script %1 (%2): %3").arg(url, entityID.toString(), errorInfo));
    };
    if (!success) {
        setError("Failed to load script (" + status + ")", EntityScriptStatus::ERROR_LOADING_SCRIPT);
        return;
    }
    ModuleInstancePointer host;
    QVariantMap payload;
    payload["entity"] = entityID.toString();
    payload["url"] = url;
    QString error;
    if (sourceOrBytes.type() == QVariant::ByteArray) {
        host = entityModuleInstance(url, sourceOrBytes.toByteArray(), error);
        if (!host) {
            setError(error, EntityScriptStatus::ERROR_RUNNING_SCRIPT);
            return;
        }
    } else {
        host = _main;
        payload["source"] = sourceOrBytes.toString();
    }
    if (!host || !host->running || !host->host || !host->instance->hasExport(ENTITY_LOAD_EXPORT)) {
        setError(QString("%1 does not host entity scripts (no %2 export)").arg(host ? host->name : getFilename(), ENTITY_LOAD_EXPORT),
                 EntityScriptStatus::ERROR_RUNNING_SCRIPT);
        return;
    }
    QVariant result;
    ++_callDepth;
    bool ok = host->host->callWithPayload(ENTITY_LOAD_EXPORT, payload, result, error);
    --_callDepth;
    if (!ok) {
        setError(error, EntityScriptStatus::ERROR_RUNNING_SCRIPT);
        return;
    }
    if (HostInterface::isErrorRecord(result)) {
        setError(errorRecordMessage(result), EntityScriptStatus::ERROR_RUNNING_SCRIPT);
        return;
    }
    newDetails.status = EntityScriptStatus::RUNNING;
    newDetails.definingSandboxURL = currentSandboxURL.isEmpty() ? QUrl(url) : currentSandboxURL;
    setEntityScriptDetails(entityID, newDetails);
    _entityHosts[entityID] = host;
    callEntityScriptMethod(entityID, "preload");
    emit entityScriptPreloadFinished(entityID);
}

// The instance of a .wasm entity script's module: one per URL, shared by the entities that run it.
WasmScriptEngine::ModuleInstancePointer WasmScriptEngine::entityModuleInstance(const QString& url, const QByteArray& bytes, QString& error) {
    auto found = _entityModules.find(url);
    if (found != _entityModules.end() && found.value()->running) {
        return found.value();
    }
    std::shared_ptr<WasmModule> module = runtime()->compile(bytes, error);
    if (!module) {
        error = "cannot compile " + url + ": " + error;
        return nullptr;
    }
    WasmInstance::Limits limits;  // a module's own limits, not the engine's (the guest hosts many scripts; a module hosts its own)
    ModuleInstancePointer instance = makeInstance(url, module, limits, error);
    if (!instance) {
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(_instancesMutex);
        _entityModules[url] = instance;
    }
    return instance;
}

WasmScriptEngine::ModuleInstancePointer WasmScriptEngine::instanceForEntity(const EntityItemID& entityID) const {
    auto found = _entityHosts.find(entityID);
    return found == _entityHosts.end() ? nullptr : found.value().lock();
}

void WasmScriptEngine::unloadEntityScript(const EntityItemID& entityID, bool shouldRemoveFromMap) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "unloadEntityScript", Q_ARG(const EntityItemID&, entityID), Q_ARG(bool, shouldRemoveFromMap));
        return;
    }
    EntityScriptDetails oldDetails;
    if (!getEntityScriptDetails(entityID, oldDetails)) {
        return;
    }
    if (isEntityScriptRunning(entityID)) {
        callEntityScriptMethod(entityID, "unload");
        ModuleInstancePointer host = instanceForEntity(entityID);
        if (host && host->running && host->host && host->instance->hasExport(ENTITY_UNLOAD_EXPORT)) {
            QVariantMap payload;
            payload["entity"] = entityID.toString();
            QVariant result;
            QString error;
            ++_callDepth;
            host->host->callWithPayload(ENTITY_UNLOAD_EXPORT, payload, result, error);
            --_callDepth;
        }
    }
    _entityHosts.remove(entityID);
    if (shouldRemoveFromMap) {
        {
            QWriteLocker locker { &_entityScriptsLock };
            _entityScripts.remove(entityID);
        }
        emit entityScriptDetailsUpdated();
    } else if (oldDetails.status != EntityScriptStatus::UNLOADED) {
        EntityScriptDetails newDetails;
        newDetails.status = EntityScriptStatus::UNLOADED;
        newDetails.lastModified = QDateTime::currentMSecsSinceEpoch();
        newDetails.scriptText = oldDetails.scriptText;
        setEntityScriptDetails(entityID, newDetails);
    }
}

void WasmScriptEngine::unloadAllEntityScripts(bool blockingCall) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "unloadAllEntityScripts", blockingCall ? Qt::BlockingQueuedConnection : Qt::QueuedConnection,
                                  Q_ARG(bool, blockingCall));
        return;
    }
    QList<EntityItemID> keys;
    {
        QReadLocker locker { &_entityScriptsLock };
        keys = _entityScripts.keys();
    }
    for (const EntityItemID& entityID : keys) {
        unloadEntityScript(entityID);
    }
    {
        QWriteLocker locker { &_entityScriptsLock };
        _entityScripts.clear();
    }
    _entityHosts.clear();
    emit entityScriptDetailsUpdated();
}

// A method of an entity's script, through the contract's entity_call: the arguments are the entity's id and
// what the caller gives (a list of strings, a pointer event, another entity and a collision).
void WasmScriptEngine::callEntityMethod(const EntityItemID& entityID, const QString& methodName, const QVariantList& arguments,
                                        const QUuid& remoteCallerID) {
    if (!isEntityScriptRunning(entityID)) {
        return;
    }
    ModuleInstancePointer host = instanceForEntity(entityID);
    if (!host || !host->running || !host->host || !host->instance->hasExport(ENTITY_CALL_EXPORT)) {
        return;
    }
    QVariantMap payload;
    payload["entity"] = entityID.toString();
    payload["method"] = methodName;
    payload["args"] = arguments;
    if (!remoteCallerID.isNull()) {
        payload["remoteCallerID"] = remoteCallerID.toString();
    }
    QVariant result;
    QString error;
    ++_callDepth;
    host->host->callWithPayload(ENTITY_CALL_EXPORT, payload, result, error);
    --_callDepth;
}

void WasmScriptEngine::callEntityScriptMethod(const EntityItemID& entityID, const QString& methodName, const QStringList& params,
                                              const QUuid& remoteCallerID) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "callEntityScriptMethod", Q_ARG(const EntityItemID&, entityID), Q_ARG(const QString&, methodName),
                                  Q_ARG(const QStringList&, params), Q_ARG(const QUuid&, remoteCallerID));
        return;
    }
    callEntityMethod(entityID, methodName, QVariantList { params }, remoteCallerID);
}

void WasmScriptEngine::callEntityScriptMethod(const EntityItemID& entityID, const QString& methodName, const PointerEvent& event) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "callEntityScriptMethod", Q_ARG(const EntityItemID&, entityID), Q_ARG(const QString&, methodName),
                                  Q_ARG(const PointerEvent&, event));
        return;
    }
    callEntityMethod(entityID, methodName, QVariantList { QVariant::fromValue(event) });
}

void WasmScriptEngine::callEntityScriptMethod(const EntityItemID& entityID, const QString& methodName, const EntityItemID& otherID,
                                              const Collision& collision) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "callEntityScriptMethod", Q_ARG(const EntityItemID&, entityID), Q_ARG(const QString&, methodName),
                                  Q_ARG(const EntityItemID&, otherID), Q_ARG(const Collision&, collision));
        return;
    }
    callEntityMethod(entityID, methodName, QVariantList { QVariant::fromValue(otherID), QVariant::fromValue(collision) });
}

// --- Script.addEventHandler for a module: the entity signals forwarded to guest functions ---------------------------

void WasmScriptEngine::connectEntityEventSignals() {
    if (_entityEventSignalsConnected) {
        return;
    }
    _entityEventSignalsConnected = true;
    auto entities = DependencyManager::get<EntityScriptingInterface>();
    connect(entities.data(), &EntityScriptingInterface::deletingEntity, this, [this](const EntityItemID& entityID) {
        _callbackHandlers.remove(entityID);
    });
    auto single = [this](const QString& eventName) {
        return [this, eventName](const EntityItemID& entityID) {
            forwardEntityEvent(entityID, eventName, QVariantList { QVariant::fromValue(entityID) });
        };
    };
    auto pointer = [this](const QString& eventName) {
        return [this, eventName](const EntityItemID& entityID, const PointerEvent& event) {
            if (!EntityTree::areEntityClicksCaptured()) {
                forwardEntityEvent(entityID, eventName, QVariantList { QVariant::fromValue(entityID), QVariant::fromValue(event) });
            }
        };
    };
    connect(entities.data(), &EntityScriptingInterface::enterEntity, this, single("enterEntity"));
    connect(entities.data(), &EntityScriptingInterface::leaveEntity, this, single("leaveEntity"));
    connect(entities.data(), &EntityScriptingInterface::mousePressOnEntity, this, pointer("mousePressOnEntity"));
    connect(entities.data(), &EntityScriptingInterface::mouseMoveOnEntity, this, pointer("mouseMoveOnEntity"));
    connect(entities.data(), &EntityScriptingInterface::mouseReleaseOnEntity, this, pointer("mouseReleaseOnEntity"));
    connect(entities.data(), &EntityScriptingInterface::clickDownOnEntity, this, pointer("clickDownOnEntity"));
    connect(entities.data(), &EntityScriptingInterface::holdingClickOnEntity, this, pointer("holdingClickOnEntity"));
    connect(entities.data(), &EntityScriptingInterface::clickReleaseOnEntity, this, pointer("clickReleaseOnEntity"));
    connect(entities.data(), &EntityScriptingInterface::hoverEnterEntity, this, pointer("hoverEnterEntity"));
    connect(entities.data(), &EntityScriptingInterface::hoverOverEntity, this, pointer("hoverOverEntity"));
    connect(entities.data(), &EntityScriptingInterface::hoverLeaveEntity, this, pointer("hoverLeaveEntity"));
    connect(entities.data(), &EntityScriptingInterface::collisionWithEntity, this,
            [this](const EntityItemID& idA, const EntityItemID& idB, const Collision& collision) {
                forwardEntityEvent(idA, "collisionWithEntity",
                                   QVariantList { QVariant::fromValue(idA), QVariant::fromValue(idB), QVariant::fromValue(collision) });
            });
}

void WasmScriptEngine::forwardEntityEvent(const EntityItemID& entityID, const QString& eventName, const QVariantList& arguments) {
    auto handlers = _callbackHandlers.find(entityID);
    if (handlers == _callbackHandlers.end()) {
        return;
    }
    const QList<ScriptCallback> callbacks = handlers.value().value(eventName);
    for (const ScriptCallback& callback : callbacks) {
        auto target = callback.target.lock();
        if (target) {
            QVariant result;
            QString error;
            target->invoke(callback.id, arguments, result, error);
        }
    }
}

void WasmScriptEngine::addEventHandler(const EntityItemID& entityID, const QString& eventName, const ScriptCallback& handler) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "addEventHandler", Q_ARG(const EntityItemID&, entityID), Q_ARG(const QString&, eventName),
                                  Q_ARG(const ScriptCallback&, handler));
        return;
    }
    if (!handler.isValid()) {
        return;
    }
    connectEntityEventSignals();
    _callbackHandlers[entityID][eventName].append(handler);
}

void WasmScriptEngine::removeEventHandler(const EntityItemID& entityID, const QString& eventName, const ScriptCallback& handler) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "removeEventHandler", Q_ARG(const EntityItemID&, entityID), Q_ARG(const QString&, eventName),
                                  Q_ARG(const ScriptCallback&, handler));
        return;
    }
    auto handlers = _callbackHandlers.find(entityID);
    if (handlers == _callbackHandlers.end()) {
        return;
    }
    QList<ScriptCallback>& list = handlers.value()[eventName];
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id == handler.id && list[i].target.lock() == handler.target.lock()) {
            list.removeAt(i);
            return;
        }
    }
}

// --- the Script API the JavaScript shim builds on -------------------------------------------------------------------

QVariantMap WasmScriptEngine::scriptSource() const {
    QVariantMap program;
    program["fileName"] = _fileNameString;
    program["source"] = _scriptContents;
    return program;
}

// The hosts put a few plain objects of numbers on the QtScript global object (DriveKeys, PickType); the carried
// engine has them, and a guest gets them as values. Objects with functions or objects inside are not values.
QVariantMap WasmScriptEngine::scriptGlobalValues() const {
    // registerGlobalValue's, then the plain objects hosts put on the QtScript global object themselves
    QVariantMap values = _globalValues;
    QScriptValueIterator it(globalObject());
    while (it.hasNext()) {
        it.next();
        QScriptValue value = it.value();
        if (!value.isObject() || value.isFunction() || value.isQObject() || value.isArray() || value.isError() ||
            value.isRegExp() || value.isDate()) {
            continue;
        }
        QVariantMap members;
        bool plain = true;
        QScriptValueIterator member(value);
        while (member.hasNext()) {
            member.next();
            QScriptValue v = member.value();
            if (v.isNumber() || v.isString() || v.isBool()) {
                members[member.name()] = v.toVariant();
            } else {
                plain = false;
                break;
            }
        }
        if (plain && !members.isEmpty() && !values.contains(it.name())) {
            values[it.name()] = members;
        }
    }
    return values;
}

QString WasmScriptEngine::resolvePathFrom(const QString& path, const QString& parentUrl) const {
    QUrl url(path);
    // a full URL already, or a Windows path like "c:/"
    if (path.startsWith("/") || url.scheme().length() == 1) {
        url = QUrl::fromLocalFile(path);
    }
    if (!url.isRelative()) {
        return expandScriptUrl(url).toString();
    }
    QUrl parentURL(parentUrl);
    if (parentURL.isRelative()) {
        parentURL = QUrl(_fileNameString);
        if (parentURL.isRelative() && url.path().contains("/")) {
            parentURL = QUrl::fromLocalFile(_fileNameString);
        }
    }
    return expandScriptUrl(parentURL.resolved(url)).toString();
}

QVariantMap WasmScriptEngine::resolveInclude(const QString& file, const QString& parentUrl, const QString& sandboxUrl) const {
    QVariantMap result;
    const QUrl sandbox = sandboxUrl.isEmpty() ? currentSandboxURL : QUrl(sandboxUrl);
    QString normalized = DependencyManager::get<ResourceManager>()->normalizeURL(file);
    QUrl thisURL;
    bool isStandardLibrary = false;
    if (normalized.startsWith("/~/")) {
        thisURL = expandScriptUrl(QUrl::fromLocalFile(expandScriptPath(normalized)));
        if (!PathUtils::defaultScriptsLocation().isParentOf(thisURL)) {
            result["reason"] = "outside of standard libraries";
            return result;
        }
        isStandardLibrary = true;
    } else {
        thisURL = QUrl(resolvePathFrom(normalized, parentUrl));
    }
    bool disallowOutsideFiles = thisURL.isLocalFile() && !isStandardLibrary && !sandbox.isLocalFile();
    if (disallowOutsideFiles && !PathUtils::isDescendantOf(thisURL, sandbox)) {
        result["reason"] = "outside of original entity script " + sandbox.toString();
        return result;
    }
    result["url"] = thisURL.toString();
    return result;
}

// Script.require.resolve's rules, as the QtScript engine's _requireResolve has them, with the error returned
// in the record (the guest throws it).
QVariantMap WasmScriptEngine::resolveModule(const QString& moduleId, const QString& relativeTo, const QString& parentUrl,
                                            const QString& sandboxUrl) const {
    QVariantMap result;
    const QUrl sandbox = sandboxUrl.isEmpty() ? currentSandboxURL : QUrl(sandboxUrl);
    QString displayId = moduleId.length() > MAX_DEBUG_VALUE_LENGTH ? moduleId.left(MAX_DEBUG_VALUE_LENGTH) + "..." : moduleId;
    auto fail = [&](const QString& details, const QString& type = QString("Error")) {
        result["error"] = QString("Cannot find module '%1' (%2)").arg(displayId, details);
        result["type"] = type;
        return result;
    };
    QUrl url(moduleId);
    int idLength = url.toString().length();
    if (idLength < 1 || idLength > MAX_MODULE_ID_LENGTH) {
        return fail(QString("rejecting invalid module id size (%1 chars [1,%2])").arg(idLength).arg(MAX_MODULE_ID_LENGTH), "RangeError");
    }
    QRegularExpression qualified("^\\w+:|^/|^[.]{1,2}(/|$)");
    QUrl defaultScriptsLoc = PathUtils::defaultScriptsLocation();
    if (!relativeTo.isEmpty()) {
        url = QUrl(relativeTo).resolved(moduleId);
        url = QUrl(resolvePathFrom(url.toString(), parentUrl));
    } else if (qualified.match(moduleId).hasMatch()) {
        url = QUrl(resolvePathFrom(moduleId, parentUrl));
    } else {
        // a "system" module, by bare name
        url = defaultScriptsLoc;
        url.setPath(QString("%1/modules/%2.js").arg(defaultScriptsLoc.path(), moduleId));
        if (!QFileInfo(url.toLocalFile()).isFile()) {
            if (!moduleId.contains("./")) {
                QUrl unanchoredUrl(resolvePathFrom("./" + moduleId, parentUrl));
                if (QFileInfo(unanchoredUrl.toLocalFile()).isFile()) {
                    return fail(QString("relative module ids must be anchored; use './%1' instead").arg(moduleId));
                }
            }
            return fail("system module not found");
        }
    }
    if (url.isRelative()) {
        return fail("could not resolve module id");
    }
    if (url.isLocalFile()) {
        QFileInfo file(url.toLocalFile());
        QUrl canonical = url;
        if (file.exists()) {
            canonical.setPath(file.canonicalFilePath());
        }
        bool disallowOutsideFiles = !defaultScriptsLoc.isParentOf(canonical) && !sandbox.isLocalFile();
        if (disallowOutsideFiles && !PathUtils::isDescendantOf(canonical, sandbox)) {
            return fail(QString("path '%1' outside of origin script '%2' '%3'")
                            .arg(PathUtils::stripFilename(url), PathUtils::stripFilename(sandbox), canonical.toString()));
        }
        if (!file.exists()) {
            return fail("path does not exist: " + url.toLocalFile());
        }
        if (!file.isFile()) {
            return fail("path is not a file: " + url.toLocalFile());
        }
    }
    result["url"] = url.toString();
    return result;
}

// The text of a script through the cache, waited for on this thread (an event loop, as the QtScript engine's
// include and require wait), with a stop seen in time.
QVariantMap WasmScriptEngine::fetchScript(const QString& url, bool forceDownload) {
    QVariantMap result;
    result["url"] = url;
    result["success"] = false;
    if (isStopping()) {
        result["status"] = "Stopped";
        return result;
    }
    auto scriptCache = DependencyManager::get<ScriptCache>();
    if (forceDownload) {
        scriptCache->deleteScript(url);
    }
    BatchLoader* loader = new BatchLoader(QList<QUrl>({ QUrl(url) }));
    connect(loader, &BatchLoader::finished, this, [this, url, &result](const QMap<QUrl, QString>& data, const QMap<QUrl, QString>& status) {
        if (isStopping()) {
            result["status"] = "Stopped";
            return;
        }
        QString contents = data[QUrl(url)];
        result["status"] = status[QUrl(url)];
        result["success"] = !contents.isNull() && ScriptCache::isSuccessStatus(status[QUrl(url)]);
        result["contents"] = contents;
    });
    connect(this, &QObject::destroyed, loader, &QObject::deleteLater);
    const int MAX_RETRIES = 1;
    loader->start(MAX_RETRIES);
    if (!loader->isFinished()) {
        QEventLoop loop;
        QTimer monitor;
        connect(loader, &BatchLoader::finished, &loop, &QEventLoop::quit);
        connect(&monitor, &QTimer::timeout, this, [this, &loop] {
            if (isStopping()) {
                loop.exit(-1);
            }
        });
        monitor.start(500);
        loop.exec();
    }
    loader->deleteLater();
    return result;
}

bool WasmScriptEngine::dumpApi(const QString& path, const QVariantMap& globals) {
    QJsonObject root;
    root["context"] = getContext();
    root["type"] = getTypeAsString();
    root["fileName"] = _fileNameString;
    root["engine"] = _runsJavaScript ? "JavaScript on WebAssembly" : "WebAssembly";
    root["globals"] = QJsonObject::fromVariantMap(globals);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        scriptWarningMessage("Script.dumpApi: cannot write " + path);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// --- statistics -----------------------------------------------------------------------------------------------------

QVariantMap WasmScriptEngine::statistics() const {
    QVariantMap stats;
    stats["frames"] = qulonglong(_frames);
    stats["updateMeanMicroseconds"] = _frames ? _updateMicroseconds / double(_frames) : 0.0;
    stats["timers"] = _timers.size();
    stats["objects"] = _objects->size();
    stats["connections"] = _relay->connectionCount();
    stats["entityModules"] = _entityModules.size();
    stats["entityScripts"] = _entityHosts.size();
    if (_main && _main->host) {
        const auto& host = _main->host->statistics();
        stats["calls"] = qulonglong(host.calls);
        stats["gets"] = qulonglong(host.gets);
        stats["sets"] = qulonglong(host.sets);
        stats["signalsDelivered"] = qulonglong(host.signalsDelivered);
        stats["callbacksDelivered"] = qulonglong(host.callbacksDelivered);
        stats["bytesToGuest"] = qulonglong(host.bytesToGuest);
        stats["bytesFromGuest"] = qulonglong(host.bytesFromGuest);
    }
    if (_main && _main->instance) {
        stats["memoryBytes"] = qulonglong(_main->instance->memorySize());
    }
    return stats;
}

// --- the globals the QtScript engine gives as a native constructor or a plain object ------------------------------

AudioEffectOptionsFactory::AudioEffectOptionsFactory(QObject* parent) : QObject(parent) {
    // Audio.setReverbOptions takes a const AudioEffectOptions*: the host interface finds it by this name
    qRegisterMetaType<AudioEffectOptions*>();
}

QObject* AudioEffectOptionsFactory::create(const QVariantMap& properties) {
    auto isNumber = [](const QVariant& value) {
        switch (value.userType()) {
            case QMetaType::Int:
            case QMetaType::UInt:
            case QMetaType::LongLong:
            case QMetaType::ULongLong:
            case QMetaType::Float:
            case QMetaType::Double:
                return true;
            default:
                return false;
        }
    };
    // as the QtScript constructor reads its argument: the defaults, then each known option that is a number
    auto options = new AudioEffectOptions();
    options->setParent(this);
    const QMetaObject* meta = options->metaObject();
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        const QByteArray name = it.key().toUtf8();
        if (meta->indexOfProperty(name.constData()) >= 0 && isNumber(it.value())) {
            options->setProperty(name.constData(), it.value());
        }
    }
    return options;
}

QVariantMap ResourceStates::getState() const {
    // the keys and values of ScriptableResource::State, as the QtScript engine's Resource.State has them
    QVariantMap states;
    const QMetaEnum metaEnum = QMetaEnum::fromType<ScriptableResource::State>();
    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        states[QString::fromUtf8(metaEnum.key(i))] = metaEnum.value(i);
    }
    return states;
}
