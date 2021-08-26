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

const config = new JsonModel(require('../../config/tests/javascript/runner/config.json'));
process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.INFO);

const runner = new TestRunnerTalent('testRunner-js', config);

runner.start();
