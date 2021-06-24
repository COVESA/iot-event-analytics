/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const uuid = require('uuid').v4;

const Ajv = require('ajv');
const KuksaValWebsocket = require('./kuksa.val.websocket');
const MetadataManager = require('../../core/metadataManager');
const Talent = require('../../core/talent');
const JsonModel = require('../../core/util/jsonModel');
const { AndRules, Rule, OpConstraint, ANY_FEATURE, ALL_TYPES } = require('../../core/rules');
const ProtocolGateway = require('../../core/protocolGateway');
const Logger = require('../../core/util/logger');
const { TalentInput } = require('../../core/util/talentIO');

/**
 * Default is backwards compatible:
 * config = {
 *   separator: '.',
 *   replacer: {
 *     '.': '$'
 *   }
 * }
 *
 * New would be:
 * config = {
 *   separator: '/',
 *   replacer: {
 *     '/': '$',
 *     '.': '_'
 *   }
 * }
 */

const VSS_PATH_CONFIG = {
    separator: '.',
    replacer: {
        '.': '$'
    }
};

const {
    INGESTION_TOPIC,
    VALUE_TYPE_RAW,
    PLATFORM_EVENTS_TOPIC,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG
} = require('../../core/constants');

class KuksaValAdapter extends Talent {
    // Handles multiple VssWebsocket Connections
    constructor(protocolGatewayConfig, wsAddress, wsToken, vssSegment, adapterId = uuid(), wsOptions = {}, enableLoopPrevention = false) {
        super(`kuksa-val-adapter-${adapterId}`, protocolGatewayConfig);

        this.pg = new ProtocolGateway(protocolGatewayConfig, this.id);
        this.metadataManager = new MetadataManager(protocolGatewayConfig);

        // Identifies the adapter instance
        this.adapterId = adapterId;
        this.subscriptions = {};

        this.wsAddress = wsAddress;
        this.wsToken = wsToken;
        this.wsOptions = wsOptions;

        this.vssSegment = vssSegment;
        this.vssTypes = [];
        this.kuksaValSocket = null;

        // Represents the actual IoT instance, which is backed by the VSS or the running Kuksa.VAL e.g. the VIN
        this.instanceId = null;
        // Represents the actor, user, to whom the IoT instance belongs
        this.userId = null;

        // Prevents events, from being sent to IoT Event Analytics, if they
        // originate by a publish and reappear in a subscription
        this.enableLoopPrevention = enableLoopPrevention;

        this.evtctx = Logger.createEventContext({});

        this.setVssPathTranslator(new VssPathTranslator());
    }

    setVssPathTranslator(vssPathTranslator) {
        this.vssPathTranslator = vssPathTranslator;
    }

    start(vssInstanceIdPath, vssUserIdPath) {
        // vssInstancePath
        // Path to node, which holds a unique ID for this VSS instance
        try {
            this.kuksaValSocket = new KuksaValWebsocket(this.wsAddress, this.wsToken, this.wsOptions);

            // Retrieve instanceId
            return this.kuksaValSocket.subscribe(vssInstanceIdPath, msg => {
                this.instanceId = msg.value;
            }, err => {
                this.logger.error(err.message, null, err);
            }, true)
                .then(() => {
                    // Retrieve userId
                    return this.kuksaValSocket.subscribe(vssUserIdPath, msg => {
                        this.userId = msg.value;
                    }, err => {
                        this.logger.error(err.message, null, err);
                    }, true)
                })
                .then(() => this.metadataManager.start())
                // Subscribe for platform events
                .then(() => this.pg.subscribeJson(PLATFORM_EVENTS_TOPIC, this.__onPlatformEvent.bind(this)))
                .then(() => super.start());
        }
        catch(err) {
            return Promise.reject(err);
        }
    }

    getRules() {
        return new AndRules([
            new Rule(
                new OpConstraint(ANY_FEATURE, OpConstraint.OPS.ISSET, null, `${this.vssSegment}.${ALL_TYPES}`, VALUE_TYPE_RAW)
            ),
            new Rule(
                new OpConstraint(ANY_FEATURE, OpConstraint.OPS.REGEX, `^${this.adapterId}.*`, `${this.vssSegment}.${ALL_TYPES}`, VALUE_TYPE_RAW, '/sub')
            ),
            new Rule(
                new OpConstraint(ANY_FEATURE, OpConstraint.OPS.NEQUALS, this.id, `${this.vssSegment}.${ALL_TYPES}`, VALUE_TYPE_RAW, '/source')
            )
        ]);
    }

    // Receives event from IoT Event Analytics Platform
    async onEvent(ev, evtctx) {
        // Make a VSS path out of IoT Event Analytics paths
        const absKuksaVssPath = this.vssPathTranslator.ioteaTypeAndFeature2KuksaVssPath(ev.type, ev.feature);

        const rawValue = TalentInput.getRawValue(ev);

        try {
            // Save the timstamp of the most recent change
            if (Object.prototype.hasOwnProperty.call(this.subscriptions, absKuksaVssPath)) {
                this.subscriptions[absKuksaVssPath].updateAtMs = Math.max(ev.timestamp, this.subscriptions[absKuksaVssPath].updateAtMs);
            }

            this.logger.debug(`Publishing value ${rawValue} to Kuksa.val at path ${absKuksaVssPath}...`, evtctx);

            // Publish raw value to VSS
            await this.kuksaValSocket.publish(absKuksaVssPath, rawValue);
        }
        catch (err) {
            this.logger.error(`Could not publish value ${rawValue} to Kuksa.val at path ${absKuksaVssPath}`, evtctx, err);
        }
    }

    async __unsubscribe(talentId, vssPaths) {
        let absVssPaths = await this.__resolveUniqueAbsolutePaths(vssPaths);

        for(const absVssPath of absVssPaths) {
            const subscription = this.subscriptions[absVssPath];

            // Check for which path there is already a subscription
            if (subscription) {
                subscription.consumers.delete(talentId);

                if (subscription.consumers.length === 0) {
                    subscription.unsubscribe();
                    delete this.subscriptions[absVssPath];
                }
            }
        }
    }

    async __subscribe(talentId, vssPaths) {
        let absVssPaths = await this.__resolveUniqueAbsolutePaths(vssPaths);

        try {
            for(const absVssPath of absVssPaths) {
                // Check for which path there is already a subscription
                if (this.subscriptions[absVssPath]) {
                    this.subscriptions[absVssPath].consumers.add(talentId);
                    continue;
                }

                await this.kuksaValSocket.subscribe(absVssPath, async msg => {
                    this.logger.debug(`Received message from Kuksa.val ${JSON.stringify(msg)}`);

                    // Add type and feature fields to message
                    Object.assign(msg, this.vssPathTranslator.kuksaVss2IoteaTypeAndFeature(msg.path));

                    // Assign a correlation id to the message
                    msg.cid = uuid();

                    if (this.userId === '---' || this.instanceId === '---') {
                        // In KUKSA.val, the value "---" means not set
                        this.logger.error(`Forwarding to pipeline failed, because userId ${this.userId} and/or instanceId ${this.instanceId} are not set`);
                        return;
                    }

                    // If timestamp is passed as seconds, fix it by adding current milliseconds
                    // Also support parsing Time Zulu timestamps given by Kuksa.Val (2021-06-23T14:30:33.1624458633Z932)
                    try {
                        msg.timestampMs = this.__fixTimestampMs(msg.timestamp);
                    }
                    catch(err) {
                        this.logger.warn(`Cannot parse given timestamp ${msg.timestamp}`, null, err);
                        return;
                    }


                    // Check if current vss message is newer than the most recent path change, otherwise, skip to prevent Pub/Sub Loops
                    if (this.enableLoopPrevention && this.subscriptions[msg.path] && msg.timestamp <= this.subscriptions[msg.path].updateAtMs) {
                        return;
                    }

                    // path is already a field in message object, since the vssSocket adds it
                    msg.subscriptionId = this.__prefixSubscriptionId(msg.subscriptionId);
                    msg.source = this.id;
                    // instance and userId are taken from VSS data fields
                    msg.instance = this.instanceId;
                    msg.userId =  this.userId;

                    this.logger.debug(`Forwarding event to IoT Event Analytics Platform: ${JSON.stringify(msg)}...`);

                    try {
                        await this.pg.publishJson(INGESTION_TOPIC, msg);
                    }
                    catch(err) {
                        this.logger.warn(`Failed to forward event to IoT Event Analytics Platform: ${JSON.stringify(msg)}`, null, err);
                    }
                }, err => {
                    this.logger.error(`Error for subscription on VSS path ${absVssPath}`, null, err);
                }, true)
                    .then(sub => {
                        this.subscriptions[sub.path] = {
                            subscription: sub,
                            consumers: new Set([ talentId ]),
                            updateAtMs: 0
                        };
                    });
            }
        }
        catch (err) {
            this.logger.error(`Subscription for VSS datapoints for talent ${talentId} failed`, null, err);
        }
    }

    __prefixSubscriptionId(subscriptionId) {
        return `${this.adapterId}.${subscriptionId}`;
    }

    async __onPlatformEvent(ev) {
        const talentId = ev.data.talent;

        if (talentId === this.id) {
            // Ignore any incoming platform event of the adapter itself
            return;
        }

        if (ev.type !== PLATFORM_EVENT_TYPE_SET_CONFIG && ev.type !== PLATFORM_EVENT_TYPE_UNSET_CONFIG) {
            return;
        }

        const talentConfig = ev.data.config;

        try {
            if (this.vssTypes.length === 0) {
                // Since the platform needs to be started already, we assume, that the metadata manager already received updates
                // This leads to an error, catch it anyway
                this.vssTypes = await this.metadataManager.resolveTypes(this.vssSegment);
            }

            const uniqueVssPaths = this.__extractUniqueVssPathsFromRules(talentConfig.rules);

            if (uniqueVssPaths.length === 0) {
                this.logger.info('No VSS Paths found for rules', talentConfig.rules);
                return;
            }

            if (ev.type === PLATFORM_EVENT_TYPE_SET_CONFIG) {
                this.logger.info(`Subscribing to VSS paths ${uniqueVssPaths}...`);
                await this.__subscribe(talentId, uniqueVssPaths);
            }

            if (ev.type === PLATFORM_EVENT_TYPE_UNSET_CONFIG) {
                this.logger.info(`Unsubscribing from VSS paths ${uniqueVssPaths}...`);
                await this.__unsubscribe(talentId, uniqueVssPaths);
            }
        }
        catch(err) {
            this.logger.warn(err.message, null, err);
        }
    }

    async __resolveUniqueAbsolutePaths(vssPaths) {
        let absPaths = new Set();

        for (const vssPath of vssPaths) {
            // Resolve all paths unique to enforce single path subscriptions
            for(const absPath of await this.kuksaValSocket.resolvePaths(vssPath)) {
                absPaths.add(absPath);
            }
        }

        return absPaths;
    }

    // Return all paths, related to vss
    __extractUniqueVssPathsFromRules(rulesJson, vssPaths = []) {
        if (rulesJson.rules) {
            for (let rule of rulesJson.rules) {
                vssPaths = this.__extractUniqueVssPathsFromRules(rule, vssPaths);
            }

            return Array.from(new Set(vssPaths));
        }

        // eslint-disable-next-line no-useless-escape
        const typeSelectionRegex = /^(?:([0-9]+|\*)\.)?([^\.]+)$/g;

        const matches = typeSelectionRegex.exec(rulesJson.typeSelector);

        let resolvedIoteaVssTypes = [];
        const type = matches[2];

        if (type === ALL_TYPES) {
            // Treat *.* and 150000.* and *
            // this.vssTypes contains all registered types, belonging to the given vssSegment
            resolvedIoteaVssTypes = [ ...this.vssTypes ];
        } else {
            // Treat *.Vehicle and 150000.Vehicle and Vehicle
            // Check whether the given type extracted from the rules typeSelector is contained within the given registered types
            if (this.vssTypes.indexOf(type) > -1) {
                resolvedIoteaVssTypes.push(type);
            }
        }

        // Append the selected feature to the given type. Also applies for ALL_FEATURES
        resolvedIoteaVssTypes.forEach(resolvedIoteaVssType => vssPaths.push(this.vssPathTranslator.ioteaTypeAndFeature2KuksaVssPath(resolvedIoteaVssType, rulesJson.feature)));

        return vssPaths;
    }

    __fixTimestampMs(timestamp) {
        // Check for Zulu Timestamp in the given format
        // Grab everything including the Z
        // 2021-06-23T14:30:33.1624458633Z932
        const timeZuluResult = /([0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[^Z]+Z)/.exec(timestamp);

        if (timeZuluResult !== null) {
            return Date.parse(timeZuluResult[1]);
        }

        if (isNaN(parseInt(timestamp, 10))) {
            throw new Error(`The input is neither a UTC- nor a Unix-timestamp`);
        }

        if (timestamp > 9999999999) {
            // Milliseconds are given
            return timestamp;
        }

        // Seconds are given
        return timestamp + (Date.now() % 1000);
    }
}

class VssPathTranslator {
    constructor(vssPathConfig = VSS_PATH_CONFIG) {
        if (!(new Ajv()).compile({
            "type": "object",
            "required": [ "separator" , "replacer" ],
            "properties": {
                "separator": {
                    "type": "string",
                    "minLength": 1,
                    "maxLength": 1
                },
                "replacer": {
                    "type": "object",
                    "patternProperties": {
                        "^.{1}$": {
                            "type": "string",
                            "minLength": 1,
                            "maxLength": 1
                        }
                    },
                    "additionalProperties": false
                }
            },
            "additionalProperties": false
        })(vssPathConfig)) {
            throw new Error('VSS path configuration could not be validated');
        }

        this.vssPathConfig = new JsonModel(vssPathConfig);
    }

    kuksaVss2Iotea(vssPath) {
        // Run through all replacements
        const replacer = this.vssPathConfig.get('replacer');

        for (let search in replacer) {
            vssPath = this.__replaceAllChars(vssPath, search, replacer[search]);
        }

        return vssPath;
    }

    kuksaVss2IoteaTypeAndFeature(kuksaVssPath) {
        const vssPathSeparator = this.vssPathConfig.get('separator');
        const vssPathParts = kuksaVssPath.split(vssPathSeparator);

        return {
            type: this.kuksaVss2Iotea(vssPathParts[0]),
            feature: this.kuksaVss2Iotea(vssPathParts.slice(1).join(vssPathSeparator))
        };
    }

    ioteaTypeAndFeature2KuksaVssPath(type, feature) {
        const pathSeparator = this.vssPathConfig.get('separator');
        const replacer = this.vssPathConfig.get('replacer');
        const reversePathSeparator = replacer[pathSeparator] === undefined ? pathSeparator : replacer[pathSeparator];

        let kuksaVssPath = `${type}${reversePathSeparator}${feature}`;

        for (let replacement in replacer) {
            kuksaVssPath = this.__replaceAllChars(kuksaVssPath, replacer[replacement], replacement);
        }

        return kuksaVssPath;
    }

    __replaceAllChars(input, searchChar, replaceChar) {
        const reservedRegexpChars = ['[', ']', '(', ')', '\\', '^', '$', '.', '|', '?', '*', '+', '{', '}' ];

        if (reservedRegexpChars.indexOf(searchChar) === -1) {
            return input.replace(new RegExp(`${searchChar}`, 'g'), replaceChar);
        }

        return input.replace(new RegExp(`\\${searchChar}`, 'g'), replaceChar);
    }
}

module.exports = {
    KuksaValAdapter,
    VssPathTranslator
};