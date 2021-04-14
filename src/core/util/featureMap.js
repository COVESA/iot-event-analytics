/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/


/**
 * Module Feature Map.
 * 
 * @module featureMap
 */

/**
 * This is a utility class for keeping features organized. Stores entries in the following hierarchical order
 * \<type\>.\<feature\>.\<instanceId\>.{ $metadata, }.
 */
 class FeatureMap {
    /**
     * Constructs a FeatureMap instance with no entries.
     */
    constructor() {
        this.clear();
    }

    /**
     * Checks if the FeatureMap contains an entry with the specified _type_, _feature_ and _instanceId_ properties.
     *
     * @param {*} type - Type that the feature belongs to.
     * @param {*} feature - Feature name.
     * @param {*} instanceId - Instance Id.
     * @returns true if the map contains the specified feature.
     */
    contains(type, feature, instanceId) {
        try {
            this.entries[type][feature][instanceId].$feature;
            return true;
        }
        catch (err) {
            return false;
        }
    }

    clear() {
        this.entries = {};
    }

    /**
     * Sets a value of an entry in the map. The entry is identified by _type_, _feature_ and _instanceId_ parameters.
     * If this entry is already in the map, it will be overwritten.
     * 
     * @param {*} type - Type that the feature belongs to.
     * @param {*} feature - Feature name.
     * @param {*} instanceId - Instance Id.
     * @param {*} $feature - Value of the feature.
     * @param {*} $metaFeature - Metadata for the feature.
     */
    set(type, feature, instanceId, $feature, $metaFeature) {
        const featureEntry = this.__createFeatureEntryIfNeeded(type, feature);

        featureEntry.$metadata = $metaFeature;

        let matches = 0;

        if (Object.prototype.hasOwnProperty.call(featureEntry, instanceId)) {
            matches = featureEntry[instanceId].matches;
        }

        featureEntry[instanceId] = { $feature, matches };
    }

    /**
     * Gets the entry in the map that can be identified by _type_, _feature_ and _instanceId_ parameters.
     * 
     * @param {*} type - Type that the feature belongs to.
     * @param {*} feature - Feature name.
     * @param {*} instanceId - Instance Id.
     * @returns the matching entry value, previously set by {@link module:featureMap~FeatureMap#set}.
     * @throws an Error in case the entry can not be found.
     */
    get(type, feature, instanceId) {
        if (this.contains(type, feature, instanceId)) {
            return this.entries[type][feature][instanceId];
        }

        throw new Error(`Could not find FeatureMap entry with type=${type}, feature=${feature}, instanceId=${instanceId}`);
    }

    /**
     * When a rule matches against a feature it can record the match using this method.
     * 
     * @param {*} type - Type that the feature belongs to.
     * @param {*} feature - Feature name.
     * @param {*} instanceId - Instance Id.
     */
    recordRuleMatchFor(type, feature, instanceId) {
        try {
            this.get(type, feature, instanceId).matches++;
        }
        catch (err) {
            // Match cannot be recorded
            // Omit this
        }
    }

    /**
     * Gets a reference to the entries of the feature map.
     * 
     * @returns object with map entries.
     */
    dump() {
        return this.entries;
    }

    __createFeatureEntryIfNeeded(type, feature) {
        let $type = this.entries[type];

        if ($type === undefined) {
            $type = {};
            this.entries[type] = $type;
        }

        let $feature = this.entries[type][feature];

        if (!Object.prototype.hasOwnProperty.call(this.entries[type], feature)) {
            $feature = {};
            this.entries[type][feature] = $feature;
        }

        return $feature;
    }
}

module.exports = FeatureMap;
