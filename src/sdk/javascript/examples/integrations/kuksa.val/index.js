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

process.env.MQTT_TOPIC_NS = 'iotea/';

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
    VssInputValue,
    VssOutputValue
} = require('../../../../../adapter/vss/vss.talentIO');

const VssWebsocket = require('../../../../../adapter/vss/vss.websocket');

const demoLogger = new Logger('KuksaValDemo');

class VssWorker extends Talent {
    constructor(connectionString) {
        super('vss-worker-talent', connectionString);
    }

    getRules() {
        return new AndRules([
            new Rule(new OpConstraint('ADAS$ObstacleDetection$IsActive', OpConstraint.OPS.EQUALS, true, 'Vehicle', VALUE_TYPE_RAW))
        ]);
    }

    async onEvent(ev) {
        const rawValue = TalentInput.getRawValue(ev, 1, false, ev.feature, ev.type, ev.instance, true);

        this.logger.info(`Talent received value ${JSON.stringify(rawValue)} from Kuksa.VAL adapter`);

        const to = new TalentOutput();

        to.addFor(ev.subject, ev.type, ev.instance, 'ADAS$ObstacleDetection$Error', VssOutputValue.create(this, VssInputValue.getSubscription(rawValue), false))

        return to.toJson();
    }
}

const config = new JsonModel(require('./config.json'));

const vssws = new VssWebsocket(config.get('vss.ws'), config.get('vss.jwt'));

let odIsActive = false;

async function publishObstacleDetectionToVssIndefinitly() {
    demoLogger.info(`Publishing ${odIsActive} to Vehicle.ADAS.ObstacleDetection.IsActive...`);

    await vssws.publish('Vehicle.ADAS.ObstacleDetection.IsActive', odIsActive);

    odIsActive = !odIsActive;

    setTimeout(() => {
        publishObstacleDetectionToVssIndefinitly();
    }, 2500);
}

new VssWorker('mqtt://localhost:1883').start()
    .then(() => vssws.subscribe('Vehicle.ADAS.ObstacleDetection.Error', msg => {
        demoLogger.info(`Received ${msg.value} from ${msg.path}`);
    }))
    .then(() => {
        publishObstacleDetectionToVssIndefinitly();
    });