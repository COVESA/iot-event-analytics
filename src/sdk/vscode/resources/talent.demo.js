const iotea = require('boschio.iotea');

process.env.MQTT_TOPIC_NS = 'iotea/';

const Logger = iotea.util.Logger;
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const {
    MqttProtocolAdapter
} = iotea.util;

const {
    Talent,
    OpConstraint,
    Rule,
    AndRules,
    ProtocolGateway,
    TalentInput
} = iotea;

const {
    VALUE_TYPE_RAW
} = iotea.constants;

class MyTalent extends Talent {
    constructor(protocolGatewayConfig) {
        super('some-unique-talent-id', protocolGatewayConfig);
    }

    getRules() {
        return new AndRules([
            new Rule(new OpConstraint('anyfeature', OpConstraint.OPS.ISSET,  null, 'anytype', VALUE_TYPE_RAW))
        ]);
    }

    async onEvent(ev, evtctx) {
        this.logger.info(`${TalentInput.getRawValue(ev)}`, evtctx);
    }
}

// Update mqttAdapterConfig.config.brokerUrlt, if you specified a different one in your configuration !!!
const mqttAdapterConfig = MqttProtocolAdapter.createDefaultConfiguration();

new MyTalent(
    ProtocolGateway.createDefaultConfiguration([ mqttAdapterConfig ])
).start();