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
const clone = require('./util/clone');
const Logger = require('./util/logger');

class Instance {
    constructor(type, id, logger = null) {
        this.id = id;
        this.type = type;
        this.features = [];
        this.featureHelper = {};
        this.logger = logger || new Logger('Instance');
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

    updateFeatureAt(idx, encodedValue, rawValue, whenMs, maxHistoryLength = DEFAULT_HISTORY_LENGTH, ttlMs = DEFAULT_FEATURE_TTL_MS, now = Date.now()) {
        // Check for invalid parameters >> reject value
        if (whenMs === undefined || encodedValue === undefined || rawValue === undefined) {
            this.logger.verbose(`(instance id=${this.id} type=${this.type}) whenMs, encodedValue and rawValue must not be undefined`);
            return null;
        }

        // Prune outdated values
        this.prune();

        let $hidx = -1;

        // Does the raw event value contain a partial value?
        if (this.__shouldProcessPartialValueAt(idx, rawValue.$part)) {
            // Partial values do not have any history, since only the most recent value is updated according to the given $part(ial) index
            this.logger.verbose(`(instance id=${this.id} type=${this.type}) Updating partial feature at index ${idx} at partial index ${rawValue.$part} to ${rawValue.value}`);
            // Update the current value
            this.features[idx].raw[rawValue.$part] = rawValue.value;
            // No encoded features for partial values
            this.features[idx].enc = null;
            // Update the ttl, since the value was updated right now
            this.features[idx].ttl = now + ttlMs;

            this.featureHelper[idx].ttlMs = this.features[idx].ttlMs;

            return {
                // $hidx is always -1 if a partial value was processed
                $hidx,
                $feature: this.features[idx]
            };
        }

        if (idx < this.features.length && this.features[idx] !== null) {
            if (this.features[idx].whenMs === whenMs) {
                // If the timestamp already exists for this feature >> reject value
                this.logger.verbose(`(instance id=${this.id} type=${this.type}) Timestamp already exists for given value [${idx}]=${rawValue} at ${whenMs}`);
                return null;
            }

            if (this.features[idx].history.find(historicalFeature => historicalFeature.whenMs === whenMs)) {
                // If the timestamp already exists in this features history >> reject value
                this.logger.verbose(`(instance id=${this.id} type=${this.type}) Timestamp exists in history for given value [${idx}]=${rawValue} at ${whenMs}`);
                return null
            }

            if (whenMs + ttlMs < now) {
                const history = this.features[idx].history;

                // Select the oldest feature
                const oldestFeature = history.length > 0 ? history[history.length - 1] : this.features[idx];

                if (whenMs + ttlMs < oldestFeature.whenMs) {
                    // The feature would have been pruned, if it had arrived at that given time
                    this.logger.verbose(`(instance id=${this.id} type=${this.type}) Feature would have been pruned already for value [${idx}]=${rawValue} at ${whenMs}`);
                    return null;
                }
            }
        } else {
            if (whenMs + ttlMs < now) {
                // Feature does not exist yet, and the given feature is already invalid
                this.logger.verbose(`(instance id=${this.id} type=${this.type}) Feature does not exist and given feature is alread invalid for value [${idx}]=${rawValue} at ${whenMs}`);
                return null;
            }
        }

        // Initialize the feature helper
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

        // Update the statistical values
        if (encodedValue !== null) {
            let encodedNumericValue = encodedValue;

            try {
                if (typeof encodedValue === 'object' && encodedValue.$vpath) {
                    // Fetch the numeric value from given $vpath
                    encodedNumericValue = jsonQuery.first(encodedValue, encodedValue.$vpath).value
                }
                this.featureHelper[idx].rstat.push(encodedNumericValue, whenMs);
            }
            catch(err) {
                // Could not update statistical data
            }
        }

        // Expand the length of features array to match the given index
        if (idx >= this.features.length) {
            this.features = [...this.features, ...new Array(idx - this.features.length + 1).fill(null)];
        }

        if (this.features[idx] !== null && whenMs < this.features[idx].whenMs) {
            // Insert value into history
            const history = this.features[idx].history;

            let historyIndex = 0;

            for (let i = 0; i <= history.length; i++) {
                if (i === history.length || whenMs > history[i].whenMs) {
                    // Place it right in front of i

                    const historicalFeature = Instance.createHistoryFeature(
                        rawValue,
                        encodedValue,
                        whenMs,
                        whenMs + ttlMs
                    );

                    history.splice(i, 0, historicalFeature);
                    historyIndex = i;
                    break;
                }
            }

            if (historyIndex < maxHistoryLength) {
                // If the feature could actually be inserted within the valid portion of the history
                $hidx = historyIndex;
            }
        } else {
            // Replace current value
            let $history = [];

            if (this.features[idx] !== null && maxHistoryLength > 0) {
                // History entries do not contain the history property at all
                // eslint-disable-next-line no-unused-vars
                let { history, stat, ...previousFeature } = clone(this.features[idx]);

                $history = history;
                // Add current feature at top of history
                $history.unshift(previousFeature);
            }

            // The partial value stays the same and gets updated below
            this.features[idx] = Instance.createFeature(
                rawValue,
                encodedValue,
                whenMs,
                whenMs + ttlMs,
                $history
            );
        }

        let deletedTimestampsMs = [];

        if (this.features[idx].history.length > maxHistoryLength) {
            deletedTimestampsMs = this.features[idx].history.splice(maxHistoryLength).map(historyEntry => historyEntry.whenMs);
        }

        if (this.featureHelper[idx].rstat) {
            // Remove invalid timestamps
            this.featureHelper[idx].rstat.remove(deletedTimestampsMs);
            // Recalculate statistical data
            this.features[idx].stat = this.featureHelper[idx].rstat.serialize();
        }

        if ($hidx >= this.features[idx].history.length) {
            // If the current history entry was inserted out of history bounds and thus deleted again in the prior step
            this.logger.verbose(`(instance id=${this.id} type=${this.type}) Feature has been set out of bounds for value [${idx}]=${rawValue}.history[${$hidx}] at ${whenMs}`);
            return null;
        }

        this.featureHelper[idx].ttlMs = this.features[idx].ttlMs;

        return {
            $hidx,
            $feature: this.features[idx]
        };
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

    __shouldProcessPartialValueAt(idx, partialIndex) {
        if (!Number.isFinite(partialIndex)) {
            return false;
        }

        if (this.features[idx] === null || !Array.isArray(this.features[idx].raw)) {
            throw new Error(`Partial results need a target feature of type Array.`);
        }

        if (partialIndex < 0 || partialIndex >= this.features[idx].raw.length) {
            throw new Error(`Invalid partial index given ${partialIndex}`);
        }

        return true;
    }

    __validateIndex(idx) {
        if (Number.isFinite(idx) && idx >= 0) {
            return true;
        }

        throw new Error(`Invalid index ${idx} given`);
    }
}

Instance.createHistoryFeature = function createHistoryFeature(rawValue, encodedValue, whenMs, ttlMs) {
    // eslint-disable-next-line no-unused-vars
    const { history, stat, ...historyFeature } = Instance.createFeature(rawValue, encodedValue, whenMs, ttlMs);
    return historyFeature;
};

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
        this.values = [];
        this.needsRecalculation = false;
    }

    push(value, timestampMs, shouldRecalculate = false) {
        this.needsRecalculation = true;

        this.values.push({ value, ts: timestampMs});

        if (shouldRecalculate) {
            this.__recalculate();
        }
    }

    remove(timestampsMs, shouldRecalculate = false) {
        this.needsRecalculation = true;

        this.values = this.values.filter(value => timestampsMs.indexOf(value.ts) === -1);

        if (shouldRecalculate) {
            this.__recalculate();
        }
    }

    count() {
        return this.values.length;
    }

    mean() {
        if (this.needsRecalculation) {
            this.__recalculate();
        }

        return this.values.length > 0 ? this.__newM : null;
    }

    variance() {
        if (this.needsRecalculation) {
            this.__recalculate();
        }

        return this.values.length > 1 ? this.__newS / (this.values.length - 1) : null;
    }

    standardDeviation(variance) {
        variance = variance !== undefined ? variance : this.variance();

        if (variance === null) {
            return null;
        }

        return Math.pow(variance, 0.5);
    }

    serialize() {
        const variance = this.variance();

        return {
            cnt: this.count(),
            mean: this.mean(),
            var: variance,
            sdev: this.standardDeviation(variance)
        };
    }

    __recalculate() {
        this.__reset();
        for (let i = 0; i < this.values.length; i++) { this.__push(this.values[i].value) }
        this.needsRecalculation = false;
    }

    __reset() {
        this.__n = 0;
        this.__oldM = 0;
        this.__newM = 0;
        this.__oldS = 0;
        this.__newS = 0;
    }

    __push(x) {
        this.__n++;

        // See Knuth TAOCP vol 2, 3rd edition, page 232
        if (this.__n === 1) {
            this.__oldM = this.__newM = x;
            this.__oldS = 0;
        } else {
            this.__newM = this.__oldM + (x - this.__oldM) / this.__n;
            this.__newS = this.__oldS + (x - this.__oldM) * ( x - this.__newM );
            this.__oldM = this.__newM;
            this.__oldS = this.__newS;
        }
    }
}

module.exports = Instance;