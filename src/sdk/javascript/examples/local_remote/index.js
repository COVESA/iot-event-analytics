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
 * Simulates a remote Talent (running locally), since it talks with the remote Message broker.
 * Works in conjunction with IoTEA platform started by docker-compose.yml and the mqtt tool from _tools/mqtt/cli.js_
 *
 */

const Logger = require('../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

process.env.MQTT_TOPIC_NS = 'iotea/';

const Talent = require('../../../../core/talent');

const {
    AndRules,
    Rule,
    OpConstraint
} = require('../../../../core/rules');

const {
    VALUE_TYPE_RAW
} = require('../../../../core/constants');

class RemoteTalent extends Talent {
    constructor(connectionString) {
        super('js-remote-talent', connectionString);
    }

    getRules() {
        const rules = new AndRules([
            new Rule(
                new OpConstraint('temp', OpConstraint.OPS.ISSET, null, 'kuehlschrank', VALUE_TYPE_RAW)
            )
        ]);

        return rules;
    }

    async onEvent(ev, evtctx) {
        this.logger.info(JSON.stringify(ev), evtctx);
    }
}

new RemoteTalent('mqtt://localhost:1884').start();