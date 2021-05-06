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
    VALUE_TYPE_ENCODED,
    VALUE_TYPE_RAW
} = require('./constants');

const {
    Constraint,
    PATH_IDENTITY
} = require('./rules');

const {
    ArrayPatternMatcher
} = require('./util/arrayPatternMatcher');
const jsonQuery = require('./util/jsonQuery');

class TimeseriesConstraint extends Constraint {
    constructor(feature, op, value, typeSelector, valueType = VALUE_TYPE_RAW, minValueCount = 1, path = PATH_IDENTITY, instanceIdFilter, limitFeatureSelection) {
        super(feature, op, value, typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection);
        this.setMinValueCount(minValueCount);
    }

    setMinValueCount(minValueCount) {
        this.minValueCount = minValueCount;
        // Since the current value also counts as value, history size is decreased by one
        this.minHistorySize = minValueCount - 1;
    }

    evaluate($feature) {
        if ($feature.history.length < this.minHistorySize) {
            return false;
        }

        const fieldName = this.__getFieldName(this.valueType);

        try {
            const featureValue = this.__getFeatureValue($feature, fieldName);

            let valueFoundForPath = false;

            // Evaluate for all results at the given path
            for (let result of jsonQuery(featureValue, this.path)) {
                valueFoundForPath = true;
                // Pass in all points needed together with statistical information
                // Get the amount of values needed from the feature
                const values = new Array(this.minHistorySize + 1);

                values[0] = result.value;

                for (let i = 0; i < $feature.history.length; i++) {
                    // Get all the historical values, using the same query, given by the result iteration
                    const hFeatureValue = this.__getFeatureValue($feature.history[i], fieldName);
                    values[i + 1] = jsonQuery.first(hFeatureValue, result.query).value;
                }

                if (!this.__evaluate(values, $feature.stat)) {
                    return false;
                }
            }

            return valueFoundForPath;
        }
        catch(err) {
            return false;
        }
    }

    /* istanbul ignore next */
    __evaluate() {
        // Parameters: values, stat
        throw new Error(`__evaluate(values, stat) needs to be overridden by a subclass`);
    }

    __getFeatureValue($feature, fieldName) {
        if ($feature[fieldName] === undefined) {
            throw new Error(`Field ${fieldName} cannot be found in feature`);
        }

        let featureValue = $feature[fieldName];

        if (featureValue.$vpath && !this.options.isAbsolutePath) {
            Constraint.logger.verbose(`Picking value from given value path = ${featureValue.$vpath}`)
            return jsonQuery.first(featureValue, featureValue.$vpath).value;
        }

        return featureValue;
    }

    __getFieldName(valueType) {
        return valueType === VALUE_TYPE_ENCODED ? 'enc': 'raw';
    }
}

class TimeseriesPatternConstraint extends TimeseriesConstraint {
    // given pattern contains numbers and wildcards
    constructor(feature, pattern, typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection) {
        // value can be set to pattern, and can be serialized as is
        super(feature, Constraint.OPS.TIMESERIES_PATTERN, pattern, typeSelector, valueType, 1, path, instanceIdFilter, limitFeatureSelection);
        this.matcher = new ArrayPatternMatcher(pattern);
        // Store the parsed pattern, which only contains wildcards for correcto saving
        this.value = this.matcher.pattern;
        this.setMinValueCount(this.matcher.totalMinLength);
    }

    // Override to return values with timestamps
    // IMPORTANT: Make sure it works correctly with the given path and the timestamp, which is returned here
    __getFeatureValue($feature, fieldName) {
        if ($feature[fieldName] === undefined) {
            throw new Error(`Field ${fieldName} cannot be found in feature`);
        }

        let featureValue = $feature[fieldName];

        if (featureValue.$vpath && !this.options.isAbsolutePath) {
            /*return {
                whenMs: $feature[fieldName].whenMs,
                value: jsonQuery.first(featureValue, featureValue.$vpath).value
            };*/
            return jsonQuery.first(featureValue, featureValue.$vpath).value;
        }

        /*return {
            whenMs: $feature.whenMs,
            value: $feature[fieldName]
        };*/
        return featureValue;
    }

    __evaluate(values) {
        try {
            // 0th index is the most recent value
            // Reverse order, so that the oldest value is at 0th index
            // reverse is an inplace operator, which makes this function impure, but since values is generated newly every time
            // it's ok for now
            values.reverse();
            const lastNode = this.matcher.treeMatch(values);
            // Make sure the pattern ends with the last value of the input
            // to prevent double matches
            // WARNING: This skips historical matches, which are NOW found due to historical value insertion
            return lastNode.vi + lastNode.length === values.length;
        }
        catch(err) {
            return false;
        }
    }
}

TimeseriesPatternConstraint.fromJson = function fromJson(feature, jsonPattern, typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection) {
    return new TimeseriesPatternConstraint(feature, ArrayPatternMatcher.patternFromJson(jsonPattern), typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection);
};

module.exports = {
    TimeseriesConstraint,
    TimeseriesPatternConstraint
};