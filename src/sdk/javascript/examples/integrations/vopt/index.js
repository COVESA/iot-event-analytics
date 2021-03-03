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
 *
 * Description:
 * Starts the IoT Event Analytics Platform together with two talents dealing with the data processing to calculate the optimal velocity.
 * In addition to the Platform, a webserver is started on Port 8080, which serves the UI, which is able to send and receive (and react to)
 * messages from IoT Event Analytics.
 *
 */

const Logger = require('../../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

// Remove any given MQTT topic namespace from the environment variables
delete process.env.MQTT_TOPIC_NS;

const express = require('express');
const path = require('path');
const open = require('open');
const bodyParser = require('body-parser');

const { NamedMqttBroker } = require('../../../../../core/util/mqttBroker');
const ConfigManager = require('../../../../../core/configManager');
const Ingestion = require('../../../../../core/ingestion');
const Encoding = require('../../../../../core/encoding');
const Routing = require('../../../../../core/routing');
const Talent = require('../../../../../core/talent');
const {
    TalentInput
} = require('../../../../../core/util/talentIO');

const {
    AndRules,
    OrRules,
    Rule,
    OpConstraint
} = require('../../../../../core/rules');

const {
    VALUE_TYPE_RAW,
    INGESTION_TOPIC,
    TALENT_DISCOVERY_INTERVAL_MS,
    ENCODING_TYPE_NUMBER
} = require('../../../../../core/constants');

class VoptTalent extends Talent {
    constructor(connectionString) {
        super('vopt-calculation-talent', connectionString);

        this.addOutput('vopt', {
            description: 'Optimal speed in km/h',
            encoding: {
                type: ENCODING_TYPE_NUMBER,
                encoder: null
            }
        });
    }

    getRules() {
        return new AndRules([
            new OrRules([
                new Rule(new OpConstraint('m1', OpConstraint.OPS.GREATER_THAN, 0, 'Car', VALUE_TYPE_RAW)),
                new Rule(new OpConstraint('m2', OpConstraint.OPS.GREATER_THAN, 0, 'Car', VALUE_TYPE_RAW)),
                new Rule(new OpConstraint('m3', OpConstraint.OPS.GREATER_THAN, 0, 'Car', VALUE_TYPE_RAW))
            ]),
            new Rule(new OpConstraint('acc', OpConstraint.OPS.NEQUALS, 0, 'Car', VALUE_TYPE_RAW)),
            new Rule(new OpConstraint('vmin', OpConstraint.OPS.NEQUALS, 0, 'Car', VALUE_TYPE_RAW)),
            new Rule(new OpConstraint('vmax', OpConstraint.OPS.NEQUALS, 0, 'Car', VALUE_TYPE_RAW))
        ]);
    }

    async onEvent(ev) {
        const m1 = TalentInput.getRawValue(ev, 1, false, 'm1', 'Car', 'webapp');
        const m2 = TalentInput.getRawValue(ev, 1, false, 'm2', 'Car', 'webapp');
        const m3 = TalentInput.getRawValue(ev, 1, false, 'm3', 'Car', 'webapp');

        let sum = m1 > 0 ? m1 : m2 > 0 ? m2 : m3;

        sum += TalentInput.getRawValue(ev, 1, false, 'acc', 'Car', 'webapp');
        sum += TalentInput.getRawValue(ev, 1, false, 'vmin', 'Car', 'webapp');
        sum += TalentInput.getRawValue(ev, 1, false, 'vmax', 'Car', 'webapp');

        await this.broker.publish('ui/out/vopt', sum + '');
    }
}

const cf = new ConfigManager('mqtt://localhost:1883', '123456');
const ing = new Ingestion('mqtt://localhost:1883');
const enc = new Encoding('mqtt://localhost:1883');
const rou = new Routing('mqtt://localhost:1883', '123456');
const t1 = new VoptTalent('mqtt://localhost:1883');
const platformLogger = new Logger('Platform');
const broker = new NamedMqttBroker('VOptUIAdapter', 'mqtt://localhost:1883');
const app = express();

const values = {
    vopt: -1,
};

app.use(express.static(path.resolve( __dirname, './www' )));

const apiRouter = express.Router();
apiRouter.use(bodyParser.json());

apiRouter.post('/event', (req, res) => {
    broker.publishJson(INGESTION_TOPIC, req.body);
    res.json({});
});

apiRouter.get('/values/:name', (req, res) => {
    res.json({
        value: values[req.params.name]
    });
});

app.use('/api', apiRouter);

cf.start(TALENT_DISCOVERY_INTERVAL_MS, path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => t1.start())
    .then(() => broker.subscribe('ui/out/vopt', vopt => {
        values.vopt = parseFloat(vopt);
    }))
    .then(() => {
        return new Promise(resolve => {
            app.listen(8080, 'localhost', 0, () => {
                platformLogger.info('VOpt Backend started successfully on port 8080');
                resolve(open('http://localhost:8080'));
            });
        });
    })
    .catch(err => {
        platformLogger.error(err.message, null, err);
    });