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
    AndRules,
    Rule,
    OpConstraint,
    OrRules
} = require('./rules');

const {
    DEFAULT_TYPE,
    VALUE_TYPE_RAW,
    ENCODING_TYPE_NUMBER,
    ENCODING_TYPE_ANY,
    ENCODING_TYPE_OBJECT,
    DEFAULT_INSTANCE
} = require('./constants');

const {
    TalentInput,
    TalentOutput
} = require('./util/talentIO');

class Mapper extends Talent {
    constructor(id, reducerId, protocolGatewayConfig) {
        super(id, protocolGatewayConfig);

        this.reducerId = reducerId;

        this.skipCycleCheckFor(
            // type.talent-id.feature
            this.__getReduceEndFeature(DEFAULT_TYPE),
            this.__getMapStartFeature(DEFAULT_TYPE)
        );

        this.addOutput(Mapper.FEATURE_MAP_START, {
            encoding: {
                type: ENCODING_TYPE_NUMBER,
                encoder: null
            },
            description: 'Mapping has started at that given time in ms since 01.01.1970',
            default: -1
        });

        this.addOutput(Mapper.FEATURE_MAP_ASSIGN, {
            encoding: {
                type: ENCODING_TYPE_ANY,
                encoder: null
            },
            description: 'Single input for worker'
        });

        this.addOutput(Mapper.FEATURE_MAP_PARTIAL, {
            encoding: {
                type: ENCODING_TYPE_OBJECT,
                encoder: null
            },
            default: [],
            description: 'Output for all workers'
        });
    }

    getRules() {
        const rules = [
            // Value is needed to determine whether a reduction process is currently running
            new Rule(
                new OpConstraint(this.__getMapStartFeature(), OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            ),
            // Value is needed to determine whether a reduction process is currently running
            new Rule(
                new OpConstraint(this.__getReduceEndFeature(), OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            )
        ];

        return new OrRules([ new AndRules(rules), this.getTriggerRules() ]);
    }

    getTriggerRules() {
        throw new Error(`Override getTriggerRules() and return AndRule, OrRule or Rule instance`);
    }

    async map() {
        // Parameters: ev
        throw new Error(`Override map(ev) and return an array. Each array entry will be sent to a worker.`);
    }

    async onEvent(ev, evtctx) {
        if (ev.feature === this.__getMapStartFeature() || ev.feature === this.__getReduceEndFeature()) {
            // These events should not trigger any calculation process
            return;
        }

        let mapStartAt = -1;
        let reduceEndAt = -1;

        try {
            mapStartAt = TalentInput.getRawValue(ev, 1, false, this.__getMapStartFeature(), DEFAULT_TYPE, DEFAULT_INSTANCE);
            reduceEndAt = TalentInput.getRawValue(ev, 1, false, this.__getReduceEndFeature(), DEFAULT_TYPE, DEFAULT_INSTANCE);
        }
        catch(err) {
            // Omit error
        }

        if (mapStartAt > 0 && mapStartAt > reduceEndAt) {
            // Reduction process is currently running
            this.logger.info('Waiting for reduction to finish...', evtctx);
            return;
        }

        const workPackages = await this.map(ev);

        if (workPackages.length === 0) {
            return;
        }

        this.logger.debug(`Mapper ${this.id} distributes work at ${new Date(ev.whenMs)}. Work packages ${JSON.stringify(workPackages)}`, evtctx);

        const partialResults = new Array(workPackages.length).fill(null);

        const talentOutput = new TalentOutput();

        // Store mapping start time
        talentOutput.add(this, ev, Mapper.FEATURE_MAP_START, Date.now());

        // Place for workers to write result
        talentOutput.add(this, ev, Mapper.FEATURE_MAP_PARTIAL, partialResults);

        // Jobs for workers
        // Ensure different timestamps for all work packages
        for (let i = 0; i < workPackages.length; i++) {
            talentOutput.add(this, ev, Mapper.FEATURE_MAP_ASSIGN, {
                idx: i,
                value: workPackages[i]
            }, ev.subject, DEFAULT_TYPE, DEFAULT_INSTANCE, Date.now() + i);
        }

        return talentOutput.toJson();
    }

    __getReduceEndFeature(type) {
        return this.getFullFeature(this.reducerId, Reducer.FEATURE_MAP_END, type);
    }

    __getMapStartFeature(type) {
        return this.getFullFeature(this.id, Mapper.FEATURE_MAP_START, type);
    }

    __getMapPartialFeature(type) {
        return this.getFullFeature(this.id, Mapper.FEATURE_MAP_PARTIAL, type);
    }

    __getMapAssignFeature(type) {
        return this.getFullFeature(this.id, Mapper.FEATURE_MAP_ASSIGN, type);
    }
}

Mapper.FEATURE_MAP_START = 'map_start';
Mapper.FEATURE_MAP_ASSIGN = 'map_assign';
Mapper.FEATURE_MAP_PARTIAL = 'map_partial';

class Worker extends Talent {
    constructor(id, mapperId, protocolGatewayConfig) {
        super(id, protocolGatewayConfig);
        this.mapperId = mapperId;
    }

    getRules() {
        const rules = new AndRules([
            new Rule(
                new OpConstraint(this.__getMapAssignFeature(), OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            )
        ]);

        return rules;
    }

    async work() {
        // Parameters: data
        throw new Error('Override work(data)');
    }

    onEvent(ev, evtctx) {
        const data = TalentInput.getRawValue(ev);

        const partialIndex = data.idx;
        const workPackage = data.value;

        this.logger.debug(`Worker calculates value for index ${partialIndex} with data ${JSON.stringify(workPackage)}`, evtctx);

        return this.work(workPackage)
            .catch(err => {
                this.logger.warn(err.message, evtctx, err);
                return Worker.ERROR;
            })
            .then(partialResult => {
                this.logger.debug(`Worker finished for index ${partialIndex} with value ${partialResult}`, evtctx);

                return [
                    TalentOutput.createFor(ev.subject, DEFAULT_TYPE, DEFAULT_INSTANCE, this.__getMapPartialFeature(), {
                        value: partialResult,
                        $part: partialIndex
                    })
                ];
        });
    }

    __getMapPartialFeature(type) {
        return this.getFullFeature(this.mapperId, Mapper.FEATURE_MAP_PARTIAL, type);
    }

    __getMapAssignFeature(type) {
        return this.getFullFeature(this.mapperId, Mapper.FEATURE_MAP_ASSIGN, type);
    }
}

Worker.ERROR = 'ERROR';

class Reducer extends Talent {
    constructor(id, mapperId, protocolGatewayConfig) {
        super(id, protocolGatewayConfig);

        this.mapperId = mapperId;

        this.skipCycleCheckFor(this.__getReduceEndFeature(DEFAULT_TYPE));

        this.addOutput(Reducer.FEATURE_MAP_END, {
            description: 'Reduction has ended at that given time in ms since 01.01.1970',
            encoding: {
                type: ENCODING_TYPE_NUMBER,
                encoder: null
            },
            default: -1
        });
    }

    getRules() {
        return new AndRules([
            new Rule(
                new OpConstraint(this.__getMapStartFeature(), OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            ),
            new Rule(
                new OpConstraint(this.__getMapPartialFeature(), OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW, '[:]')
            ),
            new Rule(
                new OpConstraint(this.__getReduceEndFeature(), OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            )
        ]);
    }

    async reduce() {
        // Parameters: data
        throw new Error('Override reduce(data)');
    }

    onEvent(ev, evtctx) {
        if (ev.feature !== this.__getMapPartialFeature()) {
            return;
        }

        // We have a Map partial Feature
        if (TalentInput.getRawValue(ev, 1, false, this.__getMapStartFeature(), DEFAULT_TYPE, DEFAULT_INSTANCE) <= TalentInput.getRawValue(ev, 1, false, this.__getReduceEndFeature(), DEFAULT_TYPE, DEFAULT_INSTANCE)) {
            // No calculation is currently pending
            return;
        }

        return this.reduce(TalentInput.getRawValue(ev))
            .catch(err => {
                this.logger.warn(err.message, evtctx, err);
            })
            .then(resultFeatures => {
                const talentOutput = new TalentOutput();

                talentOutput.add(this, ev, Reducer.FEATURE_MAP_END, Date.now());

                if (Array.isArray(resultFeatures)) {
                    for (let resultFeature of resultFeatures) {
                        talentOutput.outputs.push(resultFeature);
                    }
                }

                return talentOutput.toJson();
            });
    }

    __getMapPartialFeature(type) {
        return this.getFullFeature(this.mapperId, Mapper.FEATURE_MAP_PARTIAL, type);
    }

    __getReduceEndFeature(type) {
        return this.getFullFeature(this.id, Reducer.FEATURE_MAP_END, type);
    }

    __getMapStartFeature(type) {
        return this.getFullFeature(this.mapperId, Mapper.FEATURE_MAP_START, type);
    }
}

Reducer.FEATURE_MAP_END = 'map_end';

module.exports = {
    Mapper,
    Worker,
    Reducer
};