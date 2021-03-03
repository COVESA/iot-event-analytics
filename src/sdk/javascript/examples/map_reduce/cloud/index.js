/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('../../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

process.env.MQTT_TOPIC_NS = 'iotea/';

const { Mapper, Reducer } = require('../../../../../core/talent.mr');

const {
    Rule,
    OpConstraint
} = require('../../../../../core/rules');

const {
    VALUE_TYPE_RAW
} = require('../../../../../core/constants');

class RandomMapper extends Mapper {
    constructor(connectionString) {
        super('local-mapper', 'reducer', connectionString);
    }

    getTriggerRules() {
        return new Rule(new OpConstraint('trigger', OpConstraint.OPS.ISSET, null, 'testdevice', VALUE_TYPE_RAW));
    }

    map() {
        return Promise.resolve(new Array(9 + Math.floor(Math.random() * 2)).fill(null).map(() => Math.random()));
    }
}

class RandomReducer extends Reducer {
    constructor(connectionString) {
        super('reducer', 'local-mapper', connectionString);
    }

    async reduce(data) {
        this.logger.info(`Reducer calculated sum ${data.reduce((a, v) => a + v, 0)}`, this.evtctx);
    }
}

const mrLogger = new Logger('MapReduce');
const mapper = new RandomMapper('mqtt://localhost:1883');
const reducer = new RandomReducer('mqtt://localhost:1883');

mapper.start()
    .then(() => reducer.start())
    .catch(err => {
        mrLogger.error(err.message, null, err);
    });