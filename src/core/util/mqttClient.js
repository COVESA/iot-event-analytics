/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const uuid = require('uuid');
const mqtt = require('mqtt');

const Logger = require('./logger');
const JsonModel = require('./jsonModel');

class MqttProtocolAdapter {
    constructor(config, displayName = null) {
        this.client = null;

        this.config = new JsonModel(config);

        this.brokerUrl = this.config.get('brokerUrl');

        this.topicNs = this.config.get('topicNamespace', null);

        this.checkMqtt5Compatibility = this.config.get('mqtt5Only', true) !== false;

        if (displayName === null) {
            this.client = new MqttClient(
                this.brokerUrl,
                this.topicNs,
                this.checkMqtt5Compatibility
            );
        } else {
            this.client = new NamedMqttClient(
                displayName,
                this.brokerUrl,
                this.topicNs,
                this.checkMqtt5Compatibility
            );
        }
    }

    publish(topic, message, publishOptions) {
        const options = {
            retain: publishOptions.retain === true
        };

        return this.client.publish([ this.__prefixTopicNs(topic) ], message, options);
    }

    subscribe(topic, callback) {
        return this.client.subscribe(this.__prefixTopicNs(topic), callback);
    }

    subscribeShared(group, topic, callback) {
        if (!this.checkMqtt5Compatibility) {
            return Promise.reject(new Error(`Shared subscriptions only possible, if MQTT5 compatibility is checked. Set "mqtt5Only" to true in your protocol configuration`));
        }

        return this.client.subscribe(`$share/${group}/${this.__prefixTopicNs(topic)}`, callback);
    }

    getId() {
        return this.brokerUrl;
    }

    __prefixTopicNs(topic) {
        if (this.topicNs === null) {
            return topic;
        }

        return topic.replace(new RegExp(`^(\\$share\\/[^\\/]+\\/)?(?:${this.topicNs})?(.+)$`), `$1${this.topicNs}$2`);
    }
}

MqttProtocolAdapter.createDefaultConfiguration = function createDefaultConfiguration(isPlatformProtocol = false, brokerUrl = 'mqtt://localhost:1883') {
    return {
        platform: isPlatformProtocol,
        module: {
            // From perspective of protocolGateway.js, since require(<module.name>) is called from there
            name: './util/mqttClient',
            class: 'MqttProtocolAdapter'
        },
        config: {
            brokerUrl,
            topicNamespace: 'iotea/',
            mqtt5Only: true
        }
    };
};

class MqttClient {
    constructor(brokerUrl, topicNs = null, checkMqtt5Compatibility = true, logger = null, clientId = MqttClient.createClientId('MqttClient')) {
        if (logger === null) {
            logger = new Logger(clientId)
        }

        this.logger = logger;

        this.clientId = clientId;

        this.topicNs = null;

        if (typeof topicNs === 'string') {
            // Check if topicNs ends with a slash
            // eslint-disable-next-line no-useless-escape
            if ((/^[\/\w]+\/$/m).test(topicNs) === false) {
                throw new Error(`Given topic namespace ${topicNs} is invalid. It has to have a trailing slash`);
            }

            this.topicNs = topicNs;

            this.logger.always(`*****INFO***** Using topic namespace ${this.topicNs}`);
        } else {
            this.logger.always('*****WARNING***** No topic namespace given. Tip: Also check all topics of your subscriptions and publications');
        }

        this.__client = null;

        this.brokerUrl = brokerUrl;
        this.clientPromise = null;

        this.checkMqtt5Compatibility = checkMqtt5Compatibility;
        this.isMqtt5Compatible = false;
    }

    publish(topics, message, options = {}) {
        if (this.clientPromise && this.clientPromise.done === false) {
            // MQTT client is offline. Do not wait until it is online again.
            // If we just try to publish it anyway, we end up with a huge stack of unsent messages, which will be published
            // when the client is reconnected again
            this.logger.warn(`MQTT Broker ${this.brokerUrl} is offline. Cannot publish message to topics ${topics}`);
            return;
        }

        if (!Array.isArray(topics)) {
            topics = [ topics ];
        }

        return Promise.all(topics.map(topic => this.__publish(topic, message, options)));
    }

    publishJson(topics, json, options = {}) {
        // Check if it's actually json, which should be sent
        try {
            this.__validateJson(json);
            return this.publish(topics, JSON.stringify(json), options);
        }
        catch(err) {
            return Promise.reject(err);
        }
    }

    subscribe(topic, cb) {
        return this.__getClient()
            .then(client => {
                return new Promise((resolve, reject) => {
                    // Only works if client is connected
                    try {
                        const prefixedTopic = this.__prefixTopicNs(topic);

                        this.logger.debug(`Subscribing to topic ${prefixedTopic}...`)

                        client.subscribe(prefixedTopic, {}, (err) => {
                            if (err) {
                                reject(err);
                                return;
                            }

                            const regex = this.__prepareRegexForTopic(topic);

                            client.on('message', (topic, buffer) => {
                                regex.lastIndex = 0;

                                if (!regex.test(topic)) {
                                    return;
                                }

                                cb(buffer.toString('utf8'), topic);
                            });

                            this.logger.verbose(`Successfully subscribed to topic ${prefixedTopic}`);

                            resolve(client);
                        });
                    }
                    catch(err) {
                        reject(err);
                    }
                });
            });
    }

    subscribeJson(topic, cb) {
        return this.subscribe(topic, (msgString, topic) => {
            try {
                const json = JSON.parse(msgString);

                this.__validateJson(json);

                cb(json, topic);
            }
            catch(err) {
                this.logger.warn(err.message, null, err);
            }
        });
    }

    unsubscribe(topics) {
        return this.__getClient()
            .then(client => {
                return new Promise((resolve, reject) => {
                    try {
                        client.unsubscribe(topics.map(topic => this.__prefixTopicNs(topic)), {}, (err) => {
                            if (err) {
                                reject(err);
                                return;
                            }

                            resolve(client);
                        });
                    }
                    catch(err) {
                        reject(err);
                    }
                });
            });
    }

    disconnect(force = true) {
        return this.__getClient()
            .then(client => {
                return new Promise((resolve, reject) => {
                    try {
                        client.end(force, {}, resolve);
                    }
                    catch(err) {
                        reject(err);
                    }
                });
            })
            .then(() => {
                this.clientPromise = null;
            });
    }

    __publish(topic, message, options = {}) {
        return this.__getClient()
            .then(client => {
                return new Promise((resolve, reject) => {
                    try {
                        topic = this.__prefixTopicNs(topic);

                        this.logger.verbose(`Publishing ${message} to ${topic} @ ${this.brokerUrl}`);

                        client.publish(topic, message, options, (err, packet) => {
                            if (err) {
                                reject(err);
                                return;
                            }

                            resolve(packet);
                        });
                    }
                    catch(err) {
                        reject(err);
                    }
                });
            });
    }

    async __getClient() {
        if (this.clientPromise !== null) {
            return this.clientPromise;
        }

        // Client will reconnect and resubscribe automatically
        this.__client = mqtt.connect(this.brokerUrl, { clientId: this.clientId, clean: true, resubscribe: true, reconnectPeriod: 1000 });

        this.clientPromise = Promise.resolve(this.__client)
            .then(async client => {
                if (client.connected) {
                    return client;
                }

                this.logger.debug('Waiting for MQTT connection...');

                return new Promise(resolve => {
                    const onConnect = () => {
                        client.off('connect', onConnect);
                        this.logger.debug('MQTT connection sucessfully established');
                        resolve(client);
                    };

                    client.on('connect', onConnect);
                });
            })
            .then(client => {
                if (!this.checkMqtt5Compatibility) {
                    // No compatibility check required or compatibility already checked successfully
                    return client;
                }

                this.isMqtt5Compatible = false;

                return this.__mqtt5Probe(client, parseInt(process.env.MQTT5_PROBE_TIMEOUT, 10) || 1000)
                    .then(client => {
                        this.isMqtt5Compatible = true;
                        return client;
                    });
            });

        this.__client.on('close', () => {
            this.clientPromise.done = false;
        });

        this.__client.on('connect', () => {
            this.clientPromise.done = true;
        });

        this.clientPromise.done = false;

        return this.clientPromise;
    }

    __prefixTopicNs(topic) {
        return MqttClient.prefixTopicNs(topic, this.topicNs);
    }

    __prepareRegexForTopic(topic) {
        // Remove meta topic prefixes, since they are not transferred by the message
        // - Shared subscriptions $share/<group>/
        // eslint-disable-next-line no-useless-escape
        topic = topic.replace(/^\$share\/[^\/]+\//, '');

        // Escape / and remaining $
        // Convert + to 1-level wildcard
        // Convert # to multilevel wildcard
        return new RegExp(`^${this.__prefixTopicNs(topic).replace(/\$/g, '\\$').replace(/\//g, '\\/').replace(/\+/g, '[^\\/]+').replace(/#/, '.+')}$`, 'g');
    }

    __mqtt5Probe(client, timeoutMs) {
        const publishTo = `probe/${uuid.v4()}`;
        const subscribeTo = `$share/${uuid.v4()}/${publishTo}`;

        return new Promise((resolve, reject) => {
            const regex = this.__prepareRegexForTopic(subscribeTo);

            const onMessage = topic => {
                regex.lastIndex = 0;

                if (!regex.test(topic)) {
                    return;
                }

                clearTimeout(timeout);

                client.unsubscribe(this.__prefixTopicNs(subscribeTo), {}, (err) => {
                    client.off('message', onMessage);

                    if (err) {
                        this.logger.warn(err.message, null, err);
                    }

                    resolve(client);
                });
            };

            const timeout = setTimeout(() => {
                client.off('message', onMessage);
                reject(new Error(`Probe on topic ${publishTo} was not received on topic ${subscribeTo}. An MQTT5 compilant broker is required`));
            }, timeoutMs);

            client.subscribe(this.__prefixTopicNs(subscribeTo), {}, (err) => {
                if (err) {
                    reject(err);
                    return;
                }

                client.on('message', onMessage);

                client.publish(this.__prefixTopicNs(publishTo), 'probe', {}, err => {
                    if (err) {
                        reject(err);
                        return;
                    }
                });
            });
        });
    }

    __validateJson(json) {
        if ((!!json) && ((json.constructor === Object) || Array.isArray(json))) {
            return;
        }

        throw new Error('Given JSON document is neither an object nor an Array');
    }
}

MqttClient.prefixTopicNs = function prefixTopicNs(topic, topicNs) {
    if (topicNs === null) {
        return topic;
    }

    return topic.replace(new RegExp(`^(\\$share\\/[^\\/]+\\/)?(?:${topicNs})?(.+)$`), `$1${topicNs}$2`);
};

MqttClient.createClientId = function createClientId(prefix = null) {
    const uniquePart = uuid.v4().substring(0, 8);

    if (prefix === null) {
        return uniquePart;
    }

    return [prefix, uniquePart].join('-');
};

class NamedMqttClient extends MqttClient {
    constructor(name, brokerUrl, topicNs, checkMqtt5Compatibility, __clientId = MqttClient.createClientId(`${name}.MqttClient`)) {
        super(brokerUrl, topicNs, checkMqtt5Compatibility, new Logger(__clientId), __clientId);
    }
}

module.exports = {
    MqttClient,
    NamedMqttClient,
    MqttProtocolAdapter
};