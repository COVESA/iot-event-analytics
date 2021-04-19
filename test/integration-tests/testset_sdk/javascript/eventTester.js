const { DEFAULT_TYPE, VALUE_TYPE_RAW } = require('../../../../src/core/constants.js');
const iotea = require('../../../../src/module.js');
// const iotea = require('boschio.iotea');

const {
    FunctionTalent,
    ProtocolGateway,
    OrRules,
    Rule,
    OpConstraint
} = iotea;

const {
    Logger,
    MqttProtocolAdapter
} = iotea.util;

process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

class EventTester extends FunctionTalent {
    constructor(name, protocolGatewayConfig) {
        super(name, protocolGatewayConfig);

        this.receivedEvents = [];
        // Register Functions
        this.registerFunction('get_received_events', this.getReceivedEvents.bind(this));
    }

    async getReceivedEvents(value, ev, evtctx) {
        this.logger.info('getReceivedEvents called');
        return this.receivedEvents;
    }

    async onEvent(ev, evtctx) {
        this.logger.info(`received event: ${ev}`);
        this.receivedEvents.push(ev);
    }

    getRules() {
        return new OrRules([
            new Rule(
                new OpConstraint('testSet-sdk-js.receive_event_1', OpConstraint.OPS.ISSET, 0, 'default', VALUE_TYPE_RAW)
            )
        ]);
    }
}

const talent1 = new EventTester('event-tester-1-js', ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration() ]));
const talent2 = new EventTester('event-tester-2-js', ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration() ]));

talent1.start();
talent2.start();
