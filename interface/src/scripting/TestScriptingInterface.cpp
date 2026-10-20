//
//  Created by Bradley Austin Davis on 2016/12/12
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "TestScriptingInterface.h"

#include <SpringBones.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>

#include <shared/FileUtils.h>
#include <shared/QtHelpers.h>
#include <DependencyManager.h>
#include <MainWindow.h>
#include <OffscreenUi.h>
#include <StatTracker.h>
#include <Trace.h>

#include "Application.h"
#include <QtGui/QPixmap>

#include "editor/Editor.h"
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
                                        (double)sample.item.x, (double)sample.item.y, (double)sample.item.z });
    }
    return list;
}
