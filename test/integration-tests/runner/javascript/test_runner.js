const iotea = require('../../../../src/module.js');
// const iotea = require('boschio.iotea');

const {
    TestRunnerTalent,
    ProtocolGateway
} = iotea;

const {
    Logger
    MqttProtocolAdapter
} = iotea.util;

process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

class TestRunner extends TestRunnerTalent {
    constructor(protocolGatewayConfig) {
        // Define your testSetTalent list and set via super constructor
        super('testRunner-js', ['testSet-sdk-js', 'testSet-sdk-py', 'testSet-sdk-cpp'], protocolGatewayConfig);

        // you can run singular tests say for development also
        //super('testRunner-js', ['testSet-sdk-js'], protocolGatewayConfig);
    }
}

const runner = new TestRunner(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration() ]));

runner.start();
