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
const equals = require('./util/equals');
const UomManager = require('./uomManager');

const ErrorMessageFormatter = require('./util/errorMessageFormatter');

const ProtocolGateway = require('./protocolGateway');

const {
    DEFAULT_SEGMENT,
    DEFAULT_TYPE,
    UPDATE_TYPES_TOPIC
} = require('./constants');

class MetadataManager {
    constructor(protocolGatewayConfig, logger = new Logger('MetadataManager')) {
        this.logger = logger;

        /*
        "000000": {
            "features": {}
        },
        "default": {
            "inherits": "000000",
            "features": {}
        }
        */
        this.types = {};

        this.pg = new ProtocolGateway(protocolGatewayConfig, this.logger.name, true);

        this.version = 0;

        this.typesValidator = this.__createTypesValidator();
    }

    start() {
        this.ready = this.pg.subscribeJson(UPDATE_TYPES_TOPIC, data => {
            this.logger.debug(`Types Update v${data.version} received`);
            this.logger.debug(JSON.stringify(data.types));
            this.version = data.version;
            this.types = data.types;
        })
            .then(() => {
                this.logger.info(`Metadata manager worker started successfully`);
            });

        return this.ready;
    }

    startAsMaster(typesConfigPath, uomConfigPath) {
        this.uomManager = new UomManager();

        // Register default segment
        this.__registerType(DEFAULT_SEGMENT, DEFAULT_TYPE);

        this.ready = this.uomManager.start(uomConfigPath)
            // Master reads the initial configuration from a file
            .then(() => this.__loadConfiguration(typesConfigPath))
            .then(config => this.__initFromConfig(config))
            .then(() => this.__publishTypes())
            .then(() => {
                this.logger.info(`Metadata manager principal started successfully`);
            });

        return this.ready;
    }

    resolveFeatureAt(type, idx) {
        // Find out what feature is stored at the given index
        return this.ready.then(() => {
            if (!this.__hasType(type)) {
                throw new Error(`Type ${type} cannot be resolved`);
            }

            const specificType = this.types[type];

            try {
                return this.__findFeatureForTypeAt(type, idx);
            }
            catch(err) {
                if (specificType.inherits === null) {
                    throw err;
                }

                return this.__findFeatureForTypeAt(specificType.inherits, idx);
            }
        });
    }

    resolveMetaFeatureAt(type, idx) {
        return this.resolveFeatureAt(type, idx)
            .then(feature => this.resolveMetaFeature(type, feature));
    }

    resolveMetaFeature(type, feature) {
        return this.ready.then(() => {
            if (!this.__hasType(type)) {
                throw new Error(`Type ${type} cannot be resolved`);
            }

            const specificType = this.types[type];

            let specificFeature = null;

            if (Object.prototype.hasOwnProperty.call(specificType.features, feature)) {
                // Feature exists in specific type
                specificFeature = specificType.features[feature];
            }

            if (specificType.inherits !== null) {
                if (!this.__hasType(specificType.inherits)) {
                    throw new Error(`Invalid reference to base type ${specificType.inherits}.`);
                }

                if (Object.prototype.hasOwnProperty.call(this.types[specificType.inherits].features, feature)) {
                    return Object.assign(
                        {},
                        // Inherited feature
                        this.types[specificType.inherits].features[feature],
                        // Specific feature
                        specificFeature === null ? {} : specificFeature
                    );
                }
            }

            if (specificFeature === null) {
                throw new Error(`The feature ${feature} of type ${type} cannot be resolved`);
            }

            return Object.assign(
                {},
                specificFeature
            );
        });
    }

    resolveTypes(segment) {
        return this.ready.then(() => {
            if (!this.__hasType(segment)) {
                throw new Error(`Segment ${segment} cannot be resolved`);
            }

            if (this.types[segment].inherits !== null) {
                throw new Error(`${segment} is a type, not a segment`);
            }

            return this.types[segment].types;
        });
    }

    resolveSegment(type) {
        return this.ready.then(() => {
            if (!this.__hasType(type)) {
                throw new Error(`Type ${type} cannot be resolved`);
            }

            if (this.types[type].inherits === null) {
                throw new Error(`Segments can only be resolved from given types`);
            }

            return this.types[type].inherits;
        });
    }

    resolveMetaFeatures(type) {
        return this.resolveFeatures(type)
            .then(async features => {
                const $features = {};

                for (let feature of features) {
                    $features[feature] = await this.resolveMetaFeature(type, feature);
                }

                return $features;
            });
    }

    // Returns all feature names for a given type
    resolveFeatures(type) {
        return this.ready.then(async () => {
            if (!this.__hasType(type)) {
                throw new Error(`Given type ${type} could not be found`);
            }

            let $type = this.types[type];
            let features = [];

            for (const feature of Object.keys($type.features)) {
                features.push(feature);
            }

            if ($type.inherits !== null) {
                features = [...features, ...await this.resolveFeatures($type.inherits)];
            }

            return Array.from(new Set(features));
        });
    }

    getTypes() {
        return this.ready.then(() => {
            return Object.keys(this.types).filter(type => this.types[type].inherits !== null);
        });
    }

    getTypeMap() {
        return this.getTypes()
            .then(async types => {
                const typeMap = {};

                for (const type of types) {
                    const features = await this.resolveFeatures(type);

                    typeMap[type] = {
                        segment: this.types[type].inherits,
                        features: {}
                    };

                    for (const feature of features) {
                        typeMap[type].features[feature] = await this.resolveMetaFeature(type, feature);
                    }
                }

                return typeMap;
            });
    }

    getVersion() {
        return this.version;
    }

    async registerTalentOutputFeatures(outputFeatureMap) {
        try {
            // Validate all features
            for (let outputFeature of Object.keys(outputFeatureMap)) {
                this.__validateFeature(DEFAULT_SEGMENT, outputFeature, outputFeatureMap[outputFeature], DEFAULT_TYPE);
            }

            let typesChanged = false;

            // Register features only if all of them are valid
            for (let outputFeature of Object.keys(outputFeatureMap)) {
                typesChanged = this.__registerFeature(DEFAULT_SEGMENT, outputFeature, outputFeatureMap[outputFeature], DEFAULT_TYPE) || typesChanged;
            }

            // Check if something has changed
            if (!typesChanged) {
                return;
            }

            this.version++;

            await this.__publishTypes();
        }
        catch(err) {
            this.logger.warn(err.message);
        }
    }

    __findFeatureForTypeAt(type, idx) {
        for (let feature in this.types[type].features) {
            if (this.types[type].features[feature].idx === idx) {
                return feature;
            }
        }

        throw new Error(`Feature at index ${idx} of type ${type} does not exist`);
    }

    __publishTypes() {
        const publishOptions = ProtocolGateway.createPublishOptions(true);
        publishOptions.retain = true;

        return this.pg.publishJson(UPDATE_TYPES_TOPIC, {
            version: this.version,
            types: this.types
        }, publishOptions);
    }

    __loadConfiguration(typesConfigPath) {
        return new Promise((resolve, reject) => {
            fs.readFile(typesConfigPath, {
                encoding: 'utf8'
            }, (err, content) => {
                if (err) {
                    reject(err);
                    return;
                }

                resolve(JSON.parse(content));
            });
        });
    }

    __initFromConfig(typesConfig) {
        return this.typesValidator(typesConfig)
            .catch(err => {
                throw new Error(`Validation of types configuration failed. Errors: ${ErrorMessageFormatter.formatAjvValidationError(err)}`);
            })
            .then(() => {
                for(let segment of Object.keys(typesConfig)) {
                    if (segment === DEFAULT_SEGMENT) {
                        throw new Error(`Dynamic segment ${segment} cannot be configured via configuration file`);
                    }

                    for (let feature of Object.keys(typesConfig[segment].features)) {
                        this.__registerFeature(segment, feature, typesConfig[segment].features[feature]);
                    }

                    for (let type of Object.keys(typesConfig[segment].types)) {
                        if (Object.prototype.hasOwnProperty.call(this.types, type)) {
                            // specific type names have to be unique
                            throw new Error(`Duplicate type ${type} found in ${segment}`);
                        }

                        const features = Object.keys(typesConfig[segment].types[type].features);

                        if (features.length === 0) {
                            // No specific feature given > register empty type
                            this.__registerType(segment, type);
                        } else {
                            for (let feature of features) {
                                this.__registerFeature(segment, feature, typesConfig[segment].types[type].features[feature], type)
                            }
                        }
                    }
                }
            });
    }

    __createTypesValidator() {
        const schemaBasePath = path.normalize(path.join(__dirname, '../../resources'));

        return new Ajv({
            schemas: [
                JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'feature.schema.json')), { encoding: 'utf8' }),
                Object.assign(
                    JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'types.schema.json')), { encoding: 'utf8' }),
                    { $async: true }
                )
            ]
        }).getSchema('http://example.com/schemas/types.schema.json');
    }

    __hasType(type) {
        return Object.prototype.hasOwnProperty.call(this.types, type);
    }

    __hasFeature(type, feature) {
        return this.__hasType(type) && this.types[type].features[feature] !== undefined;
    }

    __validateFeature(segment, feature, metadata) {
        if (!Number.isInteger(metadata.idx)) {
            // No index is given
            return;
        }

        if (this.types[segment] === undefined) {
            // Segment is not defined
            return;
        }

        if (this.__containsFeatureIndex(segment, metadata.idx)) {
            throw new Error(`Feature index ${metadata.idx} of feature ${feature} is already defined in segment ${segment}`);
        }
    }

    __registerFeature(segment, feature, metadata, type) {
        if (segment === DEFAULT_SEGMENT) {
            type = DEFAULT_TYPE;
        }

        this.__validateFeature(segment, feature, metadata, type);

        let typesChanged = this.__registerSegment(segment);

        if (!this.__hasFeature(segment, feature) && (!type || !this.__hasFeature(type, feature))) {
            // Only assign an index, if
            // - the feature does not exist in the given segment
            //   AND
            //   - the type is not given at all
            //     OR
            //   - the feature does not exist in the given type
            // to ensure, that already defined indices remain at the same index
            if (!Number.isInteger(metadata.idx)) {
                // Only change the index, if it is not specified at all
                metadata.idx = this.__getMaxIndex(segment) + 1;
                typesChanged = true;
            }
        }

        let featureAssignee = this.types[segment];

        if (type) {
            typesChanged = this.__registerType(segment, type) || typesChanged;
            featureAssignee = this.types[type];
        }

        // Add unit of measurement to metadata
        metadata.$unit = null;

        if (metadata.unit) {
            if (typeof metadata.unit === 'string') {
                metadata.$unit = this.uomManager.resolve(metadata.unit);
            } else {
                /**
                 * Allow to add custom units to be declared within e.g. a talent or the types definition
                 *
                 * {
                 *   "fac": <Factor to convert to the base unit>,
                 *   "unit": "<The Unit>",
                 *   "desc": "<Some description>"
                 *   "ref": "Reference to base unit if existing"
                 * }
                 */
                metadata.$unit = this.uomManager.processBaseReference(metadata.unit);
                metadata.unit = metadata.$unit.unit;
            }
        }

        if (featureAssignee.features[feature] === undefined) {
            featureAssignee.features[feature] = metadata;
            typesChanged = true;
        } else if (segment === DEFAULT_SEGMENT && type === DEFAULT_TYPE) {
            // Keep the given index but overwrite the rest
            const $metadata = Object.assign(metadata, { idx: featureAssignee.features[feature].idx });
            typesChanged = !equals(featureAssignee.features[feature], $metadata, { strictArrayOrder: false }) || typesChanged;
            featureAssignee.features[feature] = $metadata;
        }

        return typesChanged;
    }

    __registerType(segment, type) {
        this.__registerSegment(segment);

        if (!Object.prototype.hasOwnProperty.call(this.types, type)) {
            this.types[type] = {
                inherits: segment,
                features: {}
            };

            this.types[segment].types.push(type);

            return true;
        }

        return false;
    }

    __registerSegment(segment) {
        if (!Object.prototype.hasOwnProperty.call(this.types, segment)) {
            this.types[segment] = {
                inherits: null,
                features: {},
                // All types belonging to the segment are stored here
                types: []
            };

            return true;
        }

        return false;
    }

    __createKeyForInstance(subject, instance) {
        return [subject, instance].join('/');
    }

    __containsFeatureIndex(segment, index) {
        for (let next of this.allFeatureInSegment(segment)) {
            if (next.feature.idx === index) {
                return true;
            }
        }

        return false;
    }

    __getMaxIndex(segment) {
        let maxIndex = -1;

        for (let next of this.allFeatureInSegment(segment)) {
            maxIndex = Math.max(next.feature.idx, maxIndex);
        }

        return maxIndex;
    }

    *allFeatureInSegment(segment) {
        if (segment === undefined || !this.__hasType(segment)) {
            return;
        }

        // Segment can be treated as type for common features
        yield *this.allFeaturesOfType(segment);

        const types = this.types[segment].types || [];

        for (let type of types) {
            yield *this.allFeaturesOfType(type);
        }
    }

    *allFeaturesOfType(type) {
        if (type !== undefined && this.__hasType(type)) {
            for (let feature of Object.keys(this.types[type].features)) {
                yield {
                    segment: this.types[type].inherits,
                    type: type,
                    feature: this.types[type].features[feature]
                };
            }
        }
    }
}

module.exports = MetadataManager;
