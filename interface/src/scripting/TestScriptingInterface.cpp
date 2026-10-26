//
//  Created by Bradley Austin Davis on 2016/12/12
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "TestScriptingInterface.h"

#include <MeshPartPayload.h>

#include <SpringBones.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>

#include <shared/FileUtils.h>
#include <shared/QtHelpers.h>
#include <DependencyManager.h>
#include <MainWindow.h>
#include <OffscreenUi.h>
#include <QtQuick/QQuickItem>
#include <StatTracker.h>
#include <Trace.h>

#include "Application.h"
#include <QtGui/QPixmap>
#include <plugins/DisplayPlugin.h>
#include <display-plugins/DisplayLink.h>
#include <display-plugins/OpenGLDisplayPlugin.h>

#include "editor/Editor.h"
#include "ui/Skins.h"
#include "graphics/GraphicsEngine.h"
#include <EntityTreeRenderer.h>
#include <RenderableEntityItem.h>

Q_LOGGING_CATEGORY(trace_test, "trace.test")

TestScriptingInterface* TestScriptingInterface::getInstance() {
    static TestScriptingInterface sharedInstance;
    return &sharedInstance;
}

void TestScriptingInterface::quit() {
    qApp->quit();
}

void TestScriptingInterface::waitForTextureIdle() {
    waitForCondition(0, []()->bool {
        return (0 == gpu::Context::getTexturePendingGPUTransferCount());
    });
}

void TestScriptingInterface::waitForDownloadIdle() {
    waitForCondition(0, []()->bool {
        return (0 == ResourceCache::getLoadingRequestCount()) && (0 == ResourceCache::getPendingRequestCount());
    });
}

void TestScriptingInterface::waitForProcessingIdle() {
    auto statTracker = DependencyManager::get<StatTracker>();
    waitForCondition(0, [statTracker]()->bool {
        return (0 == statTracker->getStat("Processing").toInt() && 0 == statTracker->getStat("PendingProcessing").toInt());
    });
}

void TestScriptingInterface::waitIdle() {
    // Initial wait for some incoming work
    QThread::sleep(1);
    waitForDownloadIdle();
    waitForProcessingIdle();
    waitForTextureIdle();
}

bool TestScriptingInterface::loadTestScene(QString scene) {
    if (QThread::currentThread() != thread()) {
        bool result;
        BLOCKING_INVOKE_METHOD(this, "loadTestScene", Q_RETURN_ARG(bool, result), Q_ARG(QString, scene));
        return result;
    }

    static const QString TEST_ROOT = "https://raw.githubusercontent.com/highfidelity/hifi_tests/master/";
    static const QString TEST_BINARY_ROOT = "https://hifi-public.s3.amazonaws.com/test_scene_data/";
    static const QString TEST_SCRIPTS_ROOT = TEST_ROOT + "scripts/";
    static const QString TEST_SCENES_ROOT = TEST_ROOT + "scenes/";
    
    DependencyManager::get<ResourceManager>()->setUrlPrefixOverride("atp:/", TEST_BINARY_ROOT + scene + ".atp/");
    auto tree = qApp->getEntities()->getTree();
    auto treeIsClient = tree->getIsClient();
    // Force the tree to accept the load regardless of permissions
    tree->setIsClient(false);
    auto result = tree->readFromURL(TEST_SCENES_ROOT + scene + ".json");
    tree->setIsClient(treeIsClient);
    return result;
}

bool TestScriptingInterface::startTracing(QString logrules) {
    if (!logrules.isEmpty()) {
        QLoggingCategory::setFilterRules(logrules);
    }

    if (!DependencyManager::isSet<tracing::Tracer>()) {
        return false;
    }

    DependencyManager::get<tracing::Tracer>()->startTracing();
    return true;
}

bool TestScriptingInterface::stopTracing(QString filename) {
    if (!DependencyManager::isSet<tracing::Tracer>()) {
        return false;
    }

    auto tracer = DependencyManager::get<tracing::Tracer>();
    tracer->stopTracing();
    tracer->serialize(filename);
    return true;
}

void TestScriptingInterface::clear() {
    qApp->postLambdaEvent([] {
        qApp->getEntities()->clear();
    });
}

bool TestScriptingInterface::waitForConnection(qint64 maxWaitMs) {
    // Wait for any previous connection to die
    QThread::sleep(1);
    return waitForCondition(maxWaitMs, []()->bool {
        return DependencyManager::get<NodeList>()->getDomainHandler().isConnected();
    });
}

void TestScriptingInterface::wait(int milliseconds) {
    QThread::msleep(milliseconds);
}

bool TestScriptingInterface::waitForCondition(qint64 maxWaitMs, std::function<bool()> condition) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (!condition()) {
        if (maxWaitMs > 0 && elapsed.elapsed() > maxWaitMs) {
            return false;
        }
        QThread::msleep(1);
    }
    return condition();
}

void TestScriptingInterface::startTraceEvent(QString name) {
    tracing::traceEvent(trace_test(), name, tracing::DurationBegin, "");
}

void TestScriptingInterface::endTraceEvent(QString name) {
    tracing::traceEvent(trace_test(), name, tracing::DurationEnd);
}

void TestScriptingInterface::savePhysicsSimulationStats(QString originalPath) {
    QString path = FileUtils::replaceDateTimeTokens(originalPath);
    path = FileUtils::computeDocumentPath(path);
    if (!FileUtils::canCreateFile(path)) {
        return;
    }
    qApp->saveNextPhysicsStats(path);
}

void TestScriptingInterface::profileRange(const QString& name, QScriptValue fn) {
    PROFILE_RANGE(script, name);
    fn.call();
}

void TestScriptingInterface::clearCaches() {
	qApp->reloadResourceCaches();
}

// Writes a JSON object from javascript to a file
void TestScriptingInterface::saveObject(QVariant variant, const QString& filename) {
    if (_testResultsLocation.isNull()) {
        return;
    }

    QJsonDocument jsonDocument;
    jsonDocument = QJsonDocument::fromVariant(variant);
    if (jsonDocument.isNull()) {
        return;
    }

    QByteArray jsonData = jsonDocument.toJson();

    // Append trailing slash if needed
    if (_testResultsLocation.right(1) != "/") {
        _testResultsLocation += "/";
    }

    QString filepath = QDir::cleanPath(_testResultsLocation + filename);
    QFile file(filepath);

    file.open(QFile::WriteOnly);
    file.write(jsonData);
    file.close();
}

void TestScriptingInterface::showMaximized() {
    qApp->getWindow()->showMaximized();
}

void TestScriptingInterface::setOtherAvatarsReplicaCount(int count) {
    qApp->setOtherAvatarsReplicaCount(count);
}

int TestScriptingInterface::getOtherAvatarsReplicaCount() {
    return qApp->getOtherAvatarsReplicaCount();
}

void TestScriptingInterface::setMinimumGPUTextureMemStabilityCount(int count) {
    QMetaObject::invokeMethod(qApp, "setMinimumGPUTextureMemStabilityCount", Qt::DirectConnection, Q_ARG(int, count));
}

void TestScriptingInterface::setSpringBoneLockstep(bool on) {
    springbones::Solver::setLockstep(on);
}

void TestScriptingInterface::advanceSpringBones(float seconds) {
    springbones::Solver::advance(seconds);
}

void TestScriptingInterface::advanceSpringBones(float seconds, const glm::vec3& avatarMove) {
    springbones::Solver::advance(seconds, avatarMove);
}

int TestScriptingInterface::getSpringBoneSteps() {
    return (int)springbones::Solver::getStepCount();
}

float TestScriptingInterface::getSpringBonePendingTime() {
    return springbones::Solver::getPendingTime();
}

int TestScriptingInterface::getSpringBoneConsumed() {
    return (int)springbones::Solver::getConsumedCount();
}

float TestScriptingInterface::getSpringBoneAlpha() {
    return springbones::Solver::getLastAlpha();
}

int TestScriptingInterface::getSpringBoneUpdates() {
    return (int)springbones::Solver::getUpdateCount();
}

float TestScriptingInterface::getGameLoopRate() {
    return qApp->getGameLoopRate();
}

float TestScriptingInterface::getRenderLoopRate() {
    return qApp->getRenderLoopRate();
}

bool TestScriptingInterface::isTextureLoadingComplete() {
    bool result;
    QMetaObject::invokeMethod(qApp, "gpuTextureMemSizeStable", Qt::DirectConnection, Q_RETURN_ARG(bool, result));
    return result;
}

void TestScriptingInterface::selectInEditor(const QStringList& ids) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "selectInEditor", Q_ARG(QStringList, ids));
        return;
    }
    QList<QUuid> uuids;
    for (const QString& id : ids) {
        uuids << QUuid(id);
    }
    DependencyManager::get<Editor>()->select(uuids);
}

void TestScriptingInterface::frameEditorSelection() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "frameEditorSelection");
        return;
    }
    DependencyManager::get<Editor>()->frameSelection();
}

bool TestScriptingInterface::saveInspectorPicture(const QString& path) {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "saveInspectorPicture", Q_RETURN_ARG(bool, result), Q_ARG(QString, path));
        return result;
    }
    QPixmap picture = DependencyManager::get<Editor>()->inspectorPicture();
    return !picture.isNull() && picture.save(path);
}

void TestScriptingInterface::setPresentLoad(int microseconds) {
    OpenGLDisplayPlugin::setPresentLoad(microseconds);
}

float TestScriptingInterface::getPresentPeriod() {
    return qApp->getPresentPeriod();
}

void TestScriptingInterface::holdPresents(int milliseconds) {
    OpenGLDisplayPlugin::holdPresents(milliseconds);
}

void TestScriptingInterface::chooseTrackPoint(int index) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "chooseTrackPoint", Q_ARG(int, index));
        return;
    }
    DependencyManager::get<Editor>()->chooseTrackPoint(index, false);
}

int TestScriptingInterface::chosenTrackPoint() {
    if (QThread::currentThread() != thread()) {
        int result = -1;
        BLOCKING_INVOKE_METHOD(this, "chosenTrackPoint", Q_RETURN_ARG(int, result));
        return result;
    }
    return DependencyManager::get<Editor>()->chosenTrackPoint();
}

void TestScriptingInterface::insertTrackPoint(float arcLength) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "insertTrackPoint", Q_ARG(float, arcLength));
        return;
    }
    DependencyManager::get<Editor>()->insertTrackPoint(arcLength);
}

void TestScriptingInterface::deleteTrackPoint() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "deleteTrackPoint");
        return;
    }
    DependencyManager::get<Editor>()->deleteTrackPoint();
}

void TestScriptingInterface::setBuildMode(bool on) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "setBuildMode", Q_ARG(bool, on));
        return;
    }
    DependencyManager::get<Editor>()->setActive(on);
}

QVariantMap TestScriptingInterface::getHierarchyCounts() {
    if (QThread::currentThread() != thread()) {
        QVariantMap result;
        BLOCKING_INVOKE_METHOD(this, "getHierarchyCounts", Q_RETURN_ARG(QVariantMap, result));
        return result;
    }
    return DependencyManager::get<Editor>()->hierarchyCounts();
}

void TestScriptingInterface::clickHierarchyRow(const QUuid& id) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "clickHierarchyRow", Q_ARG(QUuid, id));
        return;
    }
    DependencyManager::get<Editor>()->clickHierarchyRow(id);
}

void TestScriptingInterface::commitInspectorEdit(const QString& key, const QVariant& value) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "commitInspectorEdit", Q_ARG(QString, key), Q_ARG(QVariant, value));
        return;
    }
    DependencyManager::get<Editor>()->testInspectorEdit(key, value);
}

void TestScriptingInterface::editorUndo() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "editorUndo");
        return;
    }
    DependencyManager::get<Editor>()->undo();
}

QString TestScriptingInterface::hierarchyRowColor(const QUuid& id) {
    if (QThread::currentThread() != thread()) {
        QString result;
        BLOCKING_INVOKE_METHOD(this, "hierarchyRowColor", Q_RETURN_ARG(QString, result), Q_ARG(QUuid, id));
        return result;
    }
    QColor color = DependencyManager::get<Editor>()->hierarchyRowColor(id);
    return color.isValid() ? color.name() : QString();
}

QString TestScriptingInterface::setSkin(const QString& id) {
    if (QThread::currentThread() != thread()) {
        QString result;
        BLOCKING_INVOKE_METHOD(this, "setSkin", Q_RETURN_ARG(QString, result), Q_ARG(QString, id));
        return result;
    }
    return DependencyManager::get<Skins>()->setCurrent(id);
}

bool TestScriptingInterface::editorImportEntities(const QString& url) {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "editorImportEntities", Q_RETURN_ARG(bool, result), Q_ARG(QString, url));
        return result;
    }
    return DependencyManager::get<Editor>()->importEntities(url);
}

int TestScriptingInterface::editorExportSelection(const QString& path) {
    if (QThread::currentThread() != thread()) {
        int result = -1;
        BLOCKING_INVOKE_METHOD(this, "editorExportSelection", Q_RETURN_ARG(int, result), Q_ARG(QString, path));
        return result;
    }
    return DependencyManager::get<Editor>()->exportSelectionTo(path);
}

QString TestScriptingInterface::editorLoadProblem(const QUuid& id) {
    if (QThread::currentThread() != thread()) {
        QString result;
        BLOCKING_INVOKE_METHOD(this, "editorLoadProblem", Q_RETURN_ARG(QString, result), Q_ARG(QUuid, id));
        return result;
    }
    return DependencyManager::get<Editor>()->loadProblem(id);
}

void TestScriptingInterface::showSignInScreen() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "showSignInScreen");
        return;
    }
    // as the logout path does (ApplicationStartup.cpp, AccountManager::logoutComplete)
    qApp->getSignInGate().pauseUntilLoginDetermined();
    qApp->getSignInGate().showLoginScreen();
}

// a QML panel's item by its objectName, in the desktop (the panels are its windows)
static QObject* findPanelItem(const QString& objectName) {
    auto offscreenUi = DependencyManager::get<OffscreenUi>();
    QQuickItem* desktop = offscreenUi ? offscreenUi->getDesktop() : nullptr;
    return desktop ? desktop->findChild<QObject*>(objectName) : nullptr;
}

bool TestScriptingInterface::setPanelText(const QString& objectName, const QString& text) {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "setPanelText", Q_RETURN_ARG(bool, result), Q_ARG(QString, objectName), Q_ARG(QString, text));
        return result;
    }
    QObject* field = findPanelItem(objectName);
    return field && field->setProperty("text", text);
}

bool TestScriptingInterface::acceptPanelText(const QString& objectName) {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "acceptPanelText", Q_RETURN_ARG(bool, result), Q_ARG(QString, objectName));
        return result;
    }
    QObject* field = findPanelItem(objectName);
    return field && QMetaObject::invokeMethod(field, "accepted");
}

void TestScriptingInterface::editorCreate(const QString& key) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "editorCreate", Q_ARG(QString, key));
        return;
    }
    DependencyManager::get<Editor>()->create(key);
}

bool TestScriptingInterface::saveWindowPicture(const QString& path) {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "saveWindowPicture", Q_RETURN_ARG(bool, result), Q_ARG(QString, path));
        return result;
    }
    auto window = qApp->getWindow();
    if (!window) {
        return false;
    }
    QPixmap picture = window->grab();
    return !picture.isNull() && picture.save(path);
}

void TestScriptingInterface::dumpDraws(int frames) {
    MeshPartPayload::dumpDrawFrames.store(std::max(0, frames));
}

bool TestScriptingInterface::traceRenderedEntity(const QUuid& entityID) {
    if (QThread::currentThread() != thread()) {
        bool result = false;
        BLOCKING_INVOKE_METHOD(this, "traceRenderedEntity", Q_RETURN_ARG(bool, result), Q_ARG(QUuid, entityID));
        return result;
    }
    render::ItemID item = render::Item::INVALID_ITEM_ID;
    if (!entityID.isNull()) {
        auto renderable = qApp->getEntities()->renderableForEntityId(entityID);
        if (!renderable) {
            return false;
        }
        item = renderable->getRenderItemID();
    }
    qApp->getGraphicsEngine().setRenderTraceItem(item);
    return render::Item::isValidID(item);
}

QVariantList TestScriptingInterface::takeRenderTrace() {
    QVariantList list;
    for (const auto& sample : qApp->getGraphicsEngine().takeRenderTrace()) {
        list << QVariant(QVariantList { (double)sample.time * 1.0e-3, (double)sample.camera.x, (double)sample.camera.y, (double)sample.camera.z,
                                        (double)sample.item.x, (double)sample.item.y, (double)sample.item.z, (double)sample.frameIndex });
    }
    return list;
}

void TestScriptingInterface::tracePresents(bool on) {
    auto displayPlugin = qApp->getActiveDisplayPlugin();
    if (displayPlugin) {
        displayPlugin->setPresentTracing(on);
    }
}

QVariantList TestScriptingInterface::takePresentTrace() {
    QVariantList list;
    auto displayPlugin = qApp->getActiveDisplayPlugin();
    if (displayPlugin) {
        for (const auto& sample : displayPlugin->takePresentTrace()) {
            list << QVariant(QVariantList { (double)sample.time * 1.0e-3, (double)sample.frameIndex, sample.newFrame ? 1.0 : 0.0,
                                            (double)sample.presentUsecs * 1.0e-3, (double)sample.swapUsecs * 1.0e-3, (double)sample.gpuMs });
        }
    }
    return list;
}

void TestScriptingInterface::traceRefreshes(bool on) {
    DisplayLink::instance().setTracing(on);
}

QVariantList TestScriptingInterface::takeRefreshTrace() {
    QVariantList list;
    for (uint64_t time : DisplayLink::instance().takeTrace()) {
        list << QVariant((double)time * 1.0e-3);
    }
    return list;
}

float TestScriptingInterface::getDisplayRefreshRate() {
    auto window = qApp->getWindow();
    if (!window || !window->windowHandle() || !window->windowHandle()->screen()) {
        return 0.0f;
    }
    return (float)window->windowHandle()->screen()->refreshRate();
}

bool TestScriptingInterface::isVsyncEnabled() {
    auto displayPlugin = qApp->getActiveDisplayPlugin();
    return displayPlugin && displayPlugin->isVsyncEnabled();
}

void TestScriptingInterface::editorDrag(const glm::vec3& delta, float yawDegrees, float rollDegrees, bool begin, bool final) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "editorDrag", Q_ARG(glm::vec3, delta), Q_ARG(float, yawDegrees), Q_ARG(float, rollDegrees),
                               Q_ARG(bool, begin), Q_ARG(bool, final));
        return;
    }
    // a rotate ring's drag when any rotation is asked for: the kind is the handle's, not the size of the first step
    // (the first step of a slow drag is a degree or two)
    bool rotate = yawDegrees != 0.0f || rollDegrees != 0.0f;
    glm::quat rotation = glm::quat(glm::radians(glm::vec3(0.0f, yawDegrees, rollDegrees)));
    DependencyManager::get<Editor>()->dragSelectionForTest(delta, rotation, rotate, begin, final);
}
