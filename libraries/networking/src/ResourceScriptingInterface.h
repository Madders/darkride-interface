//
//  AssetClient.h
//  libraries/networking/src
//
//  Created by Ryan Huffman on 2015/07/21
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#ifndef hifi_networking_ResourceScriptingInterface_h
#define hifi_networking_ResourceScriptingInterface_h

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include <DependencyManager.h>

/**jsdoc
 * The <code>Resources</code> API enables the default location for different resource types to be overridden.
 *
 * @namespace Resources
 * 
 * @hifi-interface
 * @hifi-client-entity
 * @hifi-avatar
 * @hifi-server-entity
 * @hifi-assignment-client
 */

class ResourceScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
public:

    /**jsdoc
     * Overrides a path prefix with an alternative path.
     * @function Resources.overrideUrlPrefix
     * @param {string} prefix - The path prefix to override, e.g., <code>"atp:/"</code>.
     * @param {string} replacement - The replacement path for the prefix.
     */
    Q_INVOKABLE void overrideUrlPrefix(const QString& prefix, const QString& replacement);

    /**jsdoc
     * Restores the default path for a specified prefix.
     * @function Resources.restoreUrlPrefix
     * @param {string} prefix - The prefix of the resource to restore the path for.
     */
    Q_INVOKABLE void restoreUrlPrefix(const QString& prefix) {
        overrideUrlPrefix(prefix, "");
    }

    /**jsdoc
     * Every file whose load failed for good this run, one entry per address (its latest failure), the newest last: a
     * list to work through. A file that loads later leaves it.
     * @function Resources.getFailures
     * @returns {Resources.Failure[]} The failures.
     */
    /**jsdoc
     * @typedef {object} Resources.Failure
     * @property {string} url - The file's address.
     * @property {string} type - <code>"model"</code>, <code>"texture"</code>, <code>"material"</code>,
     *     <code>"animation"</code>, <code>"shader"</code> or <code>"file"</code>.
     * @property {string} reason - Why, e.g. <code>"404 not found"</code>, <code>"host not found: example.com"</code>,
     *     <code>"could not be read"</code>.
     * @property {number} time - When, in milliseconds since the epoch.
     */
    Q_INVOKABLE QVariantList getFailures() const;

    /**jsdoc
     * Empties the list of failures (Resources.getFailures), to start a fresh pass.
     * @function Resources.clearFailures
     */
    Q_INVOKABLE void clearFailures();

signals:
    /**jsdoc
     * Triggered when a file's load fails for good.
     * @function Resources.resourceFailed
     * @param {string} url - The file's address.
     * @param {string} type - What kind of file (see Resources.Failure).
     * @param {string} reason - Why.
     * @returns {Signal}
     */
    void resourceFailed(const QString& url, const QString& type, const QString& reason);
};


#endif
