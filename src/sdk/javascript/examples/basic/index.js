/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

/**
 * Description:
 * Starts the IoT Event Analytics Platform and 2 talents to shows how you can build a multistage processing pipeline for events.
 * Also shows, that work can be distributed to multiple talent instances.
 *
 * Also test Timeseriespatternconstraint by sending events for feature tsctest of type kuehlschrank
 * // OK 3, 2, 5, 6 fires
 * // NOK 3, 2, 4, 6 does not fire
 */

const uuid = require('uuid').v4;
const path = require('path');
const express = require('express');

const Logger = require('../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;
const ConfigManager = require('../../../../core/configManager');
const Ingestion = require('../../../../core/ingestion');
const Encoding = require('../../../../core/encoding');
const Routing = require('../../../../core/routing');
const Talent = require('../../../../core/talent');

const MetadataApi = require('../../../../core/metadataApi');
const InstanceApi = require('../../../../core/instanceApi');

const {
    TimeseriesPatternConstraint
} = require('../../../../core/rules.tseries');

const {
    Wildcard
} = require('../../../../core/util/arrayPatternMatcher');

const {
    AndRules,
    Rule,
    OpConstraint,
    OrRules
} = require('../../../../core/rules');

const {
    VALUE_TYPE_RAW,
    DEFAULT_TYPE,
    TALENT_DISCOVERY_INTERVAL_MS,
    ENCODING_TYPE_NUMBER,
    ENCODING_ENCODER_DELTA,
    PLATFORM_EVENTS_TOPIC
} = require('../../../../core/constants');

const InstanceManager = require('../../../../core/instanceManager');

const ProtocolGateway = require('../../../../core/protocolGateway');

const {
    MqttProtocolAdapter
} = require('../../../../core/util/mqttClient');

const { TalentInput } = require('../../../../core/util/talentIO');

const mqttAdapterConfig1 = MqttProtocolAdapter.createDefaultConfiguration(true);
const platformGatewayConfig = ProtocolGateway.createDefaultConfiguration([ mqttAdapterConfig1 ]);
const talentGatewayConfig = ProtocolGateway.createDefaultConfiguration([ mqttAdapterConfig1 ]);

class MyTalent extends Talent {
    constructor(protocolGatewayConfig) {
        super('my-talent-id', protocolGatewayConfig);

        this.count = 0;

        this.addOutput('test2', {
            description: 'Hello this is a new output',
            history: 10,
            encoding: {
                type: ENCODING_TYPE_NUMBER,
                encoder: ENCODING_ENCODER_DELTA,
                min: 0,
                max: 100
            },
            unit: {
                fac: 1,
                unit: 'lexx',
                desc: 'custom unit'
            }
        });
    }

    getRules() {
        return new AndRules([
            new Rule(
                new OpConstraint('foo', OpConstraint.OPS.GREATER_THAN, 0, `kuehlschrank0`, VALUE_TYPE_RAW)
            ),
            new Rule(
                new OpConstraint('bar', OpConstraint.OPS.GREATER_THAN, 0, `kuehlschrank1`, VALUE_TYPE_RAW, '', '^4712$')
            )
        ]);
    }

    async onEvent(ev) {
        return [{
            feature: `${this.id}.test2`,
            value: this.count++,
            subject: ev.subject
        }];
    }
}

class MyTalent2 extends Talent {
    constructor(protocolGatewayConfig) {
        super('my-talent2-id', protocolGatewayConfig);
        this.uuid = uuid();
    }

    getRules() {
        const rules = new OrRules([
            new Rule(
                new OpConstraint('my-talent-id.test2', OpConstraint.OPS.GREATER_THAN, -1, DEFAULT_TYPE, VALUE_TYPE_RAW)
            ),
            new Rule(new TimeseriesPatternConstraint(
                'tsctest',
                // OK 3, 2, 5, 6
                // NOK 3, 2, 4, 6
                [ 3, new Wildcard().accept(2, 5).minValues(1).reject(4), 6 ],
                'kuehlschrank',
                VALUE_TYPE_RAW
            ))
        ]);

        return rules;
    }

    async onEvent(ev, evtctx) {
        this.logger.info(`${this.uuid} ${TalentInput.getRawValue(ev)} ${TalentInput.getEncodedValue(ev)}`, evtctx);
        this.logger.info(`History: ${JSON.stringify(TalentInput.getRawValue(ev, 100, true))}`, evtctx);
        this.logger.info(`Unit: ${JSON.stringify(TalentInput.getMetadata(ev))}`, evtctx);
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
const t1 = new MyTalent(talentGatewayConfig);
const t2 = new MyTalent2(talentGatewayConfig);
const t21 = new MyTalent2(talentGatewayConfig);

const platformLogger = new Logger('Platform');

cf.start(TALENT_DISCOVERY_INTERVAL_MS, path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => t1.start())
    .then(() => t2.start())
    .then(() => t21.start())
    .then(() => instanceManager.start())
    .catch(err => {
        platformLogger.error('Failed to start IoT Event Analytics Platform', null, err);
    });