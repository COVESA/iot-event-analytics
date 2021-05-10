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

const Ingestion = require('../../src/core/ingestion');
const Encoding = require('../../src/core/encoding');
const Routing = require('../../src/core/routing');
const ConfigManager = require('../../src/core/configManager');

const JsonModel = require('../../src/core/util/jsonModel');

const config = new JsonModel(require('./config/config.json'));

const {
    TALENT_DISCOVERY_INTERVAL_MS
} = require('../../src/core/constants');

const Logger = require('../../src/core/util/logger');

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.WARN);

const InstanceManager = require('../../src/core/instanceManager');

const platformId = config.get('platformId', 'default');

let app = null;

function lazyInitApp() {
    if (app === null) {
        const express = require('express');
        app = express();
    }

    return app;
}

const platformLogger = new Logger(`IoT Event Analytics Platform (${platformId})`);
platformLogger.info(`Starting...`);

const configManager = new ConfigManager(config.get('protocolGateway'), platformId);
const ing = new Ingestion(config.get('protocolGateway'), platformId);
const enc = new Encoding(config.get('protocolGateway'));
const rou = new Routing(config.get('protocolGateway'), platformId);

let metadataManager = configManager.getMetadataManager();
let apiInstanceManager = null;

try {
    config.get('api.instance');
    const InstanceApi = require('../../src/core/instanceApi');
    apiInstanceManager = new InstanceManager(config.get('protocolGateway'));
    // Reuse the metadata manager for metadata API
    metadataManager = apiInstanceManager.getMetadataManager();
    const instanceApi = new InstanceApi(apiInstanceManager);
    lazyInitApp().use('/instance/api/v1', instanceApi.createApiV1());
}
catch(err) {
    // No instance API configuration found
    platformLogger.info('No instance API configuration found');
}

try {
    config.get('api.metadata');
    const MetadataApi = require('../../src/core/metadataApi');
    const metadataApi = new MetadataApi(metadataManager);
    lazyInitApp().use('/metadata/api/v1', metadataApi.createApiV1());
}
catch(err) {
    // No metadata API configuration found
    platformLogger.info('No metadata API configuration found');
}

configManager.start(config.get('talentDiscoveryIntervalMs', TALENT_DISCOVERY_INTERVAL_MS), path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => {
        if (apiInstanceManager === null) {
            return;
        }

        return apiInstanceManager.start();
    })
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => {
        try {
            const apiPort = config.get('api.port');

            return new Promise(resolve => {
                lazyInitApp().listen(apiPort, '0.0.0.0', resolve);
            });
        }
        catch(err) {
            // No API port given
            platformLogger.info('No API port given.');
        }
    })
    .then(() => {
        platformLogger.info(`IoT Event Analytics Platform started successfully`);
    })
    .catch(err => {
        platformLogger.error(err.message, null, err);
    });