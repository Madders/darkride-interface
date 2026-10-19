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

private:
    bool waitForCondition(qint64 maxWaitMs, std::function<bool()> condition);
    QString _testResultsLocation;
};

#endif  // hifi_TestScriptingInterface_h
