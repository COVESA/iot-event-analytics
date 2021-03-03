const path = require('path');

const Ingestion = require('../../src/core/ingestion');
const Encoding = require('../../src/core/encoding');
const Routing = require('../../src/core/routing');

const JsonModel = require('../../src/core/util/jsonModel');

const config = new JsonModel(require('./config/config.json'));

const Logger = require('../../src/core/util/logger');

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.WARN);

try {
    process.env.MQTT_TOPIC_NS = config.get('mqtt.ns');
}
catch(err) {
    delete process.env.MQTT_TOPIC_NS;
}

let pipelineLogger = undefined;
let platformId = undefined;

try {
    platformId = config.get('platformId');
    pipelineLogger = new Logger(`IoTeaPipeline-${platformId}`);
    pipelineLogger.info(`Local client installation ${platformId}`);
}
catch(err) {
    pipelineLogger = new Logger(`IoTeaPipeline`);
}

pipelineLogger.info(`Starting IoT Event Analytics Pipeline...`);

const ing = new Ingestion(config.get('mqtt.connectionString'));
const enc = new Encoding(config.get('mqtt.connectionString'));
const rou = new Routing(config.get('mqtt.connectionString'), platformId);

ing.start(path.resolve(__dirname, 'config', 'channels'))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => {
        pipelineLogger.info(`IoT Event Analytics Pipeline started successfully`);
    })
    .catch(err => {
        pipelineLogger.error(err.message, null, err);
    });