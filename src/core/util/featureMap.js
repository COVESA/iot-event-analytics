/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

// Stores entries in following hierarchical order
// <type>.<feature>.<instanceId>.{ $metadata, }

module.exports = class FeatureMap {
    constructor() {
        this.clear();
    }

    contains(type, feature, instanceId) {
        try {
            this.entries[type][feature][instanceId].$feature;
            return true;
        }
        catch(err) {
            return false;
        }
    }

    clear() {
        this.entries = {};
    }

    set(type, feature, instanceId, $feature, $metaFeature) {
        const featureEntry = this.__createFeatureEntryIfNeeded(type, feature);

        featureEntry.$metadata = $metaFeature;

        let matches = 0;

        if (Object.prototype.hasOwnProperty.call(featureEntry, instanceId)) {
            matches = featureEntry[instanceId].matches;
        }

        featureEntry[instanceId] = { $feature, matches };
    }

    get(type, feature, instanceId) {
        if (this.contains(type, feature, instanceId)) {
            return this.entries[type][feature][instanceId];
        }

        throw new Error(`Could not find FeatureMap entry with type=${type}, feature=${feature}, instanceId=${instanceId}`);
    }

    recordRuleMatchFor(type, feature, instanceId) {
        try {
            this.get(type, feature, instanceId).matches++;
        }
        catch(err) {
            // Match cannot be recorded
            // Omit this
        }
    }

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
};