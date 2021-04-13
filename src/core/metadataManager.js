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

        this.typesValidator = this.__createValidatorFor('http://example.com/schemas/types.schema.json');

        this.fullFeatureValidator = this.__createValidatorFor('http://example.com/schemas/fullFeature.schema.json');

        this.fullIndexedFeatureValidator = this.__createValidatorFor('http://example.com/schemas/fullIndexedFeature.schema.json');
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
        // Validate all features
        for (let outputFeature of Object.keys(outputFeatureMap)) {
            await this.__validateFeature(DEFAULT_SEGMENT, outputFeature, outputFeatureMap[outputFeature], DEFAULT_TYPE, true);
        }

        let typesChanged = false;

        // Register features only if ALL of given features are valid i.e. if one feature is invalid, none of the given features should be registered
        for (let outputFeature of Object.keys(outputFeatureMap)) {
            typesChanged = await this.__registerFeature(DEFAULT_SEGMENT, outputFeature, outputFeatureMap[outputFeature], DEFAULT_TYPE) || typesChanged;
        }

        // Check if something has changed
        if (!typesChanged) {
            return;
        }

        this.version++;

        await this.__publishTypes();
    }

    // resolves the featurename based on the given index
    __findFeatureForTypeAt(type, idx) {
        if (typeof type !== 'string') {
            throw new Error(`__findFeatureForTypeAt(type, idx): "type" argument needs to be a string. Got "${typeof type}"`);
        }

        for (let feature in this.types[type].features) {
            if (this.types[type].features[feature].idx === idx) {
                return feature;
            }
        }

        throw new Error(`Feature at index ${idx} of type ${type} does not exist`);
    }

    // publishes a type update to all metadata managers workers
    __publishTypes() {
        const publishOptions = ProtocolGateway.createPublishOptions(true);
        publishOptions.retain = true;

        return this.pg.publishJson(UPDATE_TYPES_TOPIC, {
            version: this.version,
            types: this.types
        }, publishOptions);
    }

    // Loads the configuration from the given path
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

    // Initializes the types configuration from the given configuration
    __initFromConfig(typesConfig) {
        return this.typesValidator(typesConfig)
            .catch(err => {
                throw new Error(`Validation of types configuration failed. Errors: ${ErrorMessageFormatter.formatAjvValidationError(err)}`);
            })
            .then(async () => {
                for(let segment of Object.keys(typesConfig)) {
                    if (segment === DEFAULT_SEGMENT) {
                        throw new Error(`Dynamic segment "${segment}" cannot be configured within the static types configuration`);
                    }

                    const segmentFeatures = Object.keys(typesConfig[segment].features);

                    for (let segmentFeature of segmentFeatures) {
                        await this.__registerFeature(segment, segmentFeature, typesConfig[segment].features[segmentFeature]);
                    }

                    for (let type of Object.keys(typesConfig[segment].types)) {
                        if (Object.prototype.hasOwnProperty.call(this.types, type)) {
                            // The type needs to be unique. It is not allowed to have the same type in two different segments
                            throw new Error(`Duplicate type ${type} found in segment ${segment}`);
                        }

                        const typeFeatures = Object.keys(typesConfig[segment].types[type].features);

                        if (typeFeatures.length === 0) {
                            if (segmentFeatures.length === 0) {
                                throw new Error(`Empty type ${type} found in segment ${segment}`);
                            }

                            // No additional features given. Use only segment features
                            this.__registerType(segment, type);
                        } else {
                            for (let typeFeature of typeFeatures) {
                                await this.__registerFeature(segment, typeFeature, typesConfig[segment].types[type].features[typeFeature], type);
                            }
                        }
                    }
                }
            });
    }

    // Creates a validator for types configurations
    __createValidatorFor(schema) {
        const schemaBasePath = path.normalize(path.join(__dirname, '../../resources'));

        return new Ajv({
            schemas: [
                JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'feature.schema.json')), { encoding: 'utf8' }),
                Object.assign(
                    JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'types.schema.json')), { encoding: 'utf8' }),
                    { $async: true }
                ),
                Object.assign(
                    JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'fullFeature.schema.json')), { encoding: 'utf8' }),
                    { $async: true }
                ),
                Object.assign(
                    JSON.parse(fs.readFileSync(path.join(schemaBasePath, 'fullIndexedFeature.schema.json')), { encoding: 'utf8' }),
                    { $async: true }
                )
            ]
        }).getSchema(schema);
    }

    __hasType(type) {
        return Object.prototype.hasOwnProperty.call(this.types, type);
    }

    __hasFeature(type, feature) {
        return this.__hasType(type) && this.types[type].features[feature] !== undefined;
    }

    async __validateFeature(segment, feature, metadata, type = null, checkFullFeature = false) {
        let featureIndex = null;

        if (checkFullFeature) {
            try {
                await this.fullFeatureValidator(metadata);
            }
            catch(err) {
                if (type === null) {
                    throw new Error(`The segment feature ${this.__formatSegmentFeature(segment, feature)} must be a full feature.`);
                } else {
                    throw new Error(`The type feature ${this.__formatTypeFeature(segment, type, feature)} must be a full feature.`);
                }
            }
        }

        if (Object.prototype.hasOwnProperty.call(metadata, 'idx')) {
            // If feature index is given, check if it's an integer
            if (!Number.isInteger(metadata.idx)) {
                throw new Error(`Given feature index has to be of type number`);
            }

            featureIndex = metadata.idx;

            // Once an index is provided, assume, that the given feature is a fully indexed feature
            try {
                await this.fullIndexedFeatureValidator(metadata);
            }
            catch(err) {
                if (type === null) {
                    throw new Error(`The segment feature ${this.__formatSegmentFeature(segment, feature)} must be a fully indexed feature.`);
                } else {
                    throw new Error(`The type feature ${this.__formatTypeFeature(segment, type, feature)} must be a fully indexed feature. Remove the idx property, if you want to inherit from a segment feature.`);
                }
            }
        }

        if (type === null) {
            // Check if there is a segment / type feature with the same name and a diverging index
            for (let next of this.allFeaturesInSegment(segment)) {
                if (next.feature !== feature) {
                    // Segment feature OR another feature
                    continue;
                }

                if (featureIndex !== next.$feature.idx) {
                    if (next.type === segment) {
                        // Segment feature
                        throw new Error(`Expect segment feature ${this.__formatSegmentFeature(next.type, next.feature)} to have index ${featureIndex}. Found ${next.$feature.idx}`);
                    } else {
                        throw new Error(`Expect type feature ${this.__formatTypeFeature(next.segment, next.type, next.feature)} to have index ${featureIndex}. Found ${next.$feature.idx}`);
                    }
                }

                // Type feature has same index as corresponding segment feature
                // Updates of existing segment features are still allowed
            }

        } else {
            // Check if there is a segment feature with the same name and a diverging index
            if (featureIndex !== null && this.__hasFeature(segment, feature)) {
                const segmentFeatureMeatadata = this.types[segment].features[feature];

                if (segmentFeatureMeatadata.idx !== featureIndex) {
                    throw new Error(`Expected type feature ${this.__formatTypeFeature(segment, type, feature)} to have index ${segmentFeatureMeatadata.idx}. Found ${featureIndex}`);
                }
            }
        }
    }

    /* istanbul ignore next */
    __formatTypeFeature(segment, type, feature) {
        return `"${segment}".types."${type}".features."${feature}"`;
    }

    /* istanbul ignore next */
    __formatSegmentFeature(segment, feature) {
        return `"${segment}".features."${feature}"`;
    }

    async __registerFeature(segment, feature, metadata, type = null) {
        if (segment === DEFAULT_SEGMENT) {
            // Default segment has only the default type
            type = DEFAULT_TYPE;
        }

        await this.__validateFeature(segment, feature, metadata, type);

        let typesChanged = this.__registerSegment(segment);

        // Only assign a new index for features, which NEITHER exist for the given type nor the given segment
        if (type !== null && !this.__hasFeature(segment, feature) && !this.__hasFeature(type, feature)) {
            if (!Number.isInteger(metadata.idx)) {
                // Only create a new index if no index was specified in the metadata of the new feature
                metadata.idx = this.__getMaxIndex(segment) + 1;
                typesChanged = true;
            }
        }

        let featureAssignee = this.types[segment];

        if (type !== null) {
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
        } else {
            // Set the unit to null
            metadata.unit = null;
        }

        if (this.__hasFeature(type || segment, feature)) {
            // Feature is already existing >> update the metadata but not the index (if actually given)
            const $metadata = Object.assign(metadata, { idx: featureAssignee.features[feature].idx });
            typesChanged = !equals(featureAssignee.features[feature], $metadata, { strictArrayOrder: false }) || typesChanged;
            featureAssignee.features[feature] = $metadata;
        } else {
            featureAssignee.features[feature] = metadata;
            typesChanged = true;
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

    __getMaxIndex(segment) {
        let maxIndex = -1;

        for (let next of this.allFeaturesInSegment(segment)) {
            maxIndex = Math.max(next.$feature.idx, maxIndex);
        }

        return maxIndex;
    }

    *allFeaturesInSegment(segment) {
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
                    feature,
                    segment: this.types[type].inherits,
                    type: type,
                    $feature: this.types[type].features[feature]
                };
            }
        }
    }
}

module.exports = MetadataManager;
