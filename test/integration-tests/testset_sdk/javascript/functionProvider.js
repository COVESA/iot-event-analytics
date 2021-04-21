const iotea = require('boschio.iotea');

const {
    FunctionTalent,
    ProtocolGateway
} = iotea;

const {
    Logger,
    MqttProtocolAdapter
} = iotea.util;

process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

class FunctionProvider extends FunctionTalent {
    constructor(protocolGatewayConfig) {
        super('function-provider-js', protocolGatewayConfig);

        // Register Functions
        this.registerFunction('echo', this.echo.bind(this));
    }

    async echo(value, ev, evtctx, timeoutAtMs) {
        this.logger.debug('Echo called');
        return value;
    }
}

// TODO Could add some sort of proper configuration here 
// docker-compose bridged network configuration
const fp = new FunctionProvider(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(false,"mqtt://mosquitto:1883") ]));

fp.start();
