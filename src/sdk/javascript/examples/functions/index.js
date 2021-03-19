/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

process.env.MQTT_TOPIC_NS = 'iotea/';

const path = require('path');
const express = require('express');

const Logger = require('../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;
const ConfigManager = require('../../../../core/configManager');
const Ingestion = require('../../../../core/ingestion');
const Encoding = require('../../../../core/encoding');
const Routing = require('../../../../core/routing');
const Talent = require('../../../../core/talent');
const FunctionTalent = require('../../../../core/talent.func');

const MetadataApi = require('../../../../core/metadataApi');
const InstanceApi = require('../../../../core/instanceApi');

const {
    Rule,
    OrRules,
    OpConstraint
} = require('../../../../core/rules');

const {
    VALUE_TYPE_RAW,
    TALENT_DISCOVERY_INTERVAL_MS,
    PLATFORM_EVENTS_TOPIC
} = require('../../../../core/constants');

const InstanceManager = require('../../../../core/instanceManager');

const {
    MqttProtocolAdapter
} = require('../../../../core/util/mqttClient');

const ProtocolGateway = require('../../../../core/protocolGateway');

const mqttAdapterConfig = MqttProtocolAdapter.createDefaultConfiguration(true);
const platformGatewayConfig = ProtocolGateway.createDefaultConfiguration([ mqttAdapterConfig ]);

class MathFunctions extends FunctionTalent {
    constructor(protocolGatewayConfig) {
        super('math', protocolGatewayConfig);
        this.registerFunction('multiply', this.__multiply.bind(this));
        this.registerFunction('fibonacci', this.__fibonacci.bind(this));
    }

    callees() {
        return [
            `${this.id}.fibonacci`
        ];
    }

    async __multiply(a, b, ev, evtctx) {
        this.logger.info(`Multiplying numbers ${a} and ${b}...`, evtctx);

        await new Promise(resolve => setTimeout(resolve, Math.random() * 1000));

        return a * b;
    }

    // Actually a bad example, but for the sake of demoing recursive function execution it works out
    async __fibonacci(nth, ev, evtctx) {
        this.logger.debug(`Calculating ${nth}th fibonacci number...`, evtctx);

        if (nth <= 1) {
            this.logger.debug(`Result for ${nth}th fibonacci number is ${nth}`);
            return nth;
        }

        const fib = await this.call(this.id, 'fibonacci', [ nth - 1 ], ev.subject, ev.returnTopic, 20000) + await this.call(this.id, 'fibonacci', [ nth - 2 ], ev.subject, ev.returnTopic, 20000);

        this.logger.debug(`Result for ${nth}th fibonacci number is ${fib}`);

        return fib;
    }
}

class TempTalent extends Talent {
    constructor(protocolGatewayConfig) {
        super('temp-talent', protocolGatewayConfig);
    }

    callees() {
        return [
            'math.multiply',
            'math.fibonacci'
        ];
    }

    getRules() {
        return new OrRules([
            new Rule(
                new OpConstraint('temp', OpConstraint.OPS.ISSET, null, 'kuehlschrank', VALUE_TYPE_RAW)
            )
        ]);
    }

    async onEvent(ev, evtctx) {
        try {
            this.logger.always(`Calculating ${ev.value}th fibonacci number...`, evtctx);
            const fibResult = await this.call('math', 'fibonacci', [ ev.value ], ev.subject, ev.returnTopic, 20000);
            this.logger.always(`${ev.value}th fibonacci number is ${fibResult}`, evtctx);
            this.logger.always(`Multiplying ${ev.value}x${ev.value}...`, evtctx);
            const mulResult = await this.call('math', 'multiply', [ ev.value, ev.value ], ev.subject, ev.returnTopic);
            this.logger.always(`Multiplication result of ${ev.value}x${ev.value} is ${mulResult}`, evtctx);
        }
        catch(err) {
            this.logger.error(err.message, evtctx, err);
        }
    }
}

const PLATFORM_ID = '123456';

const cf = new ConfigManager(platformGatewayConfig, PLATFORM_ID);

const instanceManager = new InstanceManager(platformGatewayConfig);
const instanceApi = new InstanceApi(instanceManager);
const metadataApi = new MetadataApi(cf.getMetadataManager());

const app = express();
app.use('/metadata/api/v1', metadataApi.createApiV1());
app.use('/data/api/v1', instanceApi.createApiV1());
app.listen(8080);

const platformEventLogger = new Logger('PlatformEvents');
const pg = new ProtocolGateway(platformGatewayConfig, 'PlatformEvents', true);

pg.subscribeJson(PLATFORM_EVENTS_TOPIC, ev => {
    platformEventLogger.info(`Received platform event of type ` + ev.type);
    platformEventLogger.info(JSON.stringify(ev.data));
});

const ing = new Ingestion(platformGatewayConfig, PLATFORM_ID);
const enc = new Encoding(platformGatewayConfig);
const rou = new Routing(platformGatewayConfig, PLATFORM_ID);
const mathFunctions1 = new MathFunctions(platformGatewayConfig);
const mathFunctions2 = new MathFunctions(platformGatewayConfig);
const tempTalent = new TempTalent(platformGatewayConfig);
const platformLogger = new Logger('Platform');

mathFunctions1.start()
    .then(() => mathFunctions2.start())
    .then(() => tempTalent.start())
    .then(() => cf.start(TALENT_DISCOVERY_INTERVAL_MS, path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json')))
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => instanceManager.start())
    .catch(err => {
        platformLogger.error('Failed to start IoT Event Analytics Platform', null, err);
    });