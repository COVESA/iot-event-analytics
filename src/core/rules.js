/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const uuid = require('uuid');
const Ajv = require('ajv');

const equals = require('./util/equals');
const Instance = require('./instance');

const {
    DEFAULT_TYPE,
    VALUE_TYPE_RAW,
    VALUE_TYPE_ENCODED
} = require('./constants');

const jsonQuery = require('./util/jsonQuery');

const Logger = require('./util/logger');

const PATH_IDENTITY = '';
const ANY_FEATURE = '*';
const ALL_TYPES = '*';
const ALL_SEGMENTS = '*';
const ALL_INSTANCE_IDS_FILTER = '.*';

class Constraint {
    constructor(feature, op, value, typeSelector, valueType, path = PATH_IDENTITY, instanceIdFilter = ALL_INSTANCE_IDS_FILTER, limitFeatureSelection = true) {
        this.options = {
            isAbsolutePath: false
        };

        this.feature = feature;
        this.op = op;

        // If path starts with / it is an absolute path. A given $vpath will be ignored
        if (path.indexOf('/') === 0) {
            this.options.isAbsolutePath = true;
            path = path.substring(1);
        }

        this.path = path;
        this.value = value;
        this.valueType = valueType;
        this.typeSelector = typeSelector;
        this.instanceIdMatcher = new RegExp(instanceIdFilter, 'm');
        // If feature == ANY_FEATURE, this limits the features, which are actually populated in the event to the given feature in the event
        this.limitFeatureSelection = limitFeatureSelection;

        if (!this.limitFeatureSelection && this.feature !== ANY_FEATURE) {
            Constraint.logger.warn(`The flag limitFeatureSelection=false has only an effect if feature is set to constant ANY_FEATURE. Currently the feature is set to "${this.feature}"`);
        }

        // @param typeSelector
        // *.kuehlschrank, .kuehlschrank, kuehlschrank
        // --> all refer to type kuehlschrank --> since types are unique --> * for segment will also deliver a single type
        // 100000.kuehschrank
        // --> is the single type kuehlschrank belonging to segment 100000
        // 100000.*
        // --> matches all types contained in the segment
        // *.*
        // --> Checks all instance for the existence of the given feature
        // eslint-disable-next-line no-useless-escape
        const rg = /(?:^(\*|(?:[^\.]+))\.)?([^\.]+)$/m;
        const match = rg.exec(this.typeSelector);

        // Segment may be undefined or an empty string
        this.segment = match[1] && match[1] !== '' ? match[1] : null;

        // Can be a specific type or *
        this.type = match[2];
    }

    getTypeFeature() {
        return Rules.getTypeFeature(this.type, this.feature, this.segment);
    }

    evaluate($feature) {
        Constraint.logger.verbose(`Evaluating valueType ${this.valueType === VALUE_TYPE_RAW ? 'raw': 'enc'} of feature ${JSON.stringify($feature)}...`);

        let featureValue = this.valueType === VALUE_TYPE_RAW ? $feature.raw : $feature.enc;

        try {
            if (featureValue.$vpath && !this.options.isAbsolutePath) {
                Constraint.logger.verbose(`Picking value from given value path = ${featureValue.$vpath}`)
                featureValue = jsonQuery.first(featureValue, featureValue.$vpath).value;
            }
        }
        catch(err) {
            return false;
        }

        Constraint.logger.verbose(`Reading value via jsonQuery from ${JSON.stringify(featureValue)} at path ${this.path}`);

        let valueFoundForPath = false;

        for (let result of jsonQuery(featureValue, this.path)) {
            valueFoundForPath = true;

            let prevFeatureValue = null;

            if ($feature.history.length > 0) {
                try {
                    const lastHistoryEntry = $feature.history[0];

                    prevFeatureValue = this.valueType === VALUE_TYPE_RAW ? lastHistoryEntry.raw : lastHistoryEntry.enc;

                    if (prevFeatureValue.$vpath && !this.options.isAbsolutePath) {
                        Constraint.logger.verbose(`Picking previous value from given value path = ${prevFeatureValue.$vpath}`);
                        prevFeatureValue = jsonQuery.first(prevFeatureValue, prevFeatureValue.$vpath).value;
                    }

                    prevFeatureValue = jsonQuery.first(prevFeatureValue, result.query).value;
                }
                catch(err) {
                    // No worries, previous value not available
                }
            }

            if (!this.__evaluate(result.value, prevFeatureValue)) {
                return false;
            }
        }

        // Just in case no value was found - return false
        return valueFoundForPath;
    }

    __evaluate() {
        // Parameters: value, previousValue
        return false;
    }

    toString() {
        return `${Constraint.stringifyValueType(this.valueType)} value of "${this.segment !== null ? `${this.segment}.`: ''}${this.type}.${this.feature}" at path "${this.path}"`;
    }
}

Constraint.logger = new Logger('Constraint');

Constraint.stringifyValueType = function stringifyValueType(valueType) {
    switch(valueType) {
        case VALUE_TYPE_RAW: return 'RAW';
        case VALUE_TYPE_ENCODED: return 'ENCODED';
        default: throw new Error(`Value type not found for key ${valueType}`);
    }
};

Constraint.OPS = {
    SCHEMA: 0,
    CHANGE: 1,
    NELSON: 2,
    TIMESERIES_PATTERN: 3
};

class SchemaConstraint extends Constraint {
    constructor(feature, value, typeSelector = DEFAULT_TYPE, valueType = VALUE_TYPE_ENCODED, path = PATH_IDENTITY, instanceIdFilter, limitFeatureSelection, sid = `${uuid.v1()}.json`) {
        super(feature, Constraint.OPS.SCHEMA, value, typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection);

        const ajv = new Ajv();

        // Assume, that this.value is only one schema
        this.value.$id = sid;

        ajv.addSchema(this.value);

        this.validator = ajv.getSchema(sid);

        if (this.validator === undefined) {
            throw new Error(`Schema ${sid} cannot be resolved. If you specified an array of schemas, make sure to supply the $id of the schema to use as last parameter of the constructor.`);
        }
    }

    __evaluate(value) {
        SchemaConstraint.logger.verbose(`Evaluating ${JSON.stringify(value)} using template ${JSON.stringify(this.value)} for feature ${this.feature}...`);
        return this.validator(value);
    }
}

SchemaConstraint.logger = new Logger('SchemaConstraint');

class OpConstraint extends SchemaConstraint {
    constructor(feature, op, value, typeSelector = DEFAULT_TYPE, valueType = VALUE_TYPE_ENCODED, path = PATH_IDENTITY, instanceIdFilter, limitFeatureSelection) {
        super(feature, OpConstraint.createSchema(op, value), typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection);
    }
}

OpConstraint.createSchema = (op, value) => {
    switch(op) {
        case OpConstraint.OPS.ISSET: {
            return {
                not: {
                    type: 'null'
                }
            };
        }
        case OpConstraint.OPS.EQUALS: {
            return {
                const: value
            };
        }
        case OpConstraint.OPS.NEQUALS: {
            return {
                not: {
                    const: value
                }
            }
        }
        case OpConstraint.OPS.LESS_THAN: {
            return {
                type: 'number',
                exclusiveMaximum: value
            };
        }
        case OpConstraint.OPS.LESS_THAN_EQUAL: {
            return {
                type: 'number',
                maximum: value
            };
        }
        case OpConstraint.OPS.GREATER_THAN: {
            return {
                type: 'number',
                exclusiveMinimum: value
            };
        }
        case OpConstraint.OPS.GREATER_THAN_EQUAL: {
            return {
                type: 'number',
                minimum: value
            };
        }
        case OpConstraint.OPS.REGEX: {
            // Check if given is RegEx instance already
            // Ignore regex flags since pattern does not support it --> make regex support it by itself
            if (value instanceof RegExp) {
                value = value.source;
            }

            return {
                type: 'string',
                pattern: value
            };
        }
        default: {
            throw new Error(`Invalid operation ${op} given`);
        }
    }
};

OpConstraint.OPS = {
    ISSET: 10,
    EQUALS: 11,
    NEQUALS: 12,
    LESS_THAN: 20,
    LESS_THAN_EQUAL: 21,
    GREATER_THAN: 22,
    GREATER_THAN_EQUAL: 23,
    REGEX: 30
};

class ChangeConstraint extends Constraint {
    constructor(feature, typeSelector = DEFAULT_TYPE, valueType = VALUE_TYPE_ENCODED, path = PATH_IDENTITY, instanceIdFilter, limitFeatureSelection) {
        super(feature, Constraint.OPS.CHANGE, null, typeSelector, valueType, path, instanceIdFilter, limitFeatureSelection);
    }

    __evaluate(value, previousValue) {
        return !equals(value, previousValue);
    }
}

class Rule {
    constructor(constraint = null) {
        if (constraint !== null && !(constraint instanceof Constraint)) {
            throw new Error(`Given constraint needs to be null or of type Constraint`);
        }

        this.constraint = constraint;
    }

    async evaluate(subject, type, feature, instanceId, featureMap, instanceManager) {
        // **************************** GET ALL MATCHING INSTANCES ******************************

        // Only for rules of the same type as the given one take instanceId for instanceIdMatcher
        // For others, take instanceIdMatcher
        let instanceIdMatcher = this.constraint.instanceIdMatcher;

        if (type === this.constraint.type && this.constraint.instanceIdMatcher.source === ALL_INSTANCE_IDS_FILTER) {
            // If the given type and the constraint type are equal, then filter instances with the given
            // instanceId to group features of the same type to the same instance. Does not override instanceIdMatcher
            instanceIdMatcher = new RegExp(`^${instanceId}$`, 'm');
        }

        let instances = [];

        try {
            // Get all instances of the given subject
            instances = instanceManager.getInstances(subject, instanceIdMatcher);
        }
        catch(err) {
            // Given subject could not be found
            Rule.logger.verbose(`Subject ${subject} with instanceId matcher ${instanceIdMatcher.source} could not be found`);
        }
        finally {
            instanceIdMatcher.lastIndex = 0;

            // Check if the given instanceId of the current features is matching the instanceIdMatcher
            // Check if the instance of the current feature is not already in the instances array
            if (instanceId.match(instanceIdMatcher) !== null && instances.find(instance => instance.type === type && instance.id === instanceId) === undefined) {
                instances.push(new Instance(type, instanceId, Rule.logger));
            }
        }

        Rule.logger.verbose(`Found ${instances.length} instances`);

        // ******************** FILTER INSTANCES BASED ON GIVEN CONSTRAINT **********************

        // Filter instances by given segment or type
        if (this.constraint.segment !== null || this.constraint.type !== ALL_TYPES) {
            if (this.constraint.type !== ALL_TYPES) {
                // Filter by given type
                Rule.logger.verbose(`Filtering instances by type ${this.constraint.type}...`);
                instances = instances.filter(instance => instance.type === this.constraint.type);
            } else if (this.constraint.segment !== ALL_SEGMENTS) {
                // Filter by segment
                // Check if instance is of given segment
                const filteredInstances = [];

                for (let instance of instances) {
                    try {
                        if (await instanceManager.getMetadataManager().resolveSegment(instance.type) === this.constraint.segment) {
                            // Take instance
                            filteredInstances.push(instance);
                        }
                    } catch(err) {
                        // Just continue, segment could not be resolved
                    }
                }

                // If instance is of same type as given type, filter it again, that only instances are valid with the same instanceId
                instances = filteredInstances.filter(filteredInstance => filteredInstance.type !== type || filteredInstance.id === instanceId);
            }
        }

        // **************************** EVALUATE GIVEN CONSTRAINT *******************************

        // Now we have all instances which should be evaluated
        let totalMatchCount = 0;
        let constraintFeature = this.constraint.feature;

        if (constraintFeature === ANY_FEATURE) {
            constraintFeature = feature;
        }

        Rule.logger.verbose(`${instances.length} instances remain after filtering`);

        for (let instance of instances) {
            try {
                // Filtering for segment and/or type is already done by filtering the instances
                // Now it's about getting the feature to evaluate

                // Pick the feature, given by the constraint from the given instance
                // <type>.<feature>
                // <segment>.*.<feature>
                // *.*.<feature>            -> <instanceType>.<feature>

                // Pick the feature, given by the event from the given instance
                // <type>.*
                // <segment>.*.*
                // *.*.*                    -> <instanceType>.<eventFeature>

                // Selection Matrix
                //
                // Only features, which actually have a value are contained in the selection matrix
                //
                // <type>.<feature>         -> select given feature from all instances of the given type        -> select <constraintFeature> from instance
                // <type>.*                 -> select all features from all instances of the given type         -> select all features from instance
                // <segment>.*.<feature>    -> select given feature from all instances of the given segment     -> select <constraintFeature> from instance
                // <segment>.*.*            -> select all features from all instances of a the given segment    -> select all features from instance
                // *.*.<feature>            -> select given feature from all instances                          -> select <constraintFeature> from instance
                // *.*.*                    -> select all feature from all instances                            -> select all features from instance

                // Get the <constraintFeature> from the instance
                let featureEntry = null;
                let $feature = null;

                try {
                    Rule.logger.verbose(`Getting feature ${instance.type}.${constraintFeature} from instance ${instance.id}...`);
                    featureEntry = featureMap.get(instance.type, constraintFeature, instance.id);
                    $feature = featureEntry.$feature;
                }
                catch(err) {
                    const $metaFeature = await instanceManager.getMetadataManager().resolveMetaFeature(instance.type, constraintFeature);
                    // The change date of a feature can be newer than the current event, since we have to accept any event order
                    // The $feature needs to be cloned, so all evaluations on the same $feature are based on the same dataset
                    $feature = await instanceManager.getFeature(subject, instance.id, constraintFeature, instance.type, true, $metaFeature);
                    featureMap.set(instance.type, constraintFeature, instance.id, $feature, $metaFeature);
                    featureEntry = featureMap.get(instance.type, constraintFeature, instance.id);
                }

                // Feature cannot be null here, since an exception is thrown otherwise and caught by the outer catch block
                Rule.logger.verbose(`Evaluating feature ${JSON.stringify($feature)}...`);

                if (this.constraint.evaluate($feature)) {
                    Rule.logger.verbose(`Successfully evaluated feature ${JSON.stringify($feature)} agains constraint ${this.constraint}`);
                    featureMap.recordRuleMatchFor(instance.type, constraintFeature, instance.id);
                    totalMatchCount++;
                }
            }
            catch(err) {
                // Error should be logged as debug
                Rule.logger.debug(`Failed to get feature ${instance.type}.${constraintFeature} from instance ${instance.id}`);
                Rule.logger.debug(`Error was: ${err.message}`);
            }
            finally {
                if (this.constraint.feature === ANY_FEATURE && !this.constraint.limitFeatureSelection) {
                    // Store all remaining features (which are actually set) into the featureMap, if <ANY feature> is selected and <limitFeatureSelection> flag is set to false

                    for (const [idx, $feat] of instance.getFeatures().entries()) {
                        // Skip undefined (uninitialized) and null (unset) entries
                        if ($feat === null || $feat === undefined) {
                            continue;
                        }

                        try {
                            feature = await instanceManager.getMetadataManager().resolveFeatureAt(instance.type, idx);

                            if (!featureMap.contains(instance.type, feature, instance.id)) {
                                // Do not overwrite any existing value in the feature map
                                const $metaFeature = await instanceManager.getMetadataManager().resolveMetaFeature(instance.type, feature);
                                featureMap.set(instance.type, feature, instance.id, $feat, $metaFeature);
                            }
                        }
                        catch(err) {
                            // meta info or feature could not be found
                        }
                    }
                }
            }
        }

        return totalMatchCount > 0;
    }

    save() {
        let path = this.constraint.path;

        if (this.constraint.options.isAbsolutePath) {
            path = '/' + path;
        }

        return {
            path,
            feature: this.constraint.feature,
            op: this.constraint.op,
            value: this.constraint.value,
            valueType: this.constraint.valueType,
            typeSelector: this.constraint.typeSelector,
            instanceIdFilter: this.constraint.instanceIdMatcher.source,
            limitFeatureSelection: this.constraint.limitFeatureSelection
        };
    }

    getUniqueTypeFeatures(typeFeatures = []) {
        const typeFeature = this.constraint.getTypeFeature();

        if (!Rules.typeFeaturesContain(typeFeatures, typeFeature)) {
            typeFeatures.push(typeFeature);
        }

        return typeFeature;
    }

    omitInstanceId(instanceId, typeFeature) {
        if (!Rules.typeFeaturesMatch(this.constraint.getTypeFeature(), typeFeature)) {
            return true;
        }

        // No match found
        return instanceId.match(this.constraint.instanceIdMatcher) === null;
    }
}

Rule.logger = new Logger('Rule');

class Rules extends Rule {
    constructor(excludeOn = null) {
        super();
        this.rules = [];
        this.excludeOn = null;

        if (excludeOn !== null) {
            if (!Array.isArray(excludeOn)) {
                throw new Error(`excludeOn parameter of Rules must be either null or an Array of strings`);
            }

            if (excludeOn.length > 0) {
                this.excludeOn = excludeOn;
            }
        }

        this.init();
    }

    init() {
        if (this.excludeOn === null) {
            return;
        }

        // Stores the processed excludeOn entries
        this.__excludeOn = [];

        // eslint-disable-next-line no-useless-escape
        const regex = new RegExp(/^([^\.]+)\.([^\.]+)(?:\.([^\.]+))?$/, 'm');

        for (let typeFeatureSelector of this.excludeOn) {
            const matches = typeFeatureSelector.match(regex);

            regex.lastIndex = 0;

            if (matches === null) {
                throw new Error(`Invalid typeFeature selector "${typeFeatureSelector}" found in excludeOn constraints in given Rules`);
            }

            const entry = {
                type: matches[1],
                feature: matches[2],
                talentNs: null
            };

            if (matches[3] !== undefined) {
                // Selector for specific talent output given
                // Only allow default.<talentId>.* and default.<talentId>.<feature>
                if (matches[1] !== DEFAULT_TYPE) {
                    // Only allow default as type
                    throw new Error(`Invalid typeFeature selector "${typeFeatureSelector}". Has to be default type`);
                }

                if (matches[2] === '*') {
                    // Given talentId must not be a wildcard
                    throw new Error(`Talent id has to be defined in typeFeature selector "${typeFeatureSelector}"`);
                }

                entry.talentNs = matches[2];
                entry.feature = matches[3];
            }

            this.__excludeOn.push(entry);
        }
    }

    add(rules) {
        if (!Array.isArray(rules)) {
            // Only one Rule
            this.rules.push(rules);
        } else {
            this.rules = [...this.rules, ...rules];
        }

        return this;
    }

    shouldExcludeOn(type, feature) {
        if (this.excludeOn === null) {
            return false;
        }

        for (let entry of this.__excludeOn) {
            if (entry.type !== ALL_TYPES && entry.type !== type) {
                // Check for equal types, if type is not a wildcard. If types differ, just skip this entry
                continue;
            }

            if (entry.type === ALL_TYPES && entry.feature === ANY_FEATURE) {
                // *.* -> Exclude on any given type, feature combination -> This rule will never be evaluated
                return true;
            }

            if (entry.talentNs !== null) {
                if (entry.feature === ANY_FEATURE) {
                    // The entry only defines a specific type and talent namespace.
                    // default.<talentNs>.* -> just check, if given feature starts with "<talentNs>."
                    if (feature.indexOf(`${entry.talentNs}.`) === 0) {
                        return true;
                    }

                    continue;
                }

                // Entry specifies also the feature
                // default.<talentNs>.<feature> -> check type and if given feature equals <talentNs>.<feature>
                if (feature === `${entry.talentNs}.${entry.feature}`) {
                    return true;
                }

                continue;
            }

            if (entry.feature === ANY_FEATURE) {
                // <type>.* -> just check if type is equal to given type
                // Since type was already checked at the beginning and since the type cannot be a wildcard here
                return true;
            }

            // *.<feature> -> just check if feature is equal to given feature
            // <type>.<feature> -> check type and feature to given values
            // Since type was already checked for equality at the beginning, we do not need to check it again
            if (entry.feature === feature) {
                return true;
            }
        }

        return false;
    }

    evaluate() {
        // Parameters: subject, type, feature, instance, featureMap, instanceManager
        return false;
    }

    getUniqueTypeFeatures(typeFeatures = []) {
        this.rules.forEach(rule => rule.getUniqueTypeFeatures(typeFeatures));
        return typeFeatures;
    }

    omitInstanceId(instanceId, typeFeature) {
        for (let rule of this.rules) {
            if (!rule.omitInstanceId(instanceId, typeFeature)) {
                return false;
            }
        }

        return true;
    }

    *forEach() {
        for (let rule of this.rules) {
            yield rule;

            if (rule instanceof Rules) {
                yield * rule.forEach();
            }
        }
    }

    save() {
        return {
            excludeOn: this.excludeOn,
            rules: this.rules.map(rule => rule.save())
        };
    }
}

Rules.getTypeFeature = function getTypeFeature(type, feature, segment = null) {
    return {
        type,
        feature,
        segment
    };
};

Rules.typeFeaturesContain = function typeFeaturesContain(typeFeatures, typeFeature) {
    return typeFeatures.find(existingTypeFeature => Rules.typeFeaturesMatch(existingTypeFeature, typeFeature)) !== undefined;
};

Rules.typeFeaturesMatch = function typeFeaturesMatch(tf1, tf2) {
    // Since types are unique, two segments could not contain the same types
    // Type similarity is a sufficient criterium for segment equality
    const typesMatch = tf1.type === '*' || tf2.type == '*' || tf1.type === tf2.type;
    const featuresMatch = tf1.feature === '*' || tf2.feature == '*' || tf1.feature === tf2.feature;
    return typesMatch && featuresMatch;
};

class AndRules extends Rules {
    constructor(rules, excludeOn) {
        super(excludeOn);
        this.add(rules);
    }

    async evaluate(subject, type, feature, instance, featureMap, instanceManager, parentRule = null) {
        if (this.shouldExcludeOn(type, feature)) {
            return parentRule === null ? false : null;
        }

        let andFulfilled = true;

        for(const rule of this.rules) {
            // Iterate through every rule to have a complete featureMap in the end
            if (await rule.evaluate(subject, type, feature, instance, featureMap, instanceManager, this) === false) {
                andFulfilled = false;
            }
        }

        return andFulfilled;
    }

    save() {
        return Object.assign(
            super.save(),
            {
                type: 'and'
            }
        );
    }
}

class OrRules extends Rules {
    constructor(rules, excludeOn) {
        super(excludeOn);
        this.add(rules);
    }

    async evaluate(subject, type, feature, instance, featureMap, instanceManager, parentRule = null) {
        if (this.shouldExcludeOn(type, feature)) {
            return parentRule === null ? false : null;
        }

        let orFulfilled = false;

        for(const rule of this.rules) {
            // Iterate through every rule to have a complete featureMap in the end
            if (await rule.evaluate(subject, type, feature, instance, featureMap, instanceManager, this) === true) {
                orFulfilled = true;
            }
        }

        return orFulfilled;
    }

    save() {
        const result = Object.assign(
            super.save(),
            {
                type: 'or'
            }
        );

        return result;
    }
}

module.exports = {
    PATH_IDENTITY,
    ANY_FEATURE,
    ALL_TYPES,
    ALL_SEGMENTS,
    ALL_INSTANCE_IDS_FILTER,
    Constraint,
    OpConstraint,
    ChangeConstraint,
    SchemaConstraint,
    Rule,
    Rules,
    AndRules,
    OrRules
};