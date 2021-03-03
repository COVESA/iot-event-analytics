const iotea = require('../../../../src/module.js');
//const iotea = require('boschio.iotea');

process.env.MQTT_TOPIC_NS = 'iotea/';

const Logger = iotea.util.Logger;
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const {
    TestRunnerTalent
} = require('../../../../src/core/talent.test.js')

class TestRunner extends TestRunnerTalent {
    constructor(connectionString) {
        // Define your testSetTalent list and set via super constructor
        super('testRunner-js', ['testSet-sdk-js', 'testSet-sdk-py', 'testSet-sdk-cpp'], connectionString);
        
        // you can run singular tests say for development also 
        //super('testRunner-js', ['testSet-sdk-js'], connectionString);
    }
}

const runner = new TestRunner('mqtt://localhost:1883');

runner.start();
