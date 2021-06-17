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
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.DEBUG;

const Talent = require('../../../../../core/talent');

const JsonModel = require('../../../../../core/util/jsonModel');

const {
    AndRules,
    Rule,
    OpConstraint
} = require('../../../../../core/rules');

const {
    VALUE_TYPE_RAW
} = require('../../../../../core/constants');

const {
    TalentOutput,
    TalentInput,
} = require('../../../../../core/util/talentIO');

const {
    KuksaValInputValue,
    KuksaValOutputValue
} = require('../../../../../adapter/kuksa.val/kuksa.val.talentIO');

const KuksaValWebsocket = require('../../../../../adapter/kuksa.val/kuksa.val.websocket');

const ProtocolGateway = require('../../../../../core/protocolGateway');
const {
    MqttProtocolAdapter
} = require('../../../../../core/util/mqttClient');

const demoLogger = new Logger('KuksaValDemo');

class VssWorker extends Talent {
    constructor(protocolGatewayConfig) {
        super('vss-worker-talent', protocolGatewayConfig);
    }

    getRules() {
        return new AndRules([
            new Rule(new OpConstraint('Acceleration$Longitudinal', OpConstraint.OPS.ISSET, null, 'Vehicle', VALUE_TYPE_RAW))
        ]);
    }

    async onEvent(ev) {
        const rawValue = TalentInput.getRawValue(ev, 1, false, ev.feature, ev.type, ev.instance, true);

        this.logger.info(`Talent received value ${JSON.stringify(rawValue)} from Kuksa.val adapter`);

        const to = new TalentOutput();

        to.addFor(ev.subject, ev.type, ev.instance, 'Acceleration$Lateral', KuksaValOutputValue.create(this, KuksaValInputValue.getSubscription(rawValue), rawValue.value));

        return to.toJson();
    }
}

const config = new JsonModel(require('./config.json'));

const kuksaValWs = new KuksaValWebsocket(config.get("'kuksa.val'.ws"), config.get("'kuksa.val'.jwt"));

let accLon = 0;

async function publishAccLonToKuksaValIndefinitly() {
    demoLogger.info(`Publishing ${accLon} to Vehicle.Acceleration.Longitudinal...`);

    await kuksaValWs.publish('Vehicle.Acceleration.Longitudinal', accLon++);

    setTimeout(() => {
        publishAccLonToKuksaValIndefinitly();
    }, 2500);
}

const mqttAdapterConfig = MqttProtocolAdapter.createDefaultConfiguration();
const talentGatewayConfig = ProtocolGateway.createDefaultConfiguration([ mqttAdapterConfig ]);

new VssWorker(talentGatewayConfig).start()
    .then(() => kuksaValWs.subscribe('Vehicle.Acceleration.Lateral', msg => {
        demoLogger.info(`Received ${msg.value} from ${msg.path}`);
    }))
    .then(() => {
        publishAccLonToKuksaValIndefinitly();
    });