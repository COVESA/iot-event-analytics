/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const path = require('path');
const fs = require('fs').promises;
const yargs = require('yargs');

const {
    VssPathTranslator
} = require('../../adapter/kuksa.val/kuksa.val.adapter');

const JsonModel = require('../../core/util/jsonModel');

const {
    ENCODING_ENCODER_CATEGORY,
    ENCODING_ENCODER_MINMAX,
    ENCODING_TYPE_NUMBER,
    ENCODING_TYPE_STRING,
    ENCODING_TYPE_OBJECT,
    ENCODING_TYPE_BOOLEAN
} = require('../../core/constants');

const args = yargs
    .option('segment', {
        alias: 's',
        description: 'Defines the segment, in which the VSS types are imported',
        required: true
    })
    .option('vss-input-path', {
        alias: 'v',
        description: 'Input vss.json file',
        required: true
    })
    .option('vss-uom-input-path', {
        alias: 'u',
        description: 'VSS unit to IoT Event Analytics unit Lookup file',
        default: null
    })
    .option('types-input-path', {
        alias: 'i',
        description: 'Input types file',
        default: null
    })
    .boolean('segment-overwrite')
    .describe('segment-overwrite', 'If true, an already existing segment in types input file will be overwritten')
    .alias('f', 'segment-overwrite')
    .default('segment-overwrite', false)
    .option('types-output-path', {
        alias: 'o',
        description: 'Output types file',
        default: 'types.out.json'
    })
    .help()
    .alias('help', 'h');

const argv = args.argv;

const vssSegment = argv.s;
const overwriteSegment = argv.f;
const vssInputPath = path.resolve(__dirname, argv.v);
const typesInputPath = argv.i !== null ? path.resolve(__dirname, argv.i) : null;
const vss2UomInputPath = argv.u !== null ? path.resolve(__dirname, argv.u) : null;
const typesOutputPath = path.resolve(__dirname, argv.o);

let vssPathTranslator = new VssPathTranslator();
let vssPathConfig = new JsonModel(vssPathTranslator.vssPathConfig);

try {
    const config = new JsonModel(require('./config.json'));
    vssPathConfig = config.getSubmodel('vss.pathConfig');
    vssPathTranslator = new VssPathTranslator(vssPathConfig);
}
catch(err) {
    console.log(`An error occurred loading the configuration for VSS path translation: ${err.message}`);
    // No valid path translator configuration found
    process.exit(1);
}

readJson(vssInputPath)
    .then(async vssJson => {
        let typesJson = {};

        if (typesInputPath !== null) {
            typesJson = await readJson(typesInputPath);
        }

        let vss2UomJson = {};

        if (vss2UomInputPath !== null) {
            vss2UomJson = await readJson(vss2UomInputPath);
        }

        if (Object.prototype.hasOwnProperty.call(typesJson, vssSegment)) {
            if (!overwriteSegment) {
                throw new Error(`Segment ${vssSegment} is already defined in input IoT Event Analytics configuration ${typesInputPath}. To overwrite, call converter with option -f true`);
            }
        }

        typesJson[vssSegment] = {
            features: {},
            types: {}
        };

        for (let type of Object.keys(vssJson)) {
            type = vssPathTranslator.kuksaVss2Iotea(type);

            typesJson[vssSegment].types[type] = {
                features: {}
            };

            walkModel(vssJson[type], '', typesJson[vssSegment].types[type], vss2UomJson);
        }

        return fs.writeFile(typesOutputPath, JSON.stringify(typesJson, null, 4));
    })
    .then(() => {
        console.log(`IoT Event Analytics configuration successfully written to ${typesOutputPath}`);
    })
    .catch(err => {
        console.error(err);
        process.exit(1);
    });

function readJson(absPath) {
    return fs.readFile(absPath, { encoding: 'utf8',  }).then(content => {
        return JSON.parse(content);
    });
}

function walkModel(node, featurePath, typeConfig, vss2UomJson, nextIndex = 0) {
    if (node.type === 'branch') {
        // Iterate through all children
        for (let child of Object.keys(node.children)) {
            let nextFeaturePath = featurePath;

            if (nextFeaturePath !== '') {
                nextFeaturePath += vssPathConfig.get('separator');
            }

            nextFeaturePath += child;

            nextIndex = walkModel(node.children[child], nextFeaturePath, typeConfig, vss2UomJson, nextIndex);
        }

        return nextIndex;
    }

    if (node.type === 'rbranch') {
        // Add feature
        const metadata = {
            description: node['prop-description'].join(', '),
            unit: 'ONE',
            idx: nextIndex++
        };

        metadata.encoding = {
            type: ENCODING_TYPE_OBJECT,
            encoder: null
        };

        typeConfig[vssPathTranslator.kuksaVss2Iotea(featurePath)] = metadata;

        return nextIndex;
    }

    if (node.type === 'attribute' || node.type === 'sensor' || node.type === 'actuator') {
        // Add feature
        const metadata = {
            description: node.description || 'None',
            idx: nextIndex++
        };

        if (node.default !== undefined) {
            metadata.default = node.default;
        }

        const unit = resolveUnit(node, vss2UomJson);

        if (unit !== undefined) {
            metadata.unit = unit;
        }

        metadata.encoding = {};

        switch(node.datatype.toLowerCase()) {
            case 'string': {
                metadata.encoding.type = ENCODING_TYPE_STRING;

                if (node.enum) {
                    metadata.encoding.enum = node.enum;
                    metadata.encoding.encoder = ENCODING_ENCODER_CATEGORY;
                } else {
                    metadata.encoding.encoder = null;
                }

                break;
            }
            case 'int8':
            case 'uint8':
            case 'int16':
            case 'uint16':
            case 'int32':
            case 'uint32':
            case 'float':
            case 'double': {
                metadata.encoding.type = ENCODING_TYPE_NUMBER;

                if (node.max !== undefined && node.min !== undefined) {
                    metadata.encoding.encoder = ENCODING_ENCODER_MINMAX;
                    metadata.encoding.min = node.min;
                    metadata.encoding.max = node.max;
                } else {
                    metadata.encoding.encoder = null;
                }

                break;
            }
            case 'uint8[]':
            case 'string[]': {
                metadata.encoding.type = 'object';
                metadata.encoding.encoder = null;
                break;
            }
            case 'boolean': {
                metadata.encoding.type = ENCODING_TYPE_BOOLEAN;
                metadata.encoding.enum = [ true, false ];
                metadata.encoding.encoder = ENCODING_ENCODER_CATEGORY;
                break;
            }
            default: {
                throw new Error('Type unknown ' + node.datatype);
            }
        }

        typeConfig.features[vssPathTranslator.kuksaVss2Iotea(featurePath)] = metadata;

        return nextIndex;
    }

    throw new Error(`Invalid type ${node.type}`);
}

function resolveUnit(node, vss2UomJson) {
    let unit = node.unit;

    if (unit === undefined) {
        return undefined;
    }

    if (Object.prototype.hasOwnProperty.call(vss2UomJson, unit)) {
        return vss2UomJson[unit];
    }

    throw new Error(`Unknown unit ${node.unit}`);
}