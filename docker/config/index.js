const path = require('path');
const express = require('express');

const ConfigManager = require('../../src/core/configManager');
const MetadataApi = require('../../src/core/metadataApi');
const InstanceApi = require('../../src/core/instanceApi');

const JsonModel = require('../../src/core/util/jsonModel');

const config = new JsonModel(require('./config/config.json'));

const {
    TALENT_DISCOVERY_INTERVAL_MS
} = require('../../src/core/constants');

const Logger = require('../../src/core/util/logger');

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.WARN);

const InstanceManager = require('../../src/core/instanceManager');

try {
    process.env.MQTT_TOPIC_NS = config.get('mqtt.ns');
}
catch(err) {
    delete process.env.MQTT_TOPIC_NS;
}

let platformLogger = undefined;
let platformId = undefined;

try {
    platformId = config.get('platformId');
    platformLogger = new Logger(`IoT Event Analytics Platform-${platformId}`);
    platformLogger.info(`Local client installation ${platformId}`);
}
catch(err) {
    platformLogger = new Logger(`IoT Event Analytics Platform`);
}

platformLogger.info(`Starting IoT Event Analytics ConfigManager...`);

const configManager = new ConfigManager(config.get('mqtt.connectionString'), platformId);

const app = express();

let metadataManager = configManager.getMetadataManager();
let apiInstanceManager = null;

try {
    config.get('api.instance');
    apiInstanceManager = new InstanceManager(config.get('mqtt.connectionString'));
    // Reuse the metadata manager for metadata API
    metadataManager = apiInstanceManager.getMetadataManager();
    const instanceApi = new InstanceApi(apiInstanceManager);
    app.use('/instance/api/v1', instanceApi.createApiV1());
}
catch(err) {
    // No instance API configuration found
    this.loger.info('No instance API configuration found');
}

try {
    config.get('api.metadata');
    const metadataApi = new MetadataApi(metadataManager);
    app.use('/metadata/api/v1', metadataApi.createApiV1());
}
catch(err) {
    // No metadata API configuration found
    this.loger.info('No metadata API configuration found');
}

configManager.start(config.get('talentDiscoveryIntervalMs', TALENT_DISCOVERY_INTERVAL_MS), path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => {
        if (apiInstanceManager === null) {
            return;
        }

        return apiInstanceManager.start();
    })
    .then(() => {
        try {
            const apiPort = config.get('api.port');

            return new Promise(resolve => {
                app.listen(apiPort, '0.0.0.0', resolve);
            });
        }
        catch(err) {
            // No API port given
            this.loger.info('No API port given.');
        }
    })
    .then(() => {
        platformLogger.info(`IoT Event Analytics ConfigManager started successfully`);
    })
    .catch(err => {
        platformLogger.error(err.message, null, err);
    });