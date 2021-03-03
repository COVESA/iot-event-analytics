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

const Logger = require('./util/logger');
const { NamedMqttBroker } = require('./util/mqttBroker');
const FieldGraph = require('./util/featureGraph');
const RulesManager = require('./rulesManager');
const MetadataManager = require('./metadataManager');
const Talent = require('./talent');
const PlatformEvents = require('./platformEvents');
const Logo = require('./logo');

const ErrorMessageFormatter = require('./util/errorMessageFormatter');

const {
    ALL_TYPES,
    ALL_SEGMENTS
} = require('./rules');

const {
    MSG_TYPE_ERROR,
    MSG_TYPE_DISCOVERY,
    TALENTS_DISCOVERY_TOPIC,
    TALENTS_DISCOVERY_RETURN_TOPIC,
    TALENT_DISCOVERY_INTERVAL_MS,
    TALENT_DISCOVERY_TIMEOUT_MS,
    TALENT_DISCOVERY_OPTIONS,
    DEFAULT_TYPE,
    ERROR_FEATURE_DEPENDENCY_LOOP,
    ERROR_NON_PREFIXED_FEATURE,
    ERROR_INVALID_DISCOVERY_INFO,
    ERROR_RESOLVING_TYPES,
    JSON_API_VERSION
 } = require('./constants');

 const RulesLoader = require('./rules.loader');

module.exports = class ConfigManager {
    constructor(connectionString, platformId = '') {
        this.logger = new Logger('ConfigManager');

        Logo.print();

        PlatformEvents.init(connectionString);

        this.broker = new NamedMqttBroker('ConfigManager', connectionString);
        this.platformId = platformId;
        this.metadataManager = new MetadataManager(connectionString, new Logger('ConfigManager.MetadataManager'));
        this.rulesManager = new RulesManager(connectionString);
        this.discoveryTimeout = null;
        this.discoveryCache = [];
        this.registeredTalentIds = [];
        this.discoveryValidator = this.__createDiscoveryValidator();
    }

    start(discoveryIntervalMs = TALENT_DISCOVERY_INTERVAL_MS, typesConfigPath, uomConfigPath) {
        return this.metadataManager.startAsMaster(typesConfigPath, uomConfigPath)
            .then(() => this.rulesManager.start())
            // Subscribe to talent discovery response
            .then(() => this.broker.subscribeJson(this.__prefixPlatformId(TALENTS_DISCOVERY_RETURN_TOPIC), this.__onTalentDiscovery.bind(this)))
            .then(() => {
                this.logger.info('ConfigManager started');
            })
            .then(() => this.__startDiscovery(discoveryIntervalMs));
    }

    getMetadataManager() {
        return this.metadataManager;
    }

    __stopDiscovery() {
        if (this.discoveryTimeout !== null) {
            clearTimeout(this.discoveryTimeout);
            this.discoveryTimeout = null;
        }
    }

    __startDiscovery(discoveryIntervalMs) {
        this.__stopDiscovery();

        this.discoveryEndsAt = Date.now() + TALENT_DISCOVERY_TIMEOUT_MS;
        this.discoveryCache = [];

        this.logger.info('Starting talent discovery...');

        // TODO: discovery filter based on some features
        return this.broker.publishJson(TALENTS_DISCOVERY_TOPIC, {
            msgType: MSG_TYPE_DISCOVERY,
            version: JSON_API_VERSION,
            returnTopic: this.__prefixPlatformId(TALENTS_DISCOVERY_RETURN_TOPIC)
        })
            .then(() => {
                // Listen for discovery responses for a given time
                this.discoveryTimeout = setTimeout(async () => {
                    if (this.discoveryCache.length === 0) {
                        // Clear all rules
                        this.logger.debug(`Clearing all rules`);
                        await this.rulesManager.clearRules();
                        // No talent was found
                        // Reduce the interval and search again
                        this.logger.info(`No talent was discovered. Restart discovery now.`);
                        this.__startDiscovery(discoveryIntervalMs);
                        return;
                    }

                    await this.__onTalentDiscoveryEnd(this.discoveryCache);

                    // Reset the timeout to the function to start a new discovery
                    this.discoveryTimeout = setTimeout(() => {
                        this.__startDiscovery(discoveryIntervalMs);
                    }, discoveryIntervalMs);
                }, TALENT_DISCOVERY_TIMEOUT_MS);
            });
    }

    __onTalentDiscovery(discovery) {
        if (Date.now() < this.discoveryEndsAt) {
            this.logger.info('Discovered talent ' + discovery.id);
            this.discoveryCache.push(discovery);
        }
    }

    __createDiscoveryValidator() {
        const schemaBasePath = path.normalize(path.join(__dirname, '../../resources'));

        return new Ajv({
            schemas: [
                JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'feature.schema.json')), { encoding: 'utf8' }),
                JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'rules.schema.json')), { encoding: 'utf8' }),
                JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'discovery.schema.json')), { encoding: 'utf8' })
            ]
        }).getSchema('http://example.com/schemas/discovery.schema.json');
    }

    async __onTalentDiscoveryEnd(discoveryCache) {
        // Filter out entries without an id
        // Filter out any duplicate entries by id --> prefer the early arrivers because of latency
        discoveryCache = discoveryCache.filter((discovery, idx, arr) => {
            return typeof discovery.id === 'string' &&
                   discovery.id.trim() !== '' &&
                   arr.findIndex(d => d.id === discovery.id) === idx
        });

        this.logger.debug(`Discovered ${discoveryCache.length} talents`);

        const fg = new FieldGraph();
        const registeredTalentIds = [];

        // Newly detected talents will be put to the end so they do not break the already working ones
        discoveryCache.sort((a, b) => {
            return this.registeredTalentIds.indexOf(a.id) > -1 && !this.registeredTalentIds.indexOf(b.id) > -1 ? 1 : -1;
        });

        for (const discovery of discoveryCache) {
            if (!this.discoveryValidator(discovery)) {
                this.logger.info(`Discovery info of ${discovery.id} is invalid. Errors: ${ErrorMessageFormatter.formatAjvValidationError(new Ajv.ValidationError(this.discoveryValidator.errors))}`);
                await this.__publishTalentDiscoveryError(discovery, ERROR_INVALID_DISCOVERY_INFO);
                continue;
            }

            // Keep the current state of the graph
            fg.freeze();

            const outputs = discovery.outputs;

            if (!Object.keys(outputs).reduce((acc, outputFeature) => acc && Talent.isValidTalentFeature(outputFeature, discovery.id), true)) {
                // Outputs, which are not prefixed correctly will rejected
                this.logger.warn(`Non-prefixed output feature detected`);

                await this.__publishTalentDiscoveryError(discovery, ERROR_NON_PREFIXED_FEATURE);

                continue;
            }

            if (discovery.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK] !== true) {
                const outputFeatures = Object.keys(outputs).map(feature => (outputs[feature].type || DEFAULT_TYPE) + '.' + feature);

                const rules = RulesLoader.load(discovery.rules);
                const typeFeatures = rules.getUniqueTypeFeatures();

                const inputFeatures = [];

                const addInputFeature = ((type, feature, features) => {
                    const typeFeature = `${type}.${feature}`;

                    if (Array.isArray(discovery.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK]) && discovery.options[TALENT_DISCOVERY_OPTIONS.SKIP_CYCLE_CHECK].indexOf(typeFeature) > -1) {
                        // Skip cycle check for the given field
                        return;
                    }

                    if (features.indexOf(typeFeature) > -1) {
                        return;
                    }

                    features.push(typeFeature);
                });

                try {
                    for (const typeFeature of typeFeatures) {
                        // NO CHECK WETHER TYPE ACTUALLY EXISTS IN THE GIVEN SEGMENT (IF GIVEN)
                        // NO CHECK WETHER FEATURE ACTUALLY EXISTS FOR THE GIVEN TYPE (Non-default types only)
                        if (typeFeature.type !== ALL_TYPES) {
                            // Type is given
                            // Check if feature is * or not
                            addInputFeature(typeFeature.type, typeFeature.feature, inputFeatures);
                            continue;
                        }

                        let types = []

                        if (typeFeature.segment !== ALL_SEGMENTS && typeFeature.segment !== null) {
                            // Segment is given, but type is wildcard
                            types = await this.metadataManager.resolveTypes(typeFeature.segment);
                        } else {
                            // Type and/or segment are wildcards --> get everything
                            types = await this.metadataManager.getTypes();
                            types.push(DEFAULT_TYPE);
                        }

                        types.forEach(type => addInputFeature(type, typeFeature.feature, inputFeatures));
                    }
                }
                catch(err) {
                    this.logger.warn(err.message, null, err);

                    await this.__publishTalentDiscoveryError(discovery, ERROR_RESOLVING_TYPES);

                    continue;
                }

                // Add dependency for each input goes to every output
                for (const inputFeature of inputFeatures) {
                    for (const outputFeature of outputFeatures) {
                        fg.addDependency(inputFeature, outputFeature);
                    }
                }

                // Loop check
                if (fg.containsCycles()) {
                    // Melt here, since dependencies were already added to the graph
                    fg.melt();

                    // Skip the given talent, since dependency cycles were found
                    this.logger.warn(`Feature dependency cycles were detected while registering talent "${discovery.id}"`);

                    await this.__publishTalentDiscoveryError(discovery, ERROR_FEATURE_DEPENDENCY_LOOP);

                    continue;
                }
            }

            try {
                await this.metadataManager.registerTalentOutputFeatures(outputs);

                await this.rulesManager.setRules(discovery.id, discovery.rules, discovery.remote);

                this.logger.info(`Registered talent ${discovery.id} successfully`);
            }
            catch(err) {
                // Melt here, since dependencies were already added to the graph
                fg.melt();

                this.logger.warn(err.message, null, err);

                continue;
            }

            registeredTalentIds.push(discovery.id);
        }

        // Unregister removed talents at rules manager
        this.registeredTalentIds
            .filter(registeredTalentId => registeredTalentIds.indexOf(registeredTalentId) === -1)
            .forEach(async removedTalentId => {
                // Check which talents have been removed
                this.logger.debug(`Removing rules for talent ${removedTalentId}`);
                await this.rulesManager.unsetRules(removedTalentId);
            });

        this.registeredTalentIds = registeredTalentIds;
    }

    __prefixPlatformId(topic) {
        if (this.platformId === '') {
            return topic;
        }

        return this.platformId + '/' + topic;
    }

    async __publishTalentDiscoveryError(discovery, code) {
        try {
            // Send event straight back to plugin
            await this.broker.publishJson(Talent.getTalentTopic(discovery.id, discovery.remote === true), {
                code,
                msgType: MSG_TYPE_ERROR
            });
        }
        catch(err) {
            this.logger.warn(`Could not send error ${code} to talent ${discovery.id}`, null, err);
        }
    }
};