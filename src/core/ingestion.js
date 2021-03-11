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

const Logger = require('./util/logger');

const jsonQuery = require('./util/jsonQuery');

const { NamedMqttBroker } = require('./util/mqttBroker');
const MetadataManager = require('./metadataManager');
const Channels = require('./channels');

const {
    INGESTION_TOPIC,
    ENCODING_TOPIC,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE,
    DEFAULT_SEGMENT,
    ENCODING_TYPE_ANY
} = require('./constants');

module.exports = class Ingestion {
    constructor(connectionString) {
        this.logger = new Logger('Ingestion');
        this.broker = new NamedMqttBroker('Ingestion', connectionString);
        this.metadataManager = new MetadataManager(connectionString, new Logger(`${this.logger.name}.MetadataManager`));
    }

    start(channelsConfigDir) {
        return Channels.load(channelsConfigDir)
            .then(channels => {
                this.channels = channels;
            })
            .then(() => this.metadataManager.start())
            // Events which can be processed by any client --> automatically load-balanced event subscription via shared subscriptions (MQTT5)
            .then(() => this.broker.subscribeJson(`$share/ingestion/${INGESTION_TOPIC}`, this.__onEvent.bind(this)))
            .then(() => {
                this.logger.info('Ingestion started successfully');
            });
    }

    async __onEvent(event) {
        if (Array.isArray(event)) {
            for (const singleEvent of event) {
                await this.__handleEvent(singleEvent)
            }
        } else {
            await this.__handleEvent(event);
        }
    }

    async __handleEvent(ev) {
        this.logger.verbose(`Received raw event ${JSON.stringify(ev)}`);

        for (const channel of this.channels) {
            try {
                ev = await channel.handle(ev);

                try {
                    if (ev.type !== DEFAULT_TYPE && ev.instance === DEFAULT_INSTANCE) {
                        this.logger.verbose(`A specific type needs a specific instance`);
                        continue;
                    }

                    const metaFeature = await this.metadataManager.resolveMetaFeature(ev.type, ev.feature);

                    this.__assertValueType(ev.value, metaFeature);

                    // Assign metadata
                    ev.$metadata = metaFeature;

                    if (!ev.segment) {
                        // Add segment information
                        ev.segment = ev.type === DEFAULT_TYPE ? DEFAULT_SEGMENT : await this.metadataManager.resolveSegment(ev.type);
                    }

                    if (ev.cid === undefined) {
                        ev.cid = uuid();
                    }

                    const evtctx = Logger.createEventContext(ev);

                    this.logger.verbose(`Forwarding event to encoding stage ${JSON.stringify(ev)}`, evtctx);

                    await this.broker.publishJson(ENCODING_TOPIC, ev);
                }
                catch(err) {
                    // - Metadata could not be found for the processed Event --> skip event
                    // - Value type is incorrect
                    // - Event could not be forwareded to message bus
                    this.logger.debug(err.message);
                }

                return;
            }
            catch(err) {
                // Event cannot be handled by the given channel
            }
        }
    }

    __assertValueType(value, metaFeature) {
        if (metaFeature.encoding.type === ENCODING_TYPE_ANY) {
            return true;
        }

        if (typeof value === 'object' && value.$vpath) {
            value = jsonQuery.first(value, value.$vpath).value;
        }

        if (typeof value !== metaFeature.encoding.type) {
            throw new Error(`Value type ${typeof value} does not match ${metaFeature.encoding.type}`);
        }
    }
};