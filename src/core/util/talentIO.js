/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

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

class TalentInput {}

TalentInput.getRawValue = function(ev, maxValueCount, includeTimestamp, feature, type, instance, ignoreValuePath = false) {
    return TalentInput.__getValue(ev, VALUE_TYPE_RAW, feature, type, instance, maxValueCount, includeTimestamp, ignoreValuePath);
};

TalentInput.getEncodedValue = function(ev, maxValueCount, includeTimestamp, feature, type, instance) {
    return TalentInput.__getValue(ev, VALUE_TYPE_ENCODED, feature, type, instance, maxValueCount, includeTimestamp);
};

TalentInput.getInstancesFor = function(ev, feature = ev.feature, type = ev.type) {
    return Object.keys(TalentIO.__ensureModel(ev).get(`$features.${type}.'${feature}'`)).filter(instance => instance[0] !== '$');
};

TalentInput.getStats = function(ev, feature = ev.feature, type = ev.type, instance = ev.instance) {
    const model = TalentIO.__ensureModel(ev);
    const $feature = model.get(`$features.${type}.'${feature}'.${instance}.$feature`);

    if (!Object.prototype.hasOwnProperty.call($feature, 'stat')) {
        throw new Error(`No statistical information available for ${type}.${feature}. It's only available for encoded features.`);
    }

    return $feature.stat;
};

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

class FeatureMetadata {}

FeatureMetadata.getUnit = function ($metadata) {
    return TalentIO.__ensureModel($metadata).get('$unit');
};

class TalentOutput {
    constructor() {
        this.outputs = [];
    }

    add(talent, ev, feature, value, subject, type, instance, timestamp) {
        this.outputs.push(TalentOutput.create(talent, ev, feature, value, subject, type, instance, timestamp));
    }

    addFor(subject, type, instance, feature, value, timestamp) {
        this.outputs.push(TalentOutput.createFor(subject, type, instance, feature, value, timestamp));
    }

    toJson() {
        return this.outputs;
    }
}

TalentOutput.create = function(talent, ev, feature, value, subject = ev.subject, type = DEFAULT_TYPE, instance = DEFAULT_INSTANCE, timestamp = Date.now()) {
    return {
        subject,
        type,
        instance,
        value,
        feature: [talent.id, feature].filter(s => s !== null).join('.'),
        whenMs: timestamp
    };
};

TalentOutput.createFor = function(subject, type, instance, feature, value, timestamp) {
    if (typeof subject !== 'string') {
        throw new Error(`The subject needs to be a string, which identifies the instances affiliation`);
    }

    return TalentOutput.create({ id: null }, null, feature, value, subject, type, instance, timestamp);
};

/*
const ev = JSON.parse('{"returnTopic":"iotea/ingestion/events","$features":{"Vehicle":{"test.Body$Lights$IsBrakeOn":{"4711":{"$feature":{"whenMs":1606820429584,"ttlMs":1606820459584,"history":[{"whenMs":1606820427604,"ttlMs":1606820457604,"raw":1,"enc":0.3333333333333333},{"whenMs":1606820425626,"ttlMs":1606820455626,"raw":0,"enc":0}],"raw":2,"enc":0.6666666666666666,"stat":{"cnt":3,"mean":0.3333333333333333,"var":0.1111111111111111,"sdev":0.3333333333333333}},"matches":1},"$metadata":{"description":"Is brake light on","idx":0,"history":20,"encoding":{"type":"number","encoder":"minmax","min":0,"max":3},"unit":"µA","$unit":{"fac":0.000001,"unit":"µA","desc":"Mikroampere","base":{"fac":1,"unit":"A","desc":"Ampere"}}}}}},"type":"Vehicle","feature":"test.Body$Lights$IsBrakeOn","value":2,"whenMs":1606820429584,"instance":"4711","subject":"someuserid","now":1606820429587,"msgType":1,"$metadata":{"description":"Is brake light on","idx":0,"history":20,"encoding":{"type":"number","encoder":"minmax","min":0,"max":3},"unit":"µA","$unit":{"fac":0.000001,"unit":"µA","desc":"Mikroampere","base":{"fac":1,"unit":"A","desc":"Ampere"}}},"segment":"100000","cid":"620ab4a4-a461-43b9-9bba-7def876ec696","$feature":{"whenMs":1606820429584,"ttlMs":1606820459584,"history":[{"whenMs":1606820427604,"ttlMs":1606820457604,"raw":1,"enc":0.3333333333333333},{"whenMs":1606820425626,"ttlMs":1606820455626,"raw":0,"enc":0}],"raw":2,"enc":0.6666666666666666,"stat":{"cnt":3,"mean":0.3333333333333333,"var":0.1111111111111111,"sdev":0.3333333333333333}}}');

console.log(TalentInput.getRawValue(ev));
console.log(TalentInput.getEncodedValue(ev, 10, true));
console.log(TalentInput.getEncodedValue(ev, 1, true, 'test.Body$Lights$IsBrakeOn', 'Vehicle'));
console.log(TalentInput.getStats(ev));
console.log(TalentInput.getInstancesFor(ev));
console.log(FeatureMetadata.getUnit(TalentInput.getMetadata(ev)));

console.log(TalentOutput.create({id: 'test'}, { subject: 'someuserid'}, 'myfeature', 3));
console.log(TalentOutput.createFor('someuserid', 'mytype', 'myinstance', 'thefeature', 5));

const to = new TalentOutput();
to.add({id: 'test2'}, { subject: 'someuserid2'}, 'myfeature2', 1337);
to.add({id: 'test2'}, { subject: 'someuserid2'}, 'myfeature3', 22);
to.addFor('someuserid2', 'mytype', 'myinstance', 'myfeature4', 33, Date.now());
console.log(to.toJson());
*/

module.exports = {
    TalentIO,
    TalentInput,
    TalentOutput,
    FeatureMetadata
};