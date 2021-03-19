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
 * Starts a talent, which is connected to a protocol adapter other than the platform
 *
 */

const Logger = require('../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

const Talent = require('../../../../core/talent');

const { TalentInput } = require('../../../../core/util/talentIO');

const {
    AndRules,
    Rule,
    OpConstraint
} = require('../../../../core/rules');

const {
    VALUE_TYPE_RAW
} = require('../../../../core/constants');

const ProtocolGateway = require('../../../../core/protocolGateway');

const {
    MqttProtocolAdapter
} = require('../../../../core/util/mqttClient');

class CloudOutputTalent extends Talent {
    constructor(protocolGatewayConfig) {
        super('other-protocol-adapter-talent', protocolGatewayConfig);
    }

    getRules() {
        const rules = new AndRules([
            new Rule(
                new OpConstraint('temp', OpConstraint.OPS.ISSET, null, 'kuehlschrank', VALUE_TYPE_RAW)
            )
        ]);

        return rules;
    }

    onEvent(ev, evtctx) {
        this.logger.info(`${TalentInput.getRawValue(ev)}`, evtctx);
    }
}

const protocolGatewayConfig = ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(false, 'mqtt://localhost:1884')]);

(new CloudOutputTalent(protocolGatewayConfig)).start();