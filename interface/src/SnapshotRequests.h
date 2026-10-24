//
//  SnapshotRequests.h
//  interface/src
//
//  The snapshots asked of the next frame's render, and the ways to ask. A request is an operator: what to do with
//  the image, the aspect ratio wanted, and whether it is the primary camera's view. add() queues one from any
//  thread; the render takes them all at once with take(), which also says whether one is the primary camera's.
//  takeSnapshot() queues the still (and starts the animated one), takeSecondaryCameraSnapshot() and
//  takeSecondaryCamera360Snapshot() the secondary camera's, and shareSnapshot() uploads one; the Window scripting
//  interface calls these for scripts. Application owns one, reached by getSnapshotRequests(); these were
//  Application's addSnapshotOperator, takeSnapshotOperators, takeSnapshot, takeSecondaryCameraSnapshot,
//  takeSecondaryCamera360Snapshot and shareSnapshot (ARCHITECTURE.md §5.4 step 4, the second owner class).
//
//  Copyright 2026 Darkride.
//
//  Distributed under the MIT License.
//

#pragma once
#ifndef hifi_SnapshotRequests_h
#define hifi_SnapshotRequests_h

#include <functional>
#include <mutex>
#include <queue>
#include <tuple>

#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtGui/QImage>

#include <glm/glm.hpp>

class SnapshotRequests {
public:
    // what to do with the image, the aspect ratio wanted (0 for the display's), whether it is the primary camera's view
    using Operator = std::tuple<std::function<void(const QImage&)>, float, bool>;

    void add(const Operator& snapshotOperator);
    bool take(std::queue<Operator>& snapshotOperators);   // all of them, for this frame; true when one is the primary camera's

    void takeSnapshot(bool notify, bool includeAnimated = false, float aspectRatio = 0.0f, const QString& filename = QString());
    void takeSecondaryCameraSnapshot(const bool& notify, const QString& filename = QString());
    void takeSecondaryCamera360Snapshot(const glm::vec3& cameraPosition, const bool& cubemapOutputFormat, const bool& notify,
                                        const QString& filename = QString());
    void shareSnapshot(const QString& filename, const QUrl& href = QUrl(""));

private:
    std::mutex _mutex;
    std::queue<Operator> _operators;
    bool _hasPrimary{ false };
};

#endif // hifi_SnapshotRequests_h
