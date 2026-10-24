//
//  Created by Bradley Austin Davis on 2016/12/12
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once
#ifndef hifi_TestScriptingInterface_h
#define hifi_TestScriptingInterface_h

#include <functional>
#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtCore/QVariant>

#include <glm/glm.hpp>

/**jsdoc
 * Here's how you could use the Test API:
 * 
 * <ul>
 *     <li>Write a script using the below and launch interface with: <code>interface --testScript path/to/script.js</code></li>
 *     <li>Save the recorded events with {@link Test.stopTracing} to a <code>.json</code> or <code>.json.gz</code> file</li>
 *     <li>Open Chrome, visit <a href="chrome://tracing">chrome://tracing</a> and load your file</li>
 * </ul>
 * 
 * @namespace Test
 *
 * @hifi-interface
 */

class QScriptValue;

class TestScriptingInterface : public QObject {
    Q_OBJECT

public:
    void setTestResultsLocation(const QString path) { _testResultsLocation = path; }
    const QString& getTestResultsLocation() { return _testResultsLocation;  };

public slots:
    static TestScriptingInterface* getInstance();

    /**jsdoc
     * Exits the application
     * @function Test.quit
     */
    void quit();

    /**jsdoc
    * Waits for all texture transfers to be complete
    * @function Test.waitForTextureIdle
    */
    void waitForTextureIdle();

    /**jsdoc
    * Waits for all pending downloads to be complete
    * @function Test.waitForDownloadIdle
    */
    void waitForDownloadIdle();

    /**jsdoc
    * Waits for all file parsing operations to be complete
    * @function Test.waitForProcessingIdle
    */
    void waitForProcessingIdle();

    /**jsdoc
    * Waits for all pending downloads, parsing and texture transfers to be complete
    * @function Test.waitIdle
    */
    void waitIdle();

    /**jsdoc
    * Waits for establishment of connection to server
    * @function Test.waitForConnection
    * @param {number} [maxWaitMs=10000] - Number of milliseconds to wait
    */
    bool waitForConnection(qint64 maxWaitMs = 10000);

    /**jsdoc
    * Waits a specific number of milliseconds
    * @function Test.wait
    * @param {number} milliseconds - Number of milliseconds to wait
    */
    void wait(int milliseconds);

    /**jsdoc
    * Waits for all pending downloads, parsing and texture transfers to be complete
    * @function Test.loadTestScene
    * @param {string} sceneFile - URL of scene to load
    */
    bool loadTestScene(QString sceneFile);

    /**jsdoc
    * Clears all caches
    * @function Test.clear
    */
    void clear();

    /**jsdoc
    * Start recording Chrome compatible tracing events<br />
    * <code>logrules</code> can be used to specify a set of logging category rules to limit what gets captured
    * @function Test.startTracing
    * @param {string} [logrules=""] - See implementation for explanation
    */
    bool startTracing(QString logrules = "");

    /**jsdoc
    * Stop recording Chrome compatible tracing events and serialize recorded events to a file<br />
    * Using a filename with <code>.gz</code> extension will automatically compress the output file
    * @function Test.stopTracing
    * @param {string} filename - Name of file to save to ending with <code>.json</code> or <code>.json.gz</code>
    * @returns {bool} True if successful.   
    */
    bool stopTracing(QString filename);

    /**jsdoc
    * Starts a specific trace event
    * @function Test.startTraceEvent
    * @param {string} name - Name of event
    */
    void startTraceEvent(QString name);

    /**jsdoc
    * Stop a specific name event
    * @function Test.endTraceEvent
    * @param {string} name - Name of event
    */
    void endTraceEvent(QString name);

    /**jsdoc
     * Write detailed timing stats of next physics stepSimulation() to filename
     * @function Test.savePhysicsSimulationStats
     * @param {string} filename - Name of file to save to
     */
    void savePhysicsSimulationStats(QString filename);

    /**jsdoc
    * Profiles a specific function
    * @function Test.savePhysicsSimulationStats
    * @param {string} name - Name used to reference the function
    * @param {function} function - Function to profile
    */
    Q_INVOKABLE void profileRange(const QString& name, QScriptValue function);

    /**jsdoc
    * Clear all caches (menu command Reload Content)
    * @function Test.clearCaches
    */
    void clearCaches();

    /**jsdoc
    * Save a JSON object to a file in the test results location
    * @function Test.saveObject
    * @param {string} name - Name of the object
    * @param {string} filename - Name of file to save to
    */
    void saveObject(QVariant v, const QString& filename);

    /**jsdoc
    * Maximizes the window
    * @function Test.showMaximized
    */
    void showMaximized();

    /**jsdoc
    * Values higher than 0 will create replicas of other-avatars when entering a domain for testing purpouses
    * @function Test.setOtherAvatarsReplicaCount
    * @param {number} count - Number of replicas we want to create
    */
    Q_INVOKABLE void setOtherAvatarsReplicaCount(int count);

    /**jsdoc
    * Return the number of replicas that are being created of other-avatars when entering a domain
    * @function Test.getOtherAvatarsReplicaCount
    * @returns {number} Current number of replicas of other-avatars.
    */
    Q_INVOKABLE int getOtherAvatarsReplicaCount();

    /**jsdoc
     * Set number of cycles texture size is required to be stable
     * @function Test.setMinimumGPUTextureMemStabilityCount
     * @param {number} count - Number of cycles to wait
     */
    Q_INVOKABLE void setMinimumGPUTextureMemStabilityCount(int count);

    /**jsdoc
     * Check whether all textures have been loaded
     * @function Test.isTextureLoadingComplete
     * @returns {boolean} <code>true</code> texture memory usage is not increasing
     */
    Q_INVOKABLE bool isTextureLoadingComplete();

    /**jsdoc
     * The rate of the game loop (simulation and script updates), in frames per second, as the stats show it.
     * @function Test.getGameLoopRate
     * @returns {number}
     */
    Q_INVOKABLE float getGameLoopRate();

    /**jsdoc
     * The rate at which frames are presented, in frames per second.
     * @function Test.getRenderLoopRate
     * @returns {number}
     */
    Q_INVOKABLE float getRenderLoopRate();

    /*@jsdoc
     * Runs the avatar's spring bones in lockstep: once on, they advance only by the time handed to them with
     * {@link Test.advanceSpringBones}, whatever the frames take, so a recording can be replayed frame by frame.
     * @function Test.setSpringBoneLockstep
     * @param {boolean} on - Lockstep on or off (off returns them to real time).
     */
    Q_INVOKABLE void setSpringBoneLockstep(bool on);

    /*@jsdoc
     * Hands the spring bones time to advance by at their next update, in lockstep.
     * @function Test.advanceSpringBones
     * @param {number} seconds - The time to advance.
     */
    Q_INVOKABLE void advanceSpringBones(float seconds);

    /*@jsdoc
     * The same, with the avatar's own movement over those seconds given (a replay whose avatar is placed by its hips
     * says what the recorded root did) rather than read from the avatar's position.
     * @function Test.advanceSpringBones
     * @param {number} seconds - The time to advance.
     * @param {Vec3} avatarMove - The avatar's own movement over that time.
     */
    Q_INVOKABLE void advanceSpringBones(float seconds, const glm::vec3& avatarMove);

    /*@jsdoc
     * Gets how many steps the avatar's spring bones have taken since the client started.
     * @function Test.getSpringBoneSteps
     * @returns {number} The step count.
     */
    Q_INVOKABLE int getSpringBoneSteps();

    /*@jsdoc
     * Gets the time handed to {@link Test.advanceSpringBones} that the spring bones have not consumed yet.
     * @function Test.getSpringBonePendingTime
     * @returns {number} Seconds.
     */
    Q_INVOKABLE float getSpringBonePendingTime();

    /*@jsdoc
     * Gets how many updates of the spring bones have consumed time handed by {@link Test.advanceSpringBones}, each
     * counted once its poses are written, so a test can read poses back only then.
     * @function Test.getSpringBoneConsumed
     * @returns {number} The count.
     */
    Q_INVOKABLE int getSpringBoneConsumed();

    /*@jsdoc
     * Gets the blend between the last two steps the spring bones last presented: 0.5 at a step's own frame, 1 a frame
     * of 60 Hz later.
     * @function Test.getSpringBoneAlpha
     * @returns {number} The blend.
     */
    Q_INVOKABLE float getSpringBoneAlpha();

    /*@jsdoc
     * Gets how many updates the avatar's spring bones have run in all.
     * @function Test.getSpringBoneUpdates
     * @returns {number} The count.
     */
    Q_INVOKABLE int getSpringBoneUpdates();

    /**jsdoc
     * Turns build mode on and selects these entities in the editor, so a scripted test can picture the Inspector.
     * @function Test.selectInEditor
     * @param {Uuid[]} ids - The entities to select.
     */
    Q_INVOKABLE void selectInEditor(const QStringList& ids);

    /**jsdoc
     * Frames the editor's selection in the viewport, as the F key does in build mode.
     * @function Test.frameEditorSelection
     */
    Q_INVOKABLE void frameEditorSelection();

    /**jsdoc
     * Saves a picture of the Inspector's whole content, whatever is scrolled into view, as the panel draws it.
     * @function Test.saveInspectorPicture
     * @param {string} path - Where to save it (PNG).
     * @returns {boolean} Whether it was saved.
     */
    Q_INVOKABLE bool saveInspectorPicture(const QString& path);

    /**jsdoc
     * Saves a picture of the client's window as its widgets draw it (the editor's panels; the viewport itself may come
     * out blank, being drawn by the GPU: Window.takeSnapshot pictures that).
     * @function Test.saveWindowPicture
     * @param {string} path - Where to save it (PNG).
     * @returns {boolean} Whether it was saved.
     */
    Q_INVOKABLE bool saveWindowPicture(const QString& path);

    /**jsdoc
     * Chooses a control point of the Track selected on its own in the editor (-1 for none), as a click on its diamond
     * does: bright in the viewport, the Move gizmo on it, the Inspector's Points list on its row.
     * @function Test.chooseTrackPoint
     * @param {number} index - The point's index, from 0.
     */
    Q_INVOKABLE void chooseTrackPoint(int index);

    /**jsdoc
     * @function Test.chosenTrackPoint
     * @returns {number} The chosen control point's index, -1 for none.
     */
    Q_INVOKABLE int chosenTrackPoint();

    /**jsdoc
     * Inserts a control point on the selected Track's curve at an arc length, as Shift+click on the curve does.
     * @function Test.insertTrackPoint
     * @param {number} arcLength - Metres along the curve from its first point.
     */
    Q_INVOKABLE void insertTrackPoint(float arcLength);

    /**jsdoc
     * Removes the selected Track's chosen control point, as Delete does.
     * @function Test.deleteTrackPoint
     */
    Q_INVOKABLE void deleteTrackPoint();

    /**jsdoc
     * Undoes the editor's last step, as Cmd+Z does in build mode.
     * @function Test.editorUndo
     */
    Q_INVOKABLE void editorUndo();

    /**jsdoc
     * Puts an installed skin on (ui/Skins.h), or Darkride's own look with "", as the Skins panel does, so a scripted
     * test can picture build mode's panels under a skin and back.
     * @function Test.setSkin
     * @param {string} id - The installed skin's id (its folder's name under the app data's skins/), or "".
     * @returns {string} "" when it is on, else what is wrong with it.
     */
    Q_INVOKABLE QString setSkin(const QString& id);

    /**jsdoc
     * The colour an entity's row in build mode's Hierarchy is drawn in: red for one that couldn't load, grey for one
     * listed but not loaded yet (both from the skin while one is on), "" for the panel's own colour.
     * @function Test.hierarchyRowColor
     * @param {Uuid} id - The entity.
     * @returns {string} "#rrggbb", or "".
     */
    Q_INVOKABLE QString hierarchyRowColor(const QUuid& id);

    /**jsdoc
     * Imports an entities file into the world as Build > Import Entities… does once the file is chosen: placed under
     * the cursor (or in front of the camera), selected, one undo step; build mode comes on for it.
     * @function Test.editorImportEntities
     * @param {string} url - The file's URL (file:///…, https://…, atp:…).
     * @returns {boolean} Whether any entities were made.
     */
    Q_INVOKABLE bool editorImportEntities(const QString& url);

    /**jsdoc
     * Exports the editor's selection and its descendants as Build > Export Selected Entities… does once the path is
     * chosen.
     * @function Test.editorExportSelection
     * @param {string} path - The file to write (.json).
     * @returns {number} How many entities were written, -1 for none.
     */
    Q_INVOKABLE int editorExportSelection(const QString& path);

    /**jsdoc
     * What build mode's Hierarchy says the entity could not load (its row red, these lines in its tooltip): its model,
     * image or textures that failed, with the reasons; empty when nothing has. Build mode checks once a second.
     * @function Test.editorLoadProblem
     * @param {Uuid} id - The entity.
     * @returns {string} The failures, one per line.
     */
    Q_INVOKABLE QString editorLoadProblem(const QUuid& id);

    /**jsdoc
     * Turns build mode on or off, as Build > Build Mode does.
     * @function Test.setBuildMode
     * @param {boolean} on - Whether build mode is on.
     */
    Q_INVOKABLE void setBuildMode(bool on);

    /**jsdoc
     * The build-mode Hierarchy's rows: how many in all and how many loaded, whether the entity server's list of every
     * entity has arrived, the list request's state, and the editor's selection.
     * @function Test.getHierarchyCounts
     * @returns {object} <code>{ entities, loaded, listed, state, selected }</code>; <code>state</code> is "none",
     *     "waiting", "answered" or "unanswered".
     */
    Q_INVOKABLE QVariantMap getHierarchyCounts();

    /**jsdoc
     * Does what a click on the entity's row in the build-mode Hierarchy does: selects it, or, for a row that is not
     * loaded, flies the build camera to it and selects it when it arrives.
     * @function Test.clickHierarchyRow
     * @param {Uuid} id - The entity.
     */
    Q_INVOKABLE void clickHierarchyRow(const QUuid& id);

    /**jsdoc
     * Edits a property of the editor's selection as its Inspector field does when a list's + button or a field commits:
     * the undo point taken, the value sent, one step recorded.
     * @function Test.commitInspectorEdit
     * @param {string} key - The Inspector's key for the property (e.g. "trackPoints").
     * @param {string|number|boolean} value - The value as the field holds it.
     */
    Q_INVOKABLE void commitInspectorEdit(const QString& key, const QVariant& value);

    /**jsdoc
     * Starts tracing an entity as it is drawn: one sample per rendered frame, taken on the render thread once the frame's
     * scene is in place, of the frame's camera position and the centre of the entity's drawn bound. A null id stops it.
     * @function Test.traceRenderedEntity
     * @param {Uuid} entityID - The entity.
     * @returns {boolean} Whether the entity has a render item to trace.
     */
    Q_INVOKABLE bool traceRenderedEntity(const QUuid& entityID);

    /**jsdoc
     * Logs every main-view draw of a model part for the next frames (INSTANCING-DUMP lines: the model file, mesh, part,
     * level of detail, indices and each instance's position), to diff a frame with GPU instancing on against one with
     * it off (tests-manual/instancing/diff-dump.py).
     * @function Test.dumpDraws
     * @param {number} frames - How many frames to log.
     */
    Q_INVOKABLE void dumpDraws(int frames);

    /**jsdoc
     * Takes the samples traced so far: each is [time in ms, camera x, y, z, item x, y, z, frame index].
     * @function Test.takeRenderTrace
     * @returns {Array<Array<number>>} The samples, oldest first.
     */
    Q_INVOKABLE QVariantList takeRenderTrace();

    /**jsdoc
     * Starts or stops recording the presents: one sample per present, taken on the present thread once the swap has
     * returned (with vsync on, at the display's refresh). Starting clears what was recorded.
     * @function Test.tracePresents
     * @param {boolean} on - Whether to record.
     */
    Q_INVOKABLE void tracePresents(bool on);

    /**jsdoc
     * Takes the presents recorded so far: each is [time in ms, the rendered frame's index (as in the render trace),
     * 1 if that frame was new or 0 if the last frame was shown again, the present's whole duration in ms, the swap's
     * own duration in ms (its wait for the display, with vsync), the GPU's averaged time per frame in ms].
     * @function Test.takePresentTrace
     * @returns {Array<Array<number>>} The samples, oldest first.
     */
    Q_INVOKABLE QVariantList takePresentTrace();

    /**jsdoc
     * Starts or stops recording the display's refreshes (macOS: the display link's callbacks, each with the instant
     * of the refresh it announces). Starting clears what was recorded.
     * @function Test.traceRefreshes
     * @param {boolean} on - Whether to record.
     */
    Q_INVOKABLE void traceRefreshes(bool on);

    /**jsdoc
     * Takes the refreshes recorded so far: the time of each in ms, on the clock of the present trace.
     * @function Test.takeRefreshTrace
     * @returns {Array<number>} The refresh times, oldest first; empty where the display gives no signal.
     */
    Q_INVOKABLE QVariantList takeRefreshTrace();

    /**jsdoc
     * The refresh rate of the display the window is on, in Hz, as the platform reports it (0 when it does not).
     * @function Test.getDisplayRefreshRate
     * @returns {number} The refresh rate.
     */
    Q_INVOKABLE float getDisplayRefreshRate();

    /**jsdoc
     * Whether the swap waits for the display's refresh (Render.vsyncEnabled, or TIVOLI_VSYNC in the environment).
     * @function Test.isVsyncEnabled
     * @returns {boolean} Whether vsync is on.
     */
    Q_INVOKABLE bool isVsyncEnabled();

    /**jsdoc
     * Makes every present cost this many microseconds more, busy-waited, as a heavy scene's would: for measuring the frame
     * pacing when presents overrun their period. 0 is none. TIVOLI_PRESENT_LOAD_US in the environment sets it at start.
     * @function Test.setPresentLoad
     * @param {number} microseconds - The extra time each present takes.
     */
    Q_INVOKABLE void setPresentLoad(int microseconds);

    /**jsdoc
     * @function Test.getPresentPeriod
     * @returns {number} The period the presents are paced at, in seconds, as the physics reads it; 0 while they are not
     *     paced (vsync, an HMD, or a schedule that restarted after a present overran).
     */
    Q_INVOKABLE float getPresentPeriod();

    /**jsdoc
     * Holds the present loop off for a while, as a stall would: nothing is presented, so no new frames are made. The
     * deadlock watchdog's report names the loop as held for a test once a second has passed.
     * @function Test.holdPresents
     * @param {number} milliseconds - How long; 0 lets presents go at once.
     */
    Q_INVOKABLE void holdPresents(int milliseconds);

    /**jsdoc
     * Drags the editor's selection as the gizmo would, from a script: turns build mode on, and with begin captures the
     * selection as a press on the gizmo does; every call applies the move or the rotation (a roll about the selection's
     * own centre) as the pointer's motion does; final takes the undo step as the release does.
     * @function Test.editorDrag
     * @param {Vec3} delta - The move, in metres (ignored when the rotation is not zero).
     * @param {number} yawDegrees - A rotation about the vertical.
     * @param {number} rollDegrees - A rotation about the forward axis.
     * @param {boolean} begin - The first call of a drag.
     * @param {boolean} final - The last call of a drag.
     */
    Q_INVOKABLE void editorDrag(const glm::vec3& delta, float yawDegrees, float rollDegrees, bool begin, bool final);

private:
    bool waitForCondition(qint64 maxWaitMs, std::function<bool()> condition);
    QString _testResultsLocation;
};

#endif  // hifi_TestScriptingInterface_h
