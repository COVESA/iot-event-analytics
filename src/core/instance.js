/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const {
    DEFAULT_FEATURE_TTL_MS,
    DEFAULT_HISTORY_LENGTH
} = require('./constants');
const jsonQuery = require('./util/jsonQuery');

class Instance {
    constructor(type, id) {
        this.id = id;
        this.type = type;
        this.features = [];
        this.featureHelper = {};
    }

    getFeatures() {
        return this.features;
    }

    getFeatureAt(idx, defaultValue) {
        this.__validateIndex(idx);

        if (idx >= this.features.length || this.features[idx] === null) {
            if (defaultValue === undefined) {
                throw new Error(`Feature at index ${idx} is null and no default value was given`);
            }

            return Instance.createFeature(defaultValue, defaultValue, -1, -1)
        }

        return this.features[idx];
    }

    setFeatureAt(idx, encodedValue, rawValue, whenMs, maxHistoryLength = DEFAULT_HISTORY_LENGTH, ttlMs = DEFAULT_FEATURE_TTL_MS) {
        // Prune outdated values as soon as any value is set
        this.prune();

        if (whenMs === undefined || encodedValue === undefined || rawValue === undefined) {
            // All values need to be defined
            return false;
        }

        if (this.featureHelper[idx] === undefined) {
            // Initialize feature helper
            this.featureHelper[idx] = {
                ttlMs: 0
            };

            if (encodedValue !== null) {
                // If an encoding exists, initialize the new Running Stats calculator
                this.featureHelper[idx].rstat = new RunningStat();
            }
        }

        // Runninc statistical calculation takes values into account since the start of the platform
        // >> Every events contributes to the statistical values even though...
        //   - ...the given timestamp is older than the most recent value
        //   - ...the given timestamp is outdated (older than the given ttl)
        if (encodedValue !== null) {
            let encodedNumericValue = encodedValue;

            try {
                if (typeof encodedValue === 'object' && encodedValue.$vpath) {
                    // Fetch the numeric value from given $vpath
                    encodedNumericValue = jsonQuery.first(encodedValue, encodedValue.$vpath).value
                }

                this.featureHelper[idx].rstat.push(encodedNumericValue);
            }
            catch(err) {
                // Could not update statistical data
            }
        }

        if (whenMs + ttlMs < Date.now()) {
            // Given value is already invalid
            return false;
        }

        if (idx >= this.features.length) {
            this.features = [...this.features, ...new Array(idx - this.features.length + 1).fill(null)];
        }

        if (this.features[idx] !== null) {
            if (this.features[idx].whenMs > whenMs) {
                // Current value is newer than the one, which should be written
                return false;
            }
        }

        // Process mapped results
        const $part = rawValue.$part;

        if (Number.isFinite($part)) {
            if (this.features[idx] === null || !Array.isArray(this.features[idx].raw)) {
                throw new Error(`Partial results need a target feature of type Array.`);
            }

            if ($part < 0 || $part >= this.features[idx].raw.length) {
                throw new Error(`Invalid partial index given ${$part}`);
            }

            this.features[idx].raw[$part] = rawValue.value;
            // No encoded features for worker results
            this.features[idx].enc = null;

            return true;
        }

        let $history = [];

        if (maxHistoryLength > 0) {
            // If history should actually be kept
            if (this.features[idx] !== null) {
                // History entries do not contain the history property at all
                // History entries do not contain statistical data
                // eslint-disable-next-line no-unused-vars
                let { history, stat, ...previousFeature } = Object.assign({}, this.features[idx]);
                $history = history;
                $history.unshift(previousFeature);
            }

            if ($history.length > maxHistoryLength) {
                $history.splice(maxHistoryLength);
            }
        }

        this.features[idx] = Instance.createFeature(
            rawValue,
            encodedValue,
            whenMs,
            whenMs + ttlMs,
            $history
        );

        // Add statistical data
        if (this.featureHelper[idx].rstat) {
            const variance = this.featureHelper[idx].rstat.variance();

            this.features[idx].stat = {
                cnt: this.featureHelper[idx].rstat.count(),
                mean: this.featureHelper[idx].rstat.mean(),
                var: variance,
                sdev: this.featureHelper[idx].rstat.standardDeviation(variance)
            };
        }

        this.featureHelper[idx].ttlMs = this.features[idx].ttlMs;

        return true;
    }

    prune(now = Date.now()) {
        for (let idx of Object.keys(this.featureHelper)) {
            if (this.featureHelper[idx].ttlMs > 0 && this.featureHelper[idx].ttlMs < now) {
                this.features[idx] = null;
                // Remove the feature helper for this specific feature
                // All statistical data and history gets deleted
                // delete this.featureHelper[idx];
                // Soft-delete by setting featureHelper[idx].ttlMs to 0
                // May get slow over time, since no feature within the featureHelper gets actually deleted
                // even though they might be invalid
                // TODO: Maybe make this configurable in the metadata
                this.featureHelper[idx].ttl = 0;
            }
        }
    }

    __validateIndex(idx) {
        if (Number.isFinite(idx) && idx >= 0) {
            return true;
        }

        throw new Error(`Invalid index ${idx} given`);
    }
}

Instance.createFeature = function createFeature(rawValue, encodedValue, whenMs, ttlMs, history = []) {
    return {
        whenMs,
        ttlMs,
        history,
        raw: rawValue,
        enc: encodedValue,
        stat: null
    };
};

// https://www.johndcook.com/blog/standard_deviation/
class RunningStat {
    constructor() {
        this.n = 0;
        this.oldM = 0;
        this.newM = 0;
        this.oldS = 0;
        this.newS = 0;
    }

    count() {
        return this.n;
    }

    mean() {
        return this.n > 0 ? this.newM : null;
    }

    variance() {
        return this.n > 1 ? this.newS / (this.n - 1) : null;
    }

    standardDeviation(variance) {
        variance = variance !== undefined ? variance : this.variance();

        if (variance === null) {
            return null;
        }

        return Math.pow(variance, 0.5);
    }

    push(x) {
        this.n++;

        // See Knuth TAOCP vol 2, 3rd edition, page 232
        if (this.n === 1) {
            this.oldM = this.newM = x;
            this.oldS = 0;
        } else {
            this.newM = this.oldM + (x - this.oldM) / this.n;
            this.newS = this.oldS + (x - this.oldM) * ( x - this.newM );
            this.oldM = this.newM;
            this.oldS = this.newS;
        }
    }
}

module.exports = Instance;