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
 * Talent, which can be converted to a cloud template using t2c.js
 * i.e.
 * node t2c.js -f cloudTalent.js -n CloudTalent -a "mqtt://localhost:1884:string"
 *
 */

const Talent = require('../../../core/talent');
const { Worker } = require('../../../core/talent.mr');

const {
    AndRules,
    Rule,
    OpConstraint,
    ALL_TYPES
} = require('../../../core/rules');

const {
    VALUE_TYPE_RAW,
    ENCODING_TYPE_NUMBER
} = require('../../../core/constants');

class CloudTalent extends Talent {
    constructor(connectionString) {
        super('my-new-cloud-talent', connectionString);

        this.addOutput('fancyCloudValue', {
            description: 'This value is magically calculated in the cloud',
            history: 10,
            encoding: {
                type: ENCODING_TYPE_NUMBER,
                encoder: null
            },
            unit: 'µM'
        });
    }

    getRules() {
        const rules = new AndRules([
            new Rule(
                new OpConstraint('temp2', OpConstraint.OPS.EQUALS, 10, `100000.${ALL_TYPES}`, VALUE_TYPE_RAW)
            )
        ]);

        return rules;
    }

    onEvent(ev, evtctx) {
        return [{
            feature: this.id + '.fancyCloudValue',
            value: 42 + Math.random(),
            subject: ev.subject,
            whenMs: Date.now()
        }];
    }
}

class CloudWorkerTalent extends Worker {
    constructor(mapperId, connectionString) {
        super('cloud-talent-worker', mapperId, connectionString);
    }
}

module.exports = {
    CloudTalent,
    CloudWorkerTalent
};