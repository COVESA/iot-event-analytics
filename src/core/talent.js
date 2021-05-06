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
const Logger = require('./util/logger');
const jsonQuery = require('./util/jsonQuery');
const clone = require('./util/clone');
const {
    TalentOutput
} = require('./util/talentIO');
const ProtocolGateway = require('./protocolGateway');

const {
    TALENTS_DISCOVERY_TOPIC,
    TALENT_DISCOVERY_OPTIONS,
    VALUE_TYPE_RAW,
    MSG_TYPE_ERROR,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE
} = require('./constants');

const {
    Rule,
    OrRules,
    OpConstraint
} = require('./rules');

class OutputFeature {
    constructor(feature, metadata) {
        this.feature = feature;
        this.metadata = metadata;
    }

    appendTo(talentId, featureMap) {
        featureMap[Talent.getTalentFeature(talentId, this.feature)] = this.metadata;
        return featureMap;
    }
}

class IOFeatures {
    constructor() {
        this.config = {};
        this.outputFeatures = [];
    }

    skipCycleCheck(value = true) {
        if (value !== true) {
            // Disable cycle check, do nothing
            // Already existing typeFeatures won't be overwritten
            return;
        }

        // Also overwrite given typeFeatures
        this.config[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] = true;
    }

    skipCycleCheckFor() {
        if (this.config[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] === true) {
            // Skip, since cycle check is disabled anyway
            return;
        }

        if (!Array.isArray(this.config[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK])) {
            this.config[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] = [...arguments];
            return;
        }

        // Ensure unique typeFeatures in Array
        this.config[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] = Array.from(new Set([...this.config[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK], ...arguments]));
    }

    addOutput(feature, metadata) {
        this.outputFeatures.push(new OutputFeature(feature, metadata));
    }

    getOutputFeatures(talentId) {
        return this.outputFeatures.reduce((outputs, $feature) => $feature.appendTo(talentId, outputs), {})
    }
}

class Talent extends IOFeatures {
    constructor(id, protocolGatewayConfig, talentConfig = {}) {
        super();
        this.id = id;
        this.uid = Talent.createUid(this.id);
        this.logger = new Logger(`Talent.${this.uid}`);
        // Clone to ensure config consistency
        this.config = clone(Object.assign(talentConfig || {}, this.config));

        if (ProtocolGateway.getAdapterCount(protocolGatewayConfig) !== 1) {
            throw new Error(`Invalid Talent ProtocolGateway Configuration. Specify a single adapter in your ProtocolGateway configuration`);
        }

        // We already ensured one Adapter
        this.pg = new ProtocolGateway(protocolGatewayConfig, this.logger.name);

        this.ioFeatures = new IOFeatures();
        this.deferredCalls = {};

        // Common channel Id
        // Prefixed by talentId to be able to have a function in multiple instances of a Talent, which is calling itself recursively
        // The rule, which evaluates for events of a function result, just checks for the prefixed talent id and not on the full channel id, since this differs
        // for multiple talent instances and thus breaks a recursively calculated response
        this.chnl = `${this.id}.${uuid.v4()}`;

        for (const callee of this.callees()) {
            // If functions should be called, skip cycle check for their return value
            // Happens, if functions call themselves recursivly
            this.skipCycleCheckFor(`${DEFAULT_TYPE}.${callee}-out`);
        }
    }

    start() {
        // remote/ prefix do not have to be prepended for Remote talents subscriptions. Talents always receive their events, as if it runs locally
        // For more information see local mosquitto bridging configuration
        // Shared telemetry event subscription for all Talent instances sharing the same id
        return this.pg.subscribeJsonShared(this.id, Talent.getTalentTopic(this.id), this.__onEvent.bind(this))
            // Subscription for Talent instance specific events
            .then(() => this.pg.subscribeJson(`${Talent.getTalentTopic(this.id)}/${this.chnl}/+`, this.__onCommonEvent.bind(this)))
            // Sufficient if one Talent instance gets it
            .then(() => this.pg.subscribeJsonShared(this.id, TALENTS_DISCOVERY_TOPIC, this.__onDiscover.bind(this)))
            .then(() => {
                this.logger.info(`Talent ${this.uid} started successfully`);
            });
    }

    callees() {
        // <id>.<func>
        return [];
    }

    async call(id, func, args, subject, returnTopic, timeoutMs = 10000, nowMs = Date.now()) {
        if (this.callees().indexOf(`${id}.${func}`) === -1) {
            throw new Error(`${id}.${func} has to be added to the return value of callees() function`);
        }

        if (timeoutMs <= 0) {
            throw new Error(`The function call ${func}() timed out`);
        }

        const callId = uuid.v4();

        const ev = TalentOutput.createFor(
            subject,
            DEFAULT_TYPE,
            DEFAULT_INSTANCE,
            `${id}.${func}-in`, {
                func,
                args,
                chnl: this.chnl,
                call: callId,
                timeoutAtMs: nowMs + timeoutMs
            },
            nowMs
        );

        const timeoutPromise = new Promise((resolve, reject) => {
            setTimeout(() => {
                // Delete it automatically after the timeout
                delete this.deferredCalls[callId];
                reject(new Error('Timeout at calling function'));
            }, timeoutMs);
        });

        const responsePromise = new Promise((resolve, reject) => {
            this.logger.debug(`Storing deferred call ${callId}...`)

            this.deferredCalls[callId] =  {
                resolve,
                reject
            };

            this.logger.verbose(`Sending function call to ${JSON.stringify(ev)} to ${returnTopic}...`);

            return this.pg.publishJson(returnTopic, ev)
                .then(() => {
                    this.logger.verbose(`Successfully sent function call`);
                });
        });

        return Promise.race([responsePromise, timeoutPromise]);
    }

    getRules() {
        throw new Error('Override getRules() and return an instance of Rules');
    }

    getFullFeature(talentId, feature, type) {
        return `${type ? `${type}.` : ''}${talentId}.${feature}`;
    }

    async onEvent() {
        // Parameters: ev, evtctx
        throw new Error('Override onEvent(ev, evtctx)');
    }

    async __onDiscover(ev) {
        await this.pg.publishJson(ev.returnTopic, this.__createDiscoveryResponse());
    }

    __getRules() {
        /**
         * 1) OR -> Triggerable Talent, which does not call any functions
         *      triggerRules
         *
         * 2) OR -> Triggerable Talent, which calls one or more functions
         *      function result rules
         *      OR/AND [exclude function result rules]
         *        triggerRules
         */

        const triggerRules = this.getRules();

        const functionResultRules = this.__getFunctionResultRules();

        if (functionResultRules === null) {
            // returns 1)
            return triggerRules;
        }

        triggerRules.excludeOn = this.callees().map(callee => `${DEFAULT_TYPE}.${callee}-out`);

        functionResultRules.add(triggerRules);

        // returns 2)
        return functionResultRules;
    }

    __getFunctionResultRules() {
        if (this.callees().length === 0) {
            return null;
        }

        return new OrRules([
            // Ensure, that only the talent with the matching channel will receive the response
            // Since the full channel id is unique for a talent instance, this rule would fail, if there are multiple instances of a talent because it would only check for one talent here
            // -> The rule only checks the talent id prefix, which is common for all scaled Talent instances.
            // eslint-disable-next-line no-useless-escape
            ...this.callees().map(callee => new Rule(new OpConstraint(`${callee}-out`, OpConstraint.OPS.REGEX, `^\/${this.id}\.[^\/]+\/.*`, DEFAULT_TYPE, VALUE_TYPE_RAW, '/$tsuffix')))
        ]);
    }

    __createDiscoveryResponse() {
        const rules = this.__getRules();

        this.logger.info(`${this.id} depends on the following feature(s):`);

        for (let rule of rules.forEach()) {
            if (rule.constraint === null) {
                continue;
            }

            this.logger.info('  ' + rule.constraint.toString());
        }

        return {
            id: this.id,
            config: Object.assign(this.config, {
                outputs: this.getOutputFeatures(this.id),
                rules: rules.save()
            })
        };
    }

    async __onCommonEvent(ev, topic) {
        this.logger.verbose(`Received common event ${JSON.stringify(ev)} at topic ${topic}`);

        // eslint-disable-next-line no-useless-escape
        const suffixMatch = topic.match(/^.*\/([^\/]+)$/m);

        const callId = suffixMatch[1];

        const deferredCall = this.deferredCalls[callId];

        if (deferredCall === undefined) {
            // Talent is not waiting for this response
            this.logger.debug(`Deferred call with id ${callId} could not be found`);
            return;
        }

        delete this.deferredCalls[callId];

        this.logger.debug(`Deferred call found with id ${callId}`);

        const value = jsonQuery.first(ev.value, ev.value.$vpath).value;

        if (ev.msgType === MSG_TYPE_ERROR) {
            deferredCall.reject(new Error(value));
        } else {
            deferredCall.resolve(value);
        }
    }

    async __onEvent(ev) {
        await this.__processEvent(ev, this.onEvent.bind(this));
    }

    async __processEvent(ev, cb) {
        const evtctx = Logger.createEventContext(ev);

        if (ev.msgType === MSG_TYPE_ERROR) {
            this.logger.warn(`Talent error ${ev.code}`, evtctx);
            return;
        }

        try {
            const outEvents = await cb(ev, evtctx);

            if (outEvents) {
                outEvents.forEach(outEvent => {
                    if (outEvent.whenMs === undefined) {
                        outEvent.whenMs = Date.now();
                    }
                });

                await this.pg.publishJson(ev.returnTopic, outEvents);
            }
        }
        catch(err) {
            this.logger.error(err.message, evtctx, err);
        }
    }
}

Talent.createUid = function createUid(prefix = null) {
    const uniquePart = uuid.v4().substring(0, 8);

    if (prefix === null) {
        return uniquePart;
    }

    return [prefix, uniquePart].join('-');
};

Talent.getTalentTopic = (talentId, suffix = '') => {
    return `talent/${talentId}/events${suffix}`;
};

Talent.isValidTalentFeature = function isValidTalentFeature(talentFeature, talentId) {
    return talentFeature.indexOf(Talent.getTalentFeature(talentId, '')) === 0;
};

Talent.getTalentFeature = function getTalentFeature(talentId, feature) {
    return `${talentId}.${feature}`;
};

module.exports = Talent;