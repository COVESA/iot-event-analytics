const iotea = require('../../../../src/module.js');
// const iotea = require('boschio.iotea');

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

const fp = new FunctionProvider(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration() ]));

fp.start();
