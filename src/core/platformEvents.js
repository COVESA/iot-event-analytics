/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('./util/logger');
const { MqttBroker } = require('./util/mqttBroker');
const {
    PLATFORM_EVENTS_TOPIC
} = require('./constants');

class PlatformEventEmitter extends MqttBroker {
    constructor(connectionString) {
        super(connectionString);
        this.logger = new Logger('PlatformEventEmitter');
    }

    send(type, data) {
        const ev = {
            type,
            data,
            timestamp: Date.now()
        };

        this.logger.verbose(`Sending platform event ${JSON.stringify(ev)}...`);

        return this.publishJson([ PLATFORM_EVENTS_TOPIC ], ev).catch(err => { this.logger.warn(err.message, null, err); });
    }
}

class PlatformEvents {}

PlatformEvents.__instance = null;
PlatformEvents.__onReadyResolve = null;
PlatformEvents.ready = new Promise(resolve => {
    PlatformEvents.__onReadyResolve = resolve;
});

PlatformEvents.init = function(connectionString) {
    PlatformEvents.__instance = new PlatformEventEmitter(connectionString);
    PlatformEvents.__onReadyResolve(PlatformEvents.__instance);
};

PlatformEvents.fire = function fire(type, data) {
    return PlatformEvents.ready.then(platformEventEmitter => platformEventEmitter.send(type, data));
};

module.exports = PlatformEvents;