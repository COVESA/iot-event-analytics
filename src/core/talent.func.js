/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Talent = require('./talent');

const {
    Rule,
    OrRules,
    SchemaConstraint
} = require('./rules');

const {
    TalentInput,
    TalentOutput
} = require('./util/talentIO');

const {
    DEFAULT_TYPE,
    VALUE_TYPE_RAW,
    ENCODING_TYPE_OBJECT,
    ENCODING_TYPE_ANY,
    MSG_TYPE_ERROR
} = require('./constants');

module.exports = class FunctionTalent extends Talent {
    constructor(id, protocolGatewayConfig) {
        super(id, protocolGatewayConfig);
        this.functions = {};
        this.functionInputFeatures = [];
    }

    registerFunction(name, cb) {
        this.functions[name] = cb;

        this.skipCycleCheckFor(`${DEFAULT_TYPE}.${this.id}.${name}-in`);

        // Define input parameters as well, so that they exist
        this.addOutput(`${name}-in`, {
            description: `Argument(s) for function ${name}`,
            // Only exists while the actual event is in the pipeline. Do not store this in the instance
            ttl: 0,
            history: 0,
            encoding: {
                type: ENCODING_TYPE_OBJECT,
                encoder: null
            },
            unit: 'ONE'
        });

        this.addOutput(`${name}-out`, {
            description: `Result of function ${name}`,
            // Only exists while the actual event is in the pipeline. Do not store this in the instance
            ttl: 0,
            history: 0,
            encoding: {
                type: ENCODING_TYPE_ANY,
                encoder: null
            },
            unit: 'ONE'
        });

        this.functionInputFeatures.push(`${this.id}.${name}-in`);
    }

    getRules() {
        // It's not required to be overridden
        return null;
    }

    async __processEvent(ev) {
        await super.__processEvent(ev, this.__processFunctionEvents.bind(this));
    }

    async __processFunctionEvents(ev, evtctx) {
        if (this.functionInputFeatures.indexOf(ev.feature) === -1) {
            try {
                return await this.onEvent(ev, evtctx);
            }
            catch(err) {
                // onEvent not implemented or execution error occurred calling onEvent
                this.logger.warn(err.message, evtctx, err);
                return;
            }
        }

        // Process function invocations
        const rawValue = TalentInput.getRawValue(ev);
        const args = rawValue.args;

        args.push(ev);
        args.push(evtctx);
        args.push(rawValue.timeoutAtMs);

        let result = null;

        let $tsuffix = `/${rawValue.chnl}/${rawValue.call}`;

        try {
            result = TalentOutput.create(this, ev, `${rawValue.func}-out`, {
                $tsuffix,
                $vpath: 'value',
                value: await this.functions[rawValue.func](...args)
            });
        }
        catch(err) {
            result = TalentOutput.create(this, ev, `${rawValue.func}-out`, {
                $tsuffix,
                $vpath: 'error',
                error: err.message
            });

            result.msgType = MSG_TYPE_ERROR;
        }

        return [ result ];
    }

    __getRules() {
        const functionNames = Object.keys(this.functions);

        if (functionNames.length === 0) {
            // returns 1) or 2) -> see Talent.__getRules()
            const rules = super.__getRules();

            if (rules === null) {
                // getRules() not overridden, callees() returns empty array, no functions registered
                throw new Error(`You have to at least register a function or override the getRules() method.`);
            }

            return rules;
        }

        /**
         * 3) OR --> Non-triggerable Function Talent, which does not call any functions by itself
         *      function input rules
         *
         * 4) OR --> Triggerable Function Talent, which does not call any functions by itself
         *      function input rules
         *      OR/AND [exclude function input rules]
         *        triggerRules
         *
         * 5) OR --> Triggerable Function Talent, which calls one or more functions
         *      function output rules i.e. callee rules
         *      OR [exclude function output rules]
         *        function result rules
         *        OR/AND [exclude function result rules]
         *          triggerRules
         */

        const functionInputRules = new OrRules(functionNames.map(functionName => {
            const eventSchema = {
                type: 'object',
                required: [ 'func', 'args', 'chnl', 'call', 'timeoutAtMs' ],
                properties: {
                    func: {
                        type: 'string',
                        const: functionName
                    },
                    args: {
                        type: 'array'
                    },
                    chnl: {
                        type: 'string'
                    },
                    call: {
                        type: 'string'
                    },
                    timeoutAtMs: {
                        type: 'integer'
                    }
                },
                additionalProperties: false
            };

            return new Rule(new SchemaConstraint(`${this.id}.${functionName}-in`, eventSchema, DEFAULT_TYPE, VALUE_TYPE_RAW));
        }));

        let triggerRules = this.getRules();
        let functionResultRules = this.__getFunctionResultRules();

        if (triggerRules === null && functionResultRules === null) {
            // return 3)
            return functionInputRules;
        }

        if (triggerRules !== null) {
            triggerRules.excludeOn = functionNames.map(functionName => `${DEFAULT_TYPE}.${this.id}.${functionName}-in`)
            functionInputRules.add(triggerRules);

            if (functionResultRules === null) {
                // return 4)
                return functionInputRules;
            }
        }

        functionInputRules.excludeOn = this.callees().map(callee => `${DEFAULT_TYPE}.${callee}-out`);
        functionResultRules.add(functionInputRules);

        // return 5)
        return functionResultRules;
    }
}