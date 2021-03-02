const iotea = require('../../../../src/module.js');
//const iotea = require('boschio.iotea');

process.env.MQTT_TOPIC_NS = 'iotea/';

const Logger = iotea.util.Logger;
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const {
    TestRunnerTalent
} = require('../../../../src/core/talent.test.js')

class JavascriptTestRunner extends TestRunnerTalent {
    constructor(connectionString) {
        // Define your testSetTalent list and set via super constructor
        super('javascript-test-runner', ['rpc-js'], connectionString);
    }
}

const runner = new JavascriptTestRunner('mqtt://localhost:1883');

runner.start();
