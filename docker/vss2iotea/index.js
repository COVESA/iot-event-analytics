const uuid = require('uuid').v4;

const {
    VssAdapter,
    VssPathTranslator
} = require('../../src/adapter/vss/vss.adapter');
const JsonModel = require('../../src/core/util/jsonModel');

const Logger = require('../../src/core/util/logger');

const config = new JsonModel(require('./config/config.json'));

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.WARN);

try {
    process.env.MQTT_TOPIC_NS = config.get('mqtt.ns');
}
catch(err) {
    delete process.env.MQTT_TOPIC_NS;
}

let vss2ioteaLogger = new Logger(`Vss2IoTEventAnalytics`);

const vssAdapter = new VssAdapter(
    config.get('mqtt.connectionString'),
    config.get('vss.ws'),
    config.get('vss.jwt'),
    config.get('segment'),
    uuid(),
    {
        rejectUnauthorized: false
    },
    true
);

let vssPathConfig = config.get('vss.pathConfig', null);

if (vssPathConfig !== null) {
    vssAdapter.setVssPathTranslator(new VssPathTranslator(vssPathConfig));
}

vss2ioteaLogger.info('Current VSS Adapter Configuration');
vss2ioteaLogger.info(JSON.stringify(config));
vss2ioteaLogger.info(`mqtt connectionString: ${config.get('mqtt.connectionString')}`);

const instanceIdPath = config.get('paths.instanceId');
const userIdPath = config.get('paths.userId');

vssAdapter.start(instanceIdPath, userIdPath)
    .then(() => {
        vss2ioteaLogger.info(`VSS Adapter started successfully on: ${config.get('mqtt.connectionString')}`);
    })
    .catch(err => {
        vss2ioteaLogger.error(err.message, null, err);
    });