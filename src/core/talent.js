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
const {
    TalentOutput
} = require('./util/talentIO');
const { NamedMqttBroker } = require('./util/mqttBroker');

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
        this.options = {};
        this.outputFeatures = [];
    }

    skipCycleCheck(value = true) {
        if (value !== true) {
            // Disable cycle check, do nothing
            // Already existing typeFeatures won't be overwritten
            return;
        }

        // Also overwrite given typeFeatures
        this.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] = true;
    }

    skipCycleCheckFor() {
        if (this.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] === true) {
            // Skip, since cycle check is disabled anyway
            return;
        }

        if (!Array.isArray(this.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK])) {
            this.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] = [...arguments];
            return;
        }

        // Ensure unique typeFeatures in Array
        this.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] = Array.from(new Set([...this.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK], ...arguments]));
    }

    addOutput(feature, metadata) {
        this.outputFeatures.push(new OutputFeature(feature, metadata));
    }

    getOutputFeatures(talentId) {
        return this.outputFeatures.reduce((outputs, $feature) => $feature.appendTo(talentId, outputs), {})
    }
}

class Talent extends IOFeatures {
    constructor(id, connectionString, disableMqtt5Support = false) {
        super();
        this.id = id;
        this.uid = Talent.createUid(this.id);
        this.logger = new Logger(`Talent.${this.uid}`);
        this.connectionString = connectionString;
        this.disableMqtt5Support = disableMqtt5Support;
        this.broker = this.__createMessageBroker(`Talent.${this.uid}`, this.connectionString);
        this.ioFeatures = new IOFeatures();
        this.deferredCalls = {};

        // Common channel Id
        this.chnl = uuid.v4();

        this.logger.info(`*****INFO***** Is remote talent? ${this.isRemote()}`);

        for (const callee of this.callees()) {
            // If functions should be called, skip cycle check for their return value
            // Happens, if functions call themselves recursivly
            this.skipCycleCheckFor(`${DEFAULT_TYPE}.${callee}-out`);
        }
    }

    start() {
        let eventSubscriptionTopic = Talent.getTalentTopic(this.id);

        if (!this.disableMqtt5Support) {
            eventSubscriptionTopic = `$share/${this.id}/${eventSubscriptionTopic}`;
        }

        // remote/ prefix do not have to be prepended for Remote talents subscriptions. Talents always receive their events, as if it runs locally
        // For more information see local mosquitto bridging configuration
        // Shared subscription for default events
        return this.__createMessageBroker(`Talent.${this.uid}`, this.connectionString).subscribeJson(eventSubscriptionTopic, this.__onEvent.bind(this))
            // Common subscription for specific events, so that all instances of a talent get the event and can decide whether to use it or not
            .then(() => this.__createMessageBroker(`Talent.${this.uid}`, this.connectionString).subscribeJson(`${Talent.getTalentTopic(this.id)}/${this.chnl}/+`, this.__onCommonEvent.bind(this)))
            // Sufficient if one gets it
            .then(() => this.__createMessageBroker(`Talent.${this.uid}`, this.connectionString).subscribeJson(`$share/${this.id}/${TALENTS_DISCOVERY_TOPIC}`, this.__onDiscover.bind(this)))
            .then(() => {
                this.logger.info(`Talent ${this.uid} started successfully`);
            });
    }

    callees() {
        // <id>.<func>
        return [];
    }

    async call(id, func, args, subject, returnTopic, timeoutMs = 10000) {
        if (this.callees().indexOf(`${id}.${func}`) === -1) {
            throw new Error(`${id}.${func} has to be added to the return value of callees() function`);
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
                call: callId
            }
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

            return this.broker.publishJson([ returnTopic ], ev)
                .then(() => {
                    this.logger.verbose(`Successfully sent function call`);
                });
        });

        return Promise.race([responsePromise, timeoutPromise]);
    }

    getRules() {
        throw new Error('Override getRules() and return an instance of Rules');
    }

    isRemote() {
        return false;
    }

    getFullFeature(talentId, feature, type) {
        return `${type ? `${type}.` : ''}${talentId}.${feature}`;
    }

    async onEvent() {
        // Parameters: ev, evtctx
        throw new Error('Override onEvent(ev, evtctx)');
    }

    async __onDiscover(ev) {
        await this.broker.publishJson(ev.returnTopic, this.__createDiscoveryResponse());
    }

    __getRules() {
        const rules = this.getRules();

        if (this.callees().length === 0) {
            return rules;
        }

        return new OrRules([
            // Ensure, that only the talent with the matching channel will receive the response
            // eslint-disable-next-line no-useless-escape
            ...this.callees().map(callee => new Rule(new OpConstraint(`${callee}-out`, OpConstraint.OPS.REGEX, `^\/${this.chnl}\/.*`, DEFAULT_TYPE, VALUE_TYPE_RAW, '/$tsuffix'))),
            rules
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
            remote: this.isRemote(),
            options: this.options,
            outputs: this.getOutputFeatures(this.id),
            rules: rules.save()
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
        const evtctx = Logger.createEventContext(ev);

        if (ev.msgType === MSG_TYPE_ERROR) {
            this.logger.warn(`Talent error ${ev.code}`, evtctx);
            return;
        }

        try {
            const outEvents = await this.onEvent(ev, evtctx);

            if (outEvents) {
                outEvents.forEach(outEvent => {
                    if (outEvent.whenMs === undefined) {
                        outEvent.whenMs = Date.now();
                    }
                });

                await this.broker.publishJson(ev.returnTopic, outEvents);
            }
        }
        catch(err) {
            this.logger.error(err.message, evtctx, err);
        }
    }

    __createMessageBroker(name, connectionString) {
        let checkMqtt5Compatibility = true;

        if (this.disableMqtt5Support) {
            if (!this.isRemote()) {
                throw new Error(`Disabling MQTT5 support is only supported for remote talents`);
            }

            checkMqtt5Compatibility = false;
        }

        return new NamedMqttBroker(name, connectionString, process.env.MQTT_TOPIC_NS, checkMqtt5Compatibility);
    }
}

Talent.createUid = function createUid(prefix = null) {
    const uniquePart = uuid.v4().substring(0, 8);

    if (prefix === null) {
        return uniquePart;
    }

    return [prefix, uniquePart].join('-');
};

Talent.getTalentTopic = (talentId, isRemote = false, suffix = '') => {
    let topic = `talent/${talentId}/events${suffix}`;

    if (isRemote === true) {
        // Prefix with remote to bridge them to the remote host
        return `remote/${topic}`
    }

    return topic;
};

Talent.isValidTalentFeature = function isValidTalentFeature(talentFeature, talentId) {
    return talentFeature.indexOf(Talent.getTalentFeature(talentId, '')) === 0;
};

Talent.getTalentFeature = function getTalentFeature(talentId, feature) {
    return `${talentId}.${feature}`;
};

module.exports = Talent;