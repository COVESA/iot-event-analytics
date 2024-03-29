//##############################################################################
// Copyright (c) 2021 Bosch.IO GmbH
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0
//##############################################################################
// uncomment for local developer setup
// const iotea = require('../../../../src/module.js');
const iotea = require('boschio.iotea');

const {
    FunctionTalent
} = iotea;

const {
    Logger,
    JsonModel
} = iotea.util;

const config = new JsonModel(require('../../config/tests/javascript/config.json'));
process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.INFO);

class FunctionProvider extends FunctionTalent {
    constructor(protocolGatewayConfig) {
        super('function-provider-js', protocolGatewayConfig);

        // Register Functions
        this.registerFunction('echo', this.echo.bind(this));
    }

    async echo(value, ev, evtctx, timeoutAtMs) {
        this.logger.debug('Echo called');
        return value;
    }
}

const pgConfig = config.get("protocolGateway")
const fp = new FunctionProvider(pgConfig);

fp.start();
