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
 * Module Talent IO. 
 *
 * @module talentIO
 */
const JsonModel = require('./jsonModel');
const jsonQuery = require('./jsonQuery');

const {
    VALUE_TYPE_ENCODED,
    VALUE_TYPE_RAW,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE
} = require('../constants');


class TalentIO {}

TalentIO.__ensureModel = function(model, validator = null) {
    if (model instanceof JsonModel) {
        return model;
    }

    return new JsonModel(model, validator);
};

/**
 * Provides utility methods for easier access to the entities of json platform events.
 */
class TalentInput {}


/**
 * Gets the raw value out of the json event. 
 * 
 * @param {*} ev - Json platform event.
 * @param {number} maxValueCount - Indicates the maximum number of raw values to be returned from the event and its history.
 * @param {boolean} includeTimestamp - Indicate if the returned object should contain timestamp entry, besides the value.
 * @param {string} feature - The feature whose raw value is requested.
 * @param {string} type - The type of feature whose raw value is requested.
 * @param {string} instance - The instance which together with type and feature identifies the entity of interest.
 * @param {boolean} [ignoreValuePath = false] - Indicates whether to include $vpath json elements.
 * @returns an object or an array of objects, depending on _maxValueCount_ carrying the raw value(s) of the event.
 */
TalentInput.getRawValue = function(ev, maxValueCount, includeTimestamp, feature, type, instance, ignoreValuePath = false) {
    return TalentInput.__getValue(ev, VALUE_TYPE_RAW, feature, type, instance, maxValueCount, includeTimestamp, ignoreValuePath);
};

/**
 * Gets the encoded values of a feature.
 *
 * @param {*} ev - Json event to extract values from.
 * @param {number} maxValueCount - Maximum number of encoded values to be returned. The values are extracted from event
 * value and history.
 * @param {boolean} includeTimestamp - Indicates whether to include the timestamp in the result.
 * @param {string} feature - The feature whose encoded value is requested.
 * @param {string} type - The type of feature whose encoded value is requested.
 * @param {string} instance - The instance which together with type and feature identifies the entity of interest.
 * @returns a single encoded value or an array of values, depending on _maxValueCount_.
 */
TalentInput.getEncodedValue = function(ev, maxValueCount, includeTimestamp, feature, type, instance) {
    return TalentInput.__getValue(ev, VALUE_TYPE_ENCODED, feature, type, instance, maxValueCount, includeTimestamp);
};

/**
 * Gets the instances of a specified feature in an event.
 * 
 * @param {*} ev - Json event to get the instances from.
 * @param {string} [feature = ev.feature] - The feature whose instances are requested.
 * @param {string} [type = ev.type] - The type of feature whose instances are requested.
 * @returns {Array<string>} - An array of strings carrying the instances.
 */
TalentInput.getInstancesFor = function(ev, feature = ev.feature, type = ev.type) {
    return Object.keys(TalentIO.__ensureModel(ev).get(`$features.${type}.'${feature}'`)).filter(instance => instance[0] !== '$');
};

/**
 * Gets the value of the statistics entry of a feature.
 * 
 * @param {*} ev - Json event to get the statistics from.
 * @param {string} [feature = ev.feature] - The feature whose statistics is requested.
 * @param {string} [type = ev.type] - The type of feature whose statistics is requested.
 * @param {string} [instance = ev.instance] - The instance, which together with feature and type identifies the entry of
 * interest.
 * @returns the value of the 'stat' entry of the specified feature.
 */
TalentInput.getStats = function(ev, feature = ev.feature, type = ev.type, instance = ev.instance) {
    const model = TalentIO.__ensureModel(ev);
    const $feature = model.get(`$features.${type}.'${feature}'.${instance}.$feature`);

    if (!Object.prototype.hasOwnProperty.call($feature, 'stat')) {
        throw new Error(`No statistical information available for ${type}.${feature}. It's only available for encoded features.`);
    }

    return $feature.stat;
};

/**
 * Gets the metadata entry of the specified feature.
 * 
 * @param {*} ev - Json event to get the data from.
 * @param {string} feature - Feature whose metadata is requested.
 * @param {string} type - Type of feature whose metadata is requested.
 * @returns - metadata entry object.
 */
TalentInput.getMetadata = function(ev, feature = ev.feature, type = ev.type) {
    return TalentIO.__ensureModel(ev).get(`$features.${type}.'${feature}'.$metadata`);
};

TalentInput.__getValue = function(ev, valueType, feature = ev.feature, type = ev.type, instance = ev.instance, maxValueCount = 1, includeTimestamp, ignoreValuePath = false) {
    if (maxValueCount < 1) {
        throw new Error(`maxValueCount must be greater than 0`);
    }

    const model = TalentIO.__ensureModel(ev);

    const $feature = model.get(`$features.${type}.'${feature}'.${instance}.$feature`);

    const fieldName = valueType === VALUE_TYPE_ENCODED ? 'enc' : 'raw';

    const values = [];

    for (let i = 0; i < maxValueCount; i++) {
        if (i === 0) {
            values.push(TalentInput.__resolveValue($feature, fieldName, includeTimestamp, ignoreValuePath));

            if (maxValueCount === 1) {
                return values[0];
            }
        } else {
            const historyIdx = i - 1;

            if (historyIdx === $feature.history.length) {
                break;
            }

            values.push(TalentInput.__resolveValue($feature.history[historyIdx], fieldName, includeTimestamp, ignoreValuePath));
        }
    }

    return values;
};

TalentInput.__resolveValue = function($feature, fieldName, includeTimestamp, ignoreValuePath) {
    if ($feature[fieldName] === null) {
        return null;
    }

    if ($feature[fieldName].$vpath === undefined || ignoreValuePath) {
        return TalentInput.__enrichValue($feature, $feature[fieldName], includeTimestamp);
    }

    return TalentInput.__enrichValue($feature, jsonQuery.first($feature[fieldName], $feature[fieldName].$vpath).value, includeTimestamp);
};

TalentInput.__enrichValue = function($feature, value, includeTimestamp = false) {
    if (!includeTimestamp) {
        return value;
    }

    return {
        value,
        ts: $feature.whenMs
    };
};

/**
 * Provides utility methods for easier access to the feature metadata of json platform events.
 */
class FeatureMetadata {}

/**
 * Gets the description of a measurement unit.
 * 
 * @param {*} $metadata - Feature metadata. Could be extracted via {@link module:talentIO~TalentInput.getMetadata}.
 * @returns json unit object.
 */
FeatureMetadata.getUnit = function ($metadata) {
    return TalentIO.__ensureModel($metadata).get('$unit');
};

/**
 * Utility class to facilitate the construction of Talent output feature.
 */
class TalentOutput {
    /**
     * Constructs a TalentOutput object with no output features.
     */ 
    constructor() {
        this.outputs = [];
    }

    /**
     * Adds an output feature, bound to a talent, to the list of this TalentOutput. 
     * 
     * @param {*} talent - Json object that carries the talent id, e.g. ```{ id: 'test' }```. 
     * @param {*} ev - Json event object to obtain the default value of _subject_. 
     * @param {string} feature - The feature to create output for.
     * @param {*} value - Value of the output.
     * @param {*} subject - Subject of the output.
     * @param {string} type - Feature type.
     * @param {string} instance - Type instance. 
     * @param {*} timestamp - Timestamp of the output.
     */
    add(talent, ev, feature, value, subject, type, instance, timestamp) {
        this.outputs.push(TalentOutput.create(talent, ev, feature, value, subject, type, instance, timestamp));
    }

    /**
     * Adds an output feature, not bound to a talent, to the list of this TalentOutput. 
     * 
     * @param {*} subject - Subject of the output.
     * @param {string} type - Feature type.
     * @param {string} instance - Type instance. 
     * @param {string} feature - The feature to create output for.
     * @param {*} value - Value of the output.
     * @param {*} timestamp - Timestamp of the output.
     */
    addFor(subject, type, instance, feature, value, timestamp) {
        this.outputs.push(TalentOutput.createFor(subject, type, instance, feature, value, timestamp));
    }

    /**
     * Converts the array of output features to a json object.
     * 
     * @returns a json object.
     */
    toJson() {
        return this.outputs;
    }
}

/**
 * Creates an output feature of a specified talent. 
 * The result will look like this:
 *  <pre>
 *   {
 *       subject: 'someuserid',
 *       type: DEFAULT_TYPE,
 *       instance: DEFAULT_INSTANCE,
 *       value: 22,
 *       feature: 'anotherfeature',
 *       whenMs: now
 *   }
 * </pre>

 * 
 * @param {*} talent - Json object that carries the talent id, e.g. ```{ id: 'test' }```.
 * @param {*} ev - Json event object to obtain the default value of _subject_.
 * @param {string} feature - The feature to create output for.
 * @param {*} value - Value of the output.
 * @param {*} [subject = ev.subject] - Subject of the output.
 * @param {string} [type = DEFAULT_TYPE] - Feature type.
 * @param {string} [instance = DEFAULT_INSTANCE] - Type instance. 
 * @param {*} [timestamp = Date.now()] - Timestamp of the output.
 * @returns json object.
 */
TalentOutput.create = function(talent, ev, feature, value, subject = ev.subject, type = DEFAULT_TYPE, instance = DEFAULT_INSTANCE, timestamp = Date.now()) {
    return {
        subject,
        type,
        instance,
        value,
        feature: [talent !== null ? talent.id : null, feature].filter(s => s !== null).join('.'),
        whenMs: timestamp
    };
};

/**
 * Creates an output feature, not bound to a talent.
 * 
 * The result will look like this:  
 * <pre>
    {
        subject: 'someuserid',
        type: DEFAULT_TYPE,
        instance: DEFAULT_INSTANCE,
        value: 22,
        feature: 'anotherfeature',
        whenMs: now
    }
 * </pre>
 * @param {*} subject - Subject of the output.
 * @param {*} type - Type of the feature.
 * @param {*} instance - Type instance.
 * @param {*} feature - Feature of the output.
 * @param {*} value - Value of the ouput.
 * @param {*} timestamp - Timestamp of the output.
 * @returns a json object.
 */
TalentOutput.createFor = function(subject, type, instance, feature, value, timestamp) {
    if (typeof subject !== 'string') {
        throw new Error(`The subject needs to be a string, which identifies the instances affiliation`);
    }

    return TalentOutput.create(null, null, feature, value, subject, type, instance, timestamp);
};

module.exports = {
    TalentIO,
    TalentInput,
    TalentOutput,
    FeatureMetadata
};
