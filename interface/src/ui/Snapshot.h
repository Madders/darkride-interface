//
//  Snapshot.h
//  interface/src/ui
//
//  Created by Stojce Slavkovski on 1/26/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Snapshot_h
#define hifi_Snapshot_h

#include <glm/glm.hpp>

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStandardPaths>
#include <QUrl>
#include <QTimer>
#include <QtGui/QImage>

#include <SettingHandle.h>
#include <DependencyManager.h>

class QFile;
class QTemporaryFile;

class SnapshotMetaData {
public:

    QUrl getURL() { return _URL; }
    void setURL(QUrl URL) { _URL = URL; }

private:
    QUrl _URL;
};


/**jsdoc
 * The <code>Snapshot</code> API provides access to the path that snapshots are saved to. This path is that provided in 
 * Settings &gt; General &gt; Snapshots. Snapshots may be taken using <code>Window</code> API functions such as 
 * {@link Window.takeSnapshot}. A snapshot taken with the default name, as Snap takes them, goes to the user's files
 * (My Files), in their Snapshots folder, when someone is signed in; it is saved to this path when no one is signed in or
 * the upload fails.
 *
 * @namespace Snapshot
 * 
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 */

class Snapshot : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY
public:
    Snapshot();
    QString saveSnapshot(QImage image, const QString& filename, const QString& pathname = QString());
    void save360Snapshot(const glm::vec3& cameraPosition,
                         const bool& cubemapOutputFormat,
                         const bool& notify,
                         const QString& filename);
    QTemporaryFile* saveTempSnapshot(QImage image);
    SnapshotMetaData* parseSnapshotData(QString snapshotPath);

    Setting::Handle<QString> _snapshotsLocation{ "snapshotsLocation" };
    void uploadSnapshot(const QString& filename, const QUrl& href = QUrl(""));
    // What happens next to a snapshot taken with the default name: one in the uploads folder (someone was signed in
    // when it was taken) is uploaded to the user's files, and any other stays where it is; snapshotStored says which.
    // Safe from any thread: the work is queued to this object's thread.
    void storeSnapshot(const QString& takenPath);

signals:

    /**jsdoc
     * Triggered when the path that snapshots are saved to is changed.
     * @function Snapshot.snapshotLocationSet
     * @param {string} location - The new snapshots location.
     * @returns {Signal}
     * @example <caption>Report when the snapshots location is changed.</caption>
     * // Run this script then change the snapshots location in Settings > General > Snapshots.
     * Snapshot.snapshotLocationSet.connect(function (path) {
     *     print("New snapshot location: " + path);
     * });
     */
    void snapshotLocationSet(const QString& value);

    /**jsdoc
     * Triggered when a snapshot taken with the default name, as Snap takes them, is where it will stay: in the user's
     * files (My Files), in their Snapshots folder, or on this computer, when no one was signed in as it was taken or the
     * upload failed. A snapshot uploaded to the user's files is deleted from this computer.
     * @function Snapshot.snapshotStored
     * @param {string} takenPath - The path the snapshot had when it was taken, as given by
     *     {@link Window.stillSnapshotTaken|stillSnapshotTaken}, {@link Window.processingGifStarted|processingGifStarted}
     *     or {@link Window.processingGifCompleted|processingGifCompleted}.
     * @param {string} location - Where the snapshot is now: its URL in the user's files, or its path on this computer.
     * @param {boolean} inUserFiles - <code>true</code> if it is in the user's files, <code>false</code> if it is on this
     *     computer.
     * @param {string} uploadError - Why the upload failed, when it did; empty when the snapshot was uploaded or no
     *     upload was tried (no one was signed in).
     * @returns {Signal}
     * @example <caption>Report where each Snap goes.</caption>
     * Snapshot.snapshotStored.connect(function (takenPath, location, inUserFiles, uploadError) {
     *     print(inUserFiles ? "In My Files: " + location : "On this computer: " + location + " " + uploadError);
     * });
     */
    void snapshotStored(const QString& takenPath, const QString& location, bool inUserFiles, const QString& uploadError);

    /**jsdoc
     * Triggered when {@link Snapshot.deleteFromUserFiles|deleteFromUserFiles} is done.
     * @function Snapshot.snapshotDeleted
     * @param {string} url - The snapshot's URL in the user's files.
     * @param {string} error - Why it could not be deleted; empty when it was.
     * @returns {Signal}
     */
    void snapshotDeleted(const QString& url, const QString& error);

public slots:

    /**jsdoc
     * Gets the path that snapshots are saved to.
     * @function Snapshot.getSnapshotsLocation
     * @returns {string} The path to save snapshots to.
     */
    Q_INVOKABLE QString getSnapshotsLocation();

    /**jsdoc
     * Sets the path that snapshots are saved to.
     * @function Snapshot.setSnapshotsLocation
     * @param {String} location - The path to save snapshots to.
     */
    Q_INVOKABLE void setSnapshotsLocation(const QString& location);

    /**jsdoc
     * Gets how long an animated snapshot (a GIF) records.
     * @function Snapshot.getAnimatedSnapshotDuration
     * @returns {number} Its length in seconds, 1 to 5.
     */
    Q_INVOKABLE float getAnimatedSnapshotDuration();

    /**jsdoc
     * Sets how long an animated snapshot (a GIF) records: the Settings panel's Snapshots section.
     * @function Snapshot.setAnimatedSnapshotDuration
     * @param {number} seconds - Its length in seconds, held to 1 to 5.
     */
    Q_INVOKABLE void setAnimatedSnapshotDuration(float seconds);

    /**jsdoc
     * Deletes a snapshot from the user's files: one uploaded there by this viewer since it started. Others are left
     * alone. {@link Snapshot.snapshotDeleted|snapshotDeleted} says when it is done.
     * @function Snapshot.deleteFromUserFiles
     * @param {string} url - The snapshot's URL, as {@link Snapshot.snapshotStored|snapshotStored} gave it.
     */
    Q_INVOKABLE void deleteFromUserFiles(const QString& url);

private slots:
    void takeNextSnapshot();

private:
    // forUserFiles: the picture is saved in the uploads folder, to be uploaded, without the address it was taken at
    QFile* savedFileForSnapshot(QImage& image,
                                       bool isTemporary,
                                       const QString& userSelectedFilename = QString(),
                                       const QString& userSelectedPathname = QString(),
                                       bool forUserFiles = false);
    // the folder a picture waits in for its upload (in the application's local data); empty if it can't be made
    static QString uploadsFolder();
    void uploadToUserFiles(const QString& takenPath);
    void keepOnThisComputer(const QString& takenPath, const QString& uploadError);
    // snapshots an earlier session left in the uploads folder, closed before their uploads finished, are kept on this
    // computer (not uploaded now: someone else may be signed in)
    void keepLeftovers();
    QDateTime _sessionStart { QDateTime::currentDateTime() };
    bool _leftoversKept { false };
    // the snapshots uploaded to the user's files since the viewer started: URL -> path there ("/Snapshots/<name>")
    QHash<QString, QString> _uploadedPaths;
    QString _snapshotFilename;
    bool _notify360;
    bool _cubemapOutputFormat;
    QTimer _snapshotTimer;
    qint16 _snapshotIndex;
    bool _waitingOnSnapshot { false };
    bool _taking360Snapshot { false };
    bool _oldEnabled;
    QVariant _oldAttachedEntityId;
    QVariant _oldOrientation;
    QVariant _oldvFoV;
    QVariant _oldNearClipPlaneDistance;
    QVariant _oldFarClipPlaneDistance;
    QImage _imageArray[6];
    void convertToCubemap();
    void convertToEquirectangular();
};

#endif // hifi_Snapshot_h
