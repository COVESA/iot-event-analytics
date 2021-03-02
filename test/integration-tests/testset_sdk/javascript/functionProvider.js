//const iotea = require('boschio.iotea');
const iotea = require('../../../../src/module.js');

process.env.MQTT_TOPIC_NS = 'iotea/';

const Logger = iotea.util.Logger;
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const {
    FunctionTalent,
} = iotea;

class FunctionProvider extends FunctionTalent {
    constructor(connectionString) {
        super('function-provider-js', connectionString);

        // Register Functions
        this.registerFunction("echo", this.echo.bind(this));
    }

    async echo(value, ev, evctx) {
        this.logger.debug("Echo called");
        return value;
    }
}

const t1 = new FunctionProvider('mqtt://localhost:1883');
t1.start()
