//
//  SnapshotRequests.cpp
//  interface/src
//
//  The snapshots asked of the next frame's render (see SnapshotRequests.h). The code of Application's
//  addSnapshotOperator, takeSnapshotOperators, takeSnapshot, takeSecondaryCameraSnapshot,
//  takeSecondaryCamera360Snapshot and shareSnapshot.
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#include "SnapshotRequests.h"

#include "Application.h"
#include "scripting/TestScriptingInterface.h"
#include "scripting/WindowScriptingInterface.h"
#include "ui/Snapshot.h"
#include "ui/SnapshotAnimated.h"

void SnapshotRequests::add(const Operator& snapshotOperator) {
    std::lock_guard<std::mutex> lock(_mutex);
    _operators.push(snapshotOperator);
    _hasPrimary = _hasPrimary || std::get<2>(snapshotOperator);
}

bool SnapshotRequests::take(std::queue<Operator>& snapshotOperators) {
    std::lock_guard<std::mutex> lock(_mutex);
    bool hasPrimarySnapshot = _hasPrimary;
    _hasPrimary = false;
    _operators.swap(snapshotOperators);
    return hasPrimarySnapshot;
}

void SnapshotRequests::takeSnapshot(bool notify, bool includeAnimated, float aspectRatio, const QString& filename) {
    add(std::make_tuple([notify, includeAnimated, aspectRatio, filename](const QImage& snapshot) {
        qApp->postLambdaEvent([snapshot, notify, includeAnimated, aspectRatio, filename] {
            QString path = DependencyManager::get<Snapshot>()->saveSnapshot(snapshot, filename, TestScriptingInterface::getInstance()->getTestResultsLocation());

            // If we're not doing an animated snapshot as well...
            if (!includeAnimated) {
                if (!path.isEmpty()) {
                    // Tell the dependency manager that the capture of the still snapshot has taken place.
                    emit DependencyManager::get<WindowScriptingInterface>()->stillSnapshotTaken(path, notify);
                }
            } else if (!SnapshotAnimated::isAlreadyTakingSnapshotAnimated()) {
                // Get an animated GIF snapshot and save it
                SnapshotAnimated::saveSnapshotAnimated(path, aspectRatio, DependencyManager::get<WindowScriptingInterface>());
            }
        });
    }, aspectRatio, true));
}

void SnapshotRequests::takeSecondaryCameraSnapshot(const bool& notify, const QString& filename) {
    add(std::make_tuple([notify, filename](const QImage& snapshot) {
        qApp->postLambdaEvent([snapshot, notify, filename] {
            QString snapshotPath = DependencyManager::get<Snapshot>()->saveSnapshot(snapshot, filename, TestScriptingInterface::getInstance()->getTestResultsLocation());

            emit DependencyManager::get<WindowScriptingInterface>()->stillSnapshotTaken(snapshotPath, notify);
        });
    }, 0.0f, false));
}

void SnapshotRequests::takeSecondaryCamera360Snapshot(const glm::vec3& cameraPosition, const bool& cubemapOutputFormat, const bool& notify, const QString& filename) {
    qApp->postLambdaEvent([notify, filename, cubemapOutputFormat, cameraPosition] {
        DependencyManager::get<Snapshot>()->save360Snapshot(cameraPosition, cubemapOutputFormat, notify, filename);
    });
}

void SnapshotRequests::shareSnapshot(const QString& path, const QUrl& href) {
    qApp->postLambdaEvent([path, href] {
        // not much to do here, everything is done in snapshot code...
        DependencyManager::get<Snapshot>()->uploadSnapshot(path, href);
    });
}
