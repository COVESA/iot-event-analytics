/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const MetadataManagerMock = require('./metadataManager.mock');
const ProtocolGateway = require('../../../src/core/protocolGateway');
const InstanceManager = require('../../../src/core/instanceManager');
const { DEFAULT_TYPE } = require('../../../src/core/constants');
const { MqttProtocolAdapter } = require('../../../src/core/util/mqttClient');
const MqttClientMock = require('./mqtt.mock');

require('mock-require')('mqtt', {
    connect: () => new MqttClientMock()
});

module.exports = class InstanceManagerMock extends InstanceManager {
    constructor(typesConfig) {
        super(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(true, '') ]));
        this.metadataManager = new MetadataManagerMock(typesConfig);
    }

    start() {
        return this.metadataManager.start();
    }

    updateFeature(subject, instanceId, feature, encodedValue, rawValue, whenMs, type = DEFAULT_TYPE) {
        return super.updateFeature(subject, instanceId, feature, encodedValue, rawValue, whenMs, type, false, false);
    }
};