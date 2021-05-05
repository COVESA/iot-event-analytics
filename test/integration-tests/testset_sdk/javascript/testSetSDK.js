const { ENCODING_TYPE_STRING } = require('../../../../src/core/constants.js');
const iotea = require('../../../../src/module.js');

// const iotea = require('boschio.iotea');

const {
    TestSetTalent,
    ProtocolGateway
} = iotea;

const {
    Logger,
    MqttProtocolAdapter
} = iotea.util;

process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

class TestSetSDK extends TestSetTalent {
    constructor(protocolGatewayConfig) {
        super('testSet-sdk-js', protocolGatewayConfig);

        // Register Tests

        // Test primitives via echo
        this.registerTest('echoString', "Hello World", this.test_echoString.bind(this), 5000);
        this.registerTest('echoBoolean', true, this.test_echoBoolean.bind(this), 5000);
        this.registerTest('echoInteger', 123, this.test_echoInteger.bind(this), 5000);
        this.registerTest('echoDouble', 123.456, this.test_echoDouble.bind(this), 5000);

        // Test lists/arrays via echo
        this.registerTest('echoEmptyList', [ ], this.test_echoEmptyList.bind(this), 5000);
        this.registerTest('echoIntegerList', [ 1, 2, 3 ], this.test_echoIntegerList.bind(this), 5000);
        this.registerTest('echoMixedList', [1, 'Hello World', 3.21], this.test_echoMixedList.bind(this), 5000);
        this.registerTest('echoDeepList', [1, [2, [3, [4, [5]]]]], this.test_echoDeepList.bind(this), 5000);

        this.registerTest('receiveEvent1ByMultipleTalents', true, this.test_receiveEvent1ByMultipleTalents.bind(this), 5000);

        // Add external talentIds which this test set is depending on
        this.talentDependencies.addTalent('function-provider-js');
        this.talentDependencies.addTalent('event-tester-1-js');
        this.talentDependencies.addTalent('event-tester-2-js');

        this.addOutput("receive_event_1", {
            "description": "receiveEvent1",
                "idx": 1,
                "encoding": {
                    "type": "string",
                    "encoder": null
                }
            });
    }

    callees() {
        return [
            'function-provider-js.echo',
            'event-tester-1-js.get_received_events',
            'event-tester-2-js.get_received_events',
        ];
    }

    async test_receiveEvent1ByMultipleTalents(ev, evtctx) {
        this.logger.info('test_receiveEvent1')
        const type = 'default';
        const feature = 'testSet-sdk-js.receive_event_1';
        const value = 'this is a string';

        let outEvent = {
            type: type,
            feature: feature,
            instance: '1',
            value: value,
            subject: ev.subject,
            whenMs: Date.now(),
        };

        this.pg.publishJson(ev.returnTopic, outEvent)

        // Check if the event was received by event-tester-1
        let received_events1 = await this.call('event-tester-1-js', 'get_received_events',
            [],
            ev.subject,
            ev.returnTopic,
            500);

        let match1 = received_events1.find((event) => event.feature === feature && event.type === type);

        // Check if the event was received by event-tester-2
        let received_events2 = await this.call('event-tester-2-js', 'get_received_events',
            [],
            ev.subject,
            ev.returnTopic,
            500);

        let match2 = received_events2.find((event) => event.feature === feature && event.type === type);


        if (match1 === undefined) {
            throw Error('Event wasn\'t received by event-tester-1-js');
        }

        if (match2 === undefined) {
            throw Error('Event wasn\'t received by event-tester-2-js');
        }

        return match1.value === value && match2.value === value;
    }

    async test_echoString(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ 'Hello World' ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoBoolean(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ true ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoInteger(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ 123 ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoDouble(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ 123.456 ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoEmptyList(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ [] ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoIntegerList(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ [1, 2, 3] ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoMixedList(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ [1, 'Hello World', 3.21] ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async test_echoDeepList(ev, evtctx) {
        let result = await this.call('function-provider-js','echo',
                                [ [1, [2, [3, [4, [5]]]]] ],
                                ev.subject,
                                ev.returnTopic,
                                500);

        return result;
    }

    async prepare(ev, evctx) {
        // Add additional preperation beside talent which have been already defined as dependency
        // (see this.talentDependencies)
        return (true && await super.prepare(ev, evctx));
    }
}

const tss = new TestSetSDK(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration() ]));

tss.start();
