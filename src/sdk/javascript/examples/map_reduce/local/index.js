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
 * Starts the IoTea Platform to demonstrate the Map-Reduce capabilities.
 *
 */

const Logger = require('../../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

process.env.MQTT_TOPIC_NS = 'iotea/';

const path = require('path');

const ConfigManager = require('../../../../../core/configManager');
const Ingestion = require('../../../../../core/ingestion');
const Encoding = require('../../../../../core/encoding');
const Routing = require('../../../../../core/routing');
const { Mapper, Worker, Reducer } = require('../../../../../core/talent.mr');

const {
    Rule,
    OpConstraint
} = require('../../../../../core/rules');

const {
    VALUE_TYPE_RAW,
    TALENT_DISCOVERY_INTERVAL_MS
} = require('../../../../../core/constants');

class RandomMapper extends Mapper {
    constructor(connectionString) {
        super('mapper', 'reducer', connectionString);
    }

    getTriggerRules() {
        return new Rule(new OpConstraint('trigger', OpConstraint.OPS.ISSET, null, 'testdevice', VALUE_TYPE_RAW));
    }

    map() {
        const workPackages = new Array(1 + Math.floor(Math.random() * 9)).fill(null).map(() => Math.random());

        this.logger.info(`Created work packages ${JSON.stringify(workPackages)}`);

        return Promise.resolve(workPackages);
    }
}

class RandomWorker extends Worker {
    constructor(connectionString) {
        super('worker', 'mapper', connectionString);
    }

    work(data) {
        this.logger.info(`Computing result for work package ${data}...`);

        return new Promise((resolve, reject) => {
            setTimeout(() => {
                if (Math.random() > 0.95) {
                    reject(new Error(`Error computing result`));
                    return;
                }
                resolve(data * 10);
            }, Math.floor(Math.random() * 2000));
        });
    }
}

class RandomReducer extends Reducer {
    constructor(connectionString) {
        super('reducer', 'mapper', connectionString);
    }

    async reduce(data) {
        this.logger.info(`Reducer calculated sum ${data.reduce((a, v) => (isNaN(parseFloat(v)) ? 0 : v) + a, 0)}`, this.evtctx);
    }
}

const cf = new ConfigManager('mqtt://localhost:1883', '123456');
const ing = new Ingestion('mqtt://localhost:1883');
const enc = new Encoding('mqtt://localhost:1883');
const rou = new Routing('mqtt://localhost:1883', '123456');
const mapper = new RandomMapper('mqtt://localhost:1883');
const w1 = new RandomWorker('mqtt://localhost:1883');
const w2 = new RandomWorker('mqtt://localhost:1883');
const reducer = new RandomReducer('mqtt://localhost:1883');
const platformLogger = new Logger('Platform');

cf.start(TALENT_DISCOVERY_INTERVAL_MS, path.resolve(__dirname, 'config', 'types.json'), path.resolve(__dirname, 'config', 'uom.json'))
    .then(() => ing.start(path.resolve(__dirname, 'config', 'channels')))
    .then(() => enc.start())
    .then(() => rou.start())
    .then(() => mapper.start())
    .then(() => w1.start())
    .then(() => w2.start())
    .then(() => reducer.start())
    .catch(err => {
        platformLogger.error(err.message, null, err);
    });