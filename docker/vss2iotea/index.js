/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const uuid = require('uuid').v4;

const {
    VssAdapter,
    VssPathTranslator
} = require('../../src/adapter/vss/vss.adapter');
const JsonModel = require('../../src/core/util/jsonModel');

const Logger = require('../../src/core/util/logger');

const config = new JsonModel(require('./config/config.json'));

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.WARN);

let vss2ioteaLogger = new Logger(`Vss2IoTEventAnalytics`);

const vssAdapter = new VssAdapter(
    config.get('protocolGateway'),
    config.get('vss.ws'),
    config.get('vss.jwt'),
    config.get('segment'),
    uuid(),
    {
        rejectUnauthorized: false
    },
    true
);

let vssPathConfig = config.get('vss.pathConfig', null);

if (vssPathConfig !== null) {
    vssAdapter.setVssPathTranslator(new VssPathTranslator(vssPathConfig));
}

vss2ioteaLogger.info('Current VSS Adapter Configuration');
vss2ioteaLogger.info(JSON.stringify(config));

const instanceIdPath = config.get('paths.instanceId');
const userIdPath = config.get('paths.userId');

vssAdapter.start(instanceIdPath, userIdPath)
    .then(() => {
        vss2ioteaLogger.info(`VSS Adapter started successfully`);
    })
    .catch(err => {
        vss2ioteaLogger.error(err.message, null, err);
    });