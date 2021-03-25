/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const fs = require('fs');
const path = require('path');
const Ajv = require('ajv');

const {
    Constraint,
    SchemaConstraint,
    ChangeConstraint,
    AndRules,
    OrRules,
    Rule
} = require('./rules');

const {
    NelsonConstraint
} = require('./rules.nelson');

const {
    TimeseriesPatternConstraint
} = require('./rules.tseries');

class RulesLoader {}

RulesLoader.__loadConstraint = function __loadConstraint(json) {
    switch(json.op) {
        case Constraint.OPS.SCHEMA: {
            return new SchemaConstraint(json.feature, json.value, json.typeSelector, json.valueType, json.path, json.instanceIdFilter, json.limitFeatureSelection);
        }
        case Constraint.OPS.CHANGE: {
            return new ChangeConstraint(json.feature, json.typeSelector, json.valueType, json.path, json.instanceIdFilter, json.limitFeatureSelection)
        }
        case Constraint.OPS.NELSON: {
            return NelsonConstraint.fromJson(json);
        }
        case Constraint.OPS.TIMESERIES_PATTERN: {
            return TimeseriesPatternConstraint.fromJson(json.feature, json.value, json.typeSelector, json.valueType, json.path, json.instanceIdFilter, json.limitFeatureSelection);
        }
    }

    throw new Error(`Invalid operation ${json.op} given`);
};

RulesLoader.load = function load(json) {
    if (!RulesLoader.rulesValidator(json)) {
        throw new Error(`Invalid rule configuration found`);
    }

    return RulesLoader.__load(json);
}

RulesLoader.__load = function __load(json) {
    let rules = new AndRules([], json.excludeOn);

    if (json.type === 'or') {
        rules = new OrRules([], json.excludeOn);
    }

    rules.rules = json.rules.map(json => {
        if (json.rules) {
            return RulesLoader.__load(json);
        }

        return new Rule(RulesLoader.__loadConstraint(json));
    });

    return rules;
};

RulesLoader.rulesValidator = new Ajv({
    schemas: [
        JSON.parse(fs.readFileSync(path.normalize(path.join(__dirname, '../../resources/rules.schema.json'))), { encoding: 'utf8' })
    ]
}).getSchema('http://example.com/schemas/rules.schema.json');

module.exports = RulesLoader;