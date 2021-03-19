/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const path = require('path');

const Logger = require('../../../../core/util/logger');
const JsonModel = require('../../../../core/util/jsonModel');

const config = new JsonModel(require('./config.json'));

process.env.LOG_LEVEL = config.get('ingestion.loglevel', config.get('loglevel', Logger.ENV_LOG_LEVEL.INFO));

const Ingestion = require('../../../../core/ingestion');

(async () => {
    const config = new JsonModel(require('./config.json'));
    await new Ingestion(config.get('protocolGateway'), config.get('platformId')).start(path.resolve(config.get('platformConfigDir'), 'channels'));
})();