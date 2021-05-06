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

const MetadataManager = require('./metadataManager');
const Channels = require('./channels');
const ProtocolGateway = require('./protocolGateway');

const validatePlatformId = require('./util/validatePlatformId');

const {
    INGESTION_TOPIC,
    ENCODING_TOPIC,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE,
    DEFAULT_SEGMENT,
    ENCODING_TYPE_ANY
} = require('./constants');

module.exports = class Ingestion {
    constructor(protocolGatewayConfig, platformId) {
        validatePlatformId(platformId);
        this.platformId = platformId;
        this.logger = new Logger('Ingestion');
        this.pg = new ProtocolGateway(protocolGatewayConfig, this.logger.name);
        this.metadataManager = new MetadataManager(protocolGatewayConfig, new Logger(`${this.logger.name}.MetadataManager`));
    }

    start(channelsConfigDir) {
        return Channels.load(channelsConfigDir)
            .then(channels => {
                this.channels = channels;
            })
            .then(() => this.metadataManager.start())
            // Subscribe for all messages, which are published from the same host as the platform is running on
            .then(() => this.pg.subscribeJsonShared('global-ingestion', INGESTION_TOPIC, this.__onEvent.bind(this)))
            // Subscribe for all events, which are sent from talents using the returnTopic
            .then(() => this.pg.subscribeJsonShared('platform-ingestion', `${this.platformId}/${INGESTION_TOPIC}`, this.__onEvent.bind(this)))
            .then(() => {
                this.logger.info('Ingestion started successfully');
            });
    }

    async __onEvent(ev) {
        if (Array.isArray(ev)) {
            for (const singleEvent of ev) {
                await this.__handleEvent(singleEvent)
            }
        } else {
            await this.__handleEvent(ev);
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

                    await this.pg.publishJson(ENCODING_TOPIC, ev, ProtocolGateway.createPublishOptions(true));
                }
                catch(err) {
                    // - Metadata could not be found for the processed Event --> skip event
                    // - Value type is incorrect
                    // - Event could not be forwarded to message bus
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