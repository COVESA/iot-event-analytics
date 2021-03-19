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

const Logger = require('../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const ConfigManager = require('../../../../core/configManager');
const Ingestion = require('../../../../core/ingestion');
const Encoding = require('../../../../core/encoding');
const Routing = require('../../../../core/routing');
const Talent = require('../../../../core/talent');

const {
    AndRules,
    Rule,
    ChangeConstraint
} = require('../../../../core/rules');

const {
    VALUE_TYPE_ENCODED,
    TALENT_DISCOVERY_INTERVAL_MS
} = require('../../../../core/constants');

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
    }

    getRules() {
        const rules = new AndRules([
            new Rule(
                new ChangeConstraint('lightcolor', 'mybulb', VALUE_TYPE_ENCODED)
            )
        ]);

        return rules;
    }

    async onEvent(ev, evtctx) {
        const metadata = TalentInput.getMetadata(ev, 'lightcolor', 'mybulb');

        const rgbEnc = Math.round(TalentInput.getEncodedValue(ev) * metadata.encoding.max);

        const b = rgbEnc & 0xFF;
        const g = (rgbEnc & (0xFF << 8)) >> 8;
        const r = (rgbEnc & (0xFF << 16)) >> 16;

        this.logger.info(`Raw value: ${JSON.stringify(TalentInput.getRawValue(ev))} Encoded value: ${TalentInput.getEncodedValue(ev)} Raw from encoded value: ${r}, ${g}, ${b}`, evtctx);
    }
}

const PLATFORM_ID = '123456';

const cf = new ConfigManager(platformGatewayConfig, PLATFORM_ID);
const ing = new Ingestion(platformGatewayConfig, PLATFORM_ID);
const enc = new Encoding(platformGatewayConfig);
const rou = new Routing(platformGatewayConfig, PLATFORM_ID);
const t1 = new MyTalent(talentGatewayConfig);

const platformLogger = new Logger('Platform');

cf.start(TALENT_DISCOVERY_INTERVAL_MS, path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => t1.start())
    .catch(err => {
        platformLogger.error(err.message, null, err);
    });