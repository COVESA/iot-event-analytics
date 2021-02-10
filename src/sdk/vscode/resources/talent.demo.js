const iotea = require('boschio.iotea');

process.env.MQTT_TOPIC_NS = 'iotea/';

const Logger = iotea.util.Logger;
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const {
    Talent,
    OpConstraint,
    ChangeConstraint,
    Rule,
    AndRules,
    OrRules
} = iotea;

const {
    VALUE_TYPE_RAW,
    VALUE_TYPE_ENCODED
} = iotea.constants;

class MyTalent extends Talent {
    constructor(connectionString) {
        super('some-unique-talent-id', connectionString);
    }

    getRules() {
        return new AndRules([
            new Rule(new OpConstraint('anyfeature', OpConstraint.OPS.ISSET,  null, 'anytype', VALUE_TYPE_RAW))
        ]);
    }

    async onEvent(ev, evtctx) {
        this.logger.info(`${JSON.stringify(ev.value)}`, evtctx);
    }
}

new MyTalent('mqtt://localhost:1883').start();