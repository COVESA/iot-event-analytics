//##############################################################################
// Copyright (c) 2021 Bosch.IO GmbH
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0
//##############################################################################

const iotea = require('boschio.iotea');

const {
    TestRunnerTalent,
    ProtocolGateway
} = iotea;

const {
    Logger,
    MqttProtocolAdapter
} = iotea.util;

process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

class TestRunner extends TestRunnerTalent {
    constructor(protocolGatewayConfig) {
        // Define your testSetTalent list and set via super constructor
        super('testRunner-all', ['testSet-sdk-js', 'testSet-sdk-py', 'testSet-sdk-cpp'], protocolGatewayConfig);
    }
}

const runner = new TestRunner(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(false,"mqtt://mosquitto:1883") ]));

runner.start();
