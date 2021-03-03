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
 * Starts a talent, which outputs the computed value it received from the cloud talent
 * Check your trigger rule on AWS, that it matches the cloud talent id "my-new-cloud-talent"
 *
 */

const Logger = require('../../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

// Remove any given MQTT topic namespace from the environment variables
delete process.env.MQTT_TOPIC_NS;

const Talent = require('../../../../../core/talent');

const {
    AndRules,
    Rule,
    OpConstraint
} = require('../../../../../core/rules');

const {
    VALUE_TYPE_RAW,
    DEFAULT_TYPE
} = require('../../../../../core/constants');

class CloudOutputTalent extends Talent {
    constructor(connectionString) {
        super('local-cloud-output', connectionString);
    }

    getRules() {
        const rules = new AndRules([
            new Rule(
                new OpConstraint('my-new-cloud-talent.fancyCloudValue', OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            )
        ]);

        return rules;
    }

    onEvent(ev, evtctx) {
        this.logger.info(JSON.stringify(ev), evtctx);
    }
}

(new CloudOutputTalent('mqtt://localhost:1883')).start();