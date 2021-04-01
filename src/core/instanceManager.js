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

const Logger = require('./util/logger');
const ProtocolGateway = require('./protocolGateway');
const MetadataManager = require('./metadataManager');
const clone = require('./util/clone');
const Instance = require('./instance');

const {
    UPDATE_FEATURE_TOPIC,
    DEFAULT_FEATURE_TTL_MS,
    DEFAULT_TYPE
} = require('./constants');

module.exports = class InstanceManager {
    constructor(protocolGatewayConfig) {
        this.uid = uuid();
        this.logger = new Logger(`InstanceManager.${this.uid}`);
        this.pg = new ProtocolGateway(protocolGatewayConfig, this.logger.name, true);
        this.metadataManager = new MetadataManager(protocolGatewayConfig, new Logger(`${this.logger.name}.MetadataManager`));
        this.subjects = {};
    }

    start() {
        return this.metadataManager.start()
            .then(() => this.pg.subscribeJson(UPDATE_FEATURE_TOPIC, this.__onFeatureUpdate.bind(this)))
            .then(() => {
                this.logger.info(`Instance manager ${this.uid} started successfully`);
            });
    }

    getMetadataManager() {
        return this.metadataManager;
    }

    getInstance(subject, instanceId) {
        this.__checkInstanceId(subject, instanceId);
        return this.subjects[subject][instanceId];
    }

    getInstances(subject, instanceIdMatcher = /.*/) {
        this.__checkSubject(subject);

        return Object.keys(this.subjects[subject])
            .filter(instanceId => {
                instanceIdMatcher.lastIndex = 0;
                return instanceId.match(instanceIdMatcher) !== null
            })
            .reduce((acc, instanceId) => {
                acc.push(this.subjects[subject][instanceId]);
                return acc;
            }, []);
    }

    getFeature(subject, instanceId, feature, type = DEFAULT_TYPE, returnClonedFeature = true, metaFeature = null) {
        let metaPromise = null;

        if (metaFeature !== null) {
            metaPromise = Promise.resolve(metaFeature);
        } else {
            metaPromise = this.metadataManager.resolveMetaFeature(type, feature)
        }

        return metaPromise
            .then(metaFeature => {
                let instance = null;

                try {
                    instance = this.getInstance(subject, instanceId);
                }
                catch(err) {
                    // No instance found, but default feature value given in metadata
                    if (metaFeature.default === undefined) {
                        throw err;
                    }

                    return Instance.createFeature(metaFeature.default, metaFeature.default, -1, -1);
                }

                const $feature = instance.getFeatureAt(metaFeature.idx, metaFeature.default);

                if (returnClonedFeature) {
                    return clone($feature);
                }

                return $feature;
            });
    }

    updateFeature(subject, instanceId, feature, encodedValue, rawValue, whenMs, type = DEFAULT_TYPE, returnClonedFeature = true, shouldPublishUpdate = true) {
        let instance = null;

        try {
            instance = this.getInstance(subject, instanceId);
        }
        catch(err) {
            instance = new Instance(type, instanceId, this.logger);

            if (!Object.prototype.hasOwnProperty.call(this.subjects, subject)) {
                this.subjects[subject] = {};
            }

            if (!Object.prototype.hasOwnProperty.call(this.subjects[subject], instanceId)) {
                this.subjects[subject][instanceId] = instance;
            }
        }

        return this.metadataManager.resolveMetaFeature(type, feature)
            .then(meta => {
                if (meta.ttl === 0) {
                    // Features, with a TTL of 0 are immediately returned as volatile feature
                    return {
                        $hidx: -1,
                        $feature: Instance.createFeature(rawValue, encodedValue, whenMs, whenMs)
                    };
                }

                let updateResult = instance.updateFeatureAt(
                    meta.idx,
                    encodedValue,
                    rawValue,
                    whenMs,
                    meta.history,
                    meta.ttl || DEFAULT_FEATURE_TTL_MS
                );

                if (updateResult === null) {
                    return null;
                }

                if (returnClonedFeature) {
                    // Clone $feature to prevent changes made to the reference by any other async tasks
                    // Since broker.publishJson is asynchronous, changes may happen in between instance.getFeatureAt
                    // and return the $feature as promise resolution
                    updateResult.$feature = clone(updateResult.$feature);
                }

                if (!shouldPublishUpdate) {
                    return updateResult;
                }

                return this.pg.publishJson(UPDATE_FEATURE_TOPIC, {
                    sender: this.uid,
                    subject,
                    instanceId,
                    feature,
                    type,
                    whenMs,
                    enc: encodedValue,
                    raw: rawValue
                }).then(() => updateResult);
            });
    }

    getSubjects() {
        return Object.keys(this.subjects);
    }

    __checkSubject(subject) {
        if (!Object.prototype.hasOwnProperty.call(this.subjects, subject)) {
            throw new Error(`Cannot find subject ${subject}`);
        }
    }

    __checkInstanceId(subject, instanceId) {
        this.__checkSubject(subject);

        if (!Object.prototype.hasOwnProperty.call(this.subjects[subject], instanceId)) {
            throw new Error(`Cannot find instanceId ${instanceId}`);
        }
    }

    async __onFeatureUpdate(feature) {
        if (feature.sender === this.uid) {
            return;
        }

        // Do not publish any updates since it's a received update already
        try {
            await this.updateFeature(feature.subject, feature.instanceId, feature.feature, feature.enc, feature.raw, feature.whenMs, feature.type, false, false);
        }
        catch(err) {
            this.logger.warn(`Could not update feature ${feature.feature} for instance ${feature.instanceId} of type ${feature.type}`);
        }
    }
};