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
    TestRunnerTalent
} = iotea;

const {
    Logger,
    JsonModel
} = iotea.util;

const config = new JsonModel(require('./config/tests/javascript/config.json'));
process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.INFO);

class TestRunner extends TestRunnerTalent {
    constructor(protocolGatewayConfig) {
        // Define your testSetTalent list and set via super constructor
        super('testRunner-js', ['testSet-sdk-js', 'testSet-sdk-py'], protocolGatewayConfig);
        //super('testRunner-js', ['testSet-sdk-js', 'testSet-sdk-py', 'testSet-sdk-cpp'], protocolGatewayConfig);

        // you can run singular tests say for development
        //super('testRunner-js', ['testSet-sdk-py'], config.get('protocolGateway'));
    }
}

// reads config for all integration-test
const pgConfig = config.get("protocolGateway")
const runner = new TestRunner(pgConfig);

runner.start();
