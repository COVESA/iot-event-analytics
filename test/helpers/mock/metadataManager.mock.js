/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const MetadataManager = require('../../../src/core/metadataManager');
const ProtocolGateway = require('../../../src/core/protocolGateway');
const { MqttProtocolAdapter } = require('../../../src/core/util/mqttClient');

const MqttClientMock = require('./mqtt.mock');

require('mock-require')('mqtt', {
    connect: () => new MqttClientMock()
});

module.exports = class MetadataManagerMock extends MetadataManager {
    constructor(typesConfig) {
        super(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(true, '') ]));
        this.ready = Promise.resolve();
        this.typesConfig = typesConfig;
    }

    registerFeature(segment, feature, metadata, type) {
        this.__registerFeature(segment, feature, metadata, type)
    }

    start() {
        return this.__initFromConfig(this.typesConfig);
    }
};