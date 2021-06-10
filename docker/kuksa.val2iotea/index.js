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
    KuksaValAdapter,
    VssPathTranslator
} = require('../../src/adapter/kuksa.val/kuksa.val.adapter');
const JsonModel = require('../../src/core/util/jsonModel');

const Logger = require('../../src/core/util/logger');

const config = new JsonModel(require('./config/config.json'));

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.WARN);

let kuksaVal2ioteaLogger = new Logger(`KuksaVal2IoTEventAnalytics`);

const kuksaValAdapter = new KuksaValAdapter(
    config.get('protocolGateway'),
    config.get("'kuksa.val'.ws"),
    config.get("'kuksa.val'.jwt"),
    config.get('segment'),
    uuid(),
    {
        rejectUnauthorized: false
    },
    true
);

let vssPathConfig = config.get("'kuksa.val'.pathConfig", null);

if (vssPathConfig !== null) {
    kuksaValAdapter.setVssPathTranslator(new VssPathTranslator(vssPathConfig));
}

kuksaVal2ioteaLogger.info('Current Kuksa.val Adapter Configuration');
kuksaVal2ioteaLogger.info(JSON.stringify(config));

const instanceIdPath = config.get('paths.instanceId');
const userIdPath = config.get('paths.userId');

kuksaValAdapter.start(instanceIdPath, userIdPath)
    .then(() => {
        kuksaVal2ioteaLogger.info(`Kuksa.val Adapter started successfully`);
    })
    .catch(err => {
        kuksaVal2ioteaLogger.error(err.message, null, err);
    });