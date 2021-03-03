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

class MyTalent extends Talent {
    constructor(connectionString) {
        super('my-talent-id', connectionString);
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
        const rgbEnc = Math.round(ev.$feature.enc * ev.$features.mybulb.lightcolor.$metadata.encoding.max);

        const b = rgbEnc & 0xFF;
        const g = (rgbEnc & (0xFF << 8)) >> 8;
        const r = (rgbEnc & (0xFF << 16)) >> 16;

        this.logger.info(`Raw value: ${ev.$feature.raw} Encoded value: ${ev.$feature.enc} Raw from encoded value: ${r}, ${g}, ${b}`, evtctx);
    }
}

const cf = new ConfigManager('mqtt://localhost:1883', '123456');
const ing = new Ingestion('mqtt://localhost:1883');
const enc = new Encoding('mqtt://localhost:1883');
const rou = new Routing('mqtt://localhost:1883', '123456');
const t1 = new MyTalent('mqtt://localhost:1883');
const platformLogger = new Logger('Platform');

cf.start(TALENT_DISCOVERY_INTERVAL_MS, path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => t1.start())
    .catch(err => {
        platformLogger.error(err.message, null, err);
    });