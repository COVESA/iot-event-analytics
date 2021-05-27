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
/**
 * MQTT Client Module.
 *
 * @module mqttClient
 */

/**
 * The class represents the MQTT communication layer between the platform components.
 */
class MqttProtocolAdapter {
    /**
     * Constructs an instance of the MqttProtocolAdapter based on a json config. Creates an internal MQTT client to be
     * used by {@link module:mqttClient~MqttProtocolAdapter#publish} and
     * {@link module:mqttClient~MqttProtocolAdapter#subscribe} methods.
     *
     * @param {*} config - Json object, which carries all settings necessary to establish connection to the broker as
     * 'brokerUrl' and the 'topicNamespace' prefix, prepended to publish and subscription topics.
     * @param {string} displayName - Descriptive name to identify the module, which uses this MqttProtocolAdapter.
     */
    constructor(config, displayName = null) {
        this.client = null;

        this.config = new JsonModel(config);

        this.brokerUrl = this.config.get('brokerUrl');

        this.topicNs = this.config.get('topicNamespace', null);

        if (displayName === null) {
            this.client = new MqttClient(
                this.brokerUrl,
                this.topicNs,
                true
            );
        } else {
            this.client = new NamedMqttClient(
                displayName,
                this.brokerUrl,
                this.topicNs,
                true
            );
        }
    }

    /**
     * Publishes a message to the underlying MQTT Broker. The topic is prefixed by 'topicNamespace' from the configuration.
     *
     * @param {string} topic  - Topic to publish the message to.
     * @param {string} message - Message to be published.
     * @param {*} publishOptions - Connection options.
     * @returns a promise.
     */
    publish(topic, message, publishOptions = {}) {
        // convert PublishOptions to MqttPublishOptions, which are directly sent as argument to client.publish
        const mqttPublishOptions = {
            retain: publishOptions.retain === true
        };

        return this.client.publish([ this.__prefixTopicNs(topic) ], message, mqttPublishOptions, publishOptions.stash);
    }

    /**
     * Subscribes for messages with a specified topic to the MQTT Broker. The topic is prefixed by 'topicNamespace'
     * from the configuration.
     *
     * @param {string} topic - Topic to subscribe for.
     * @param {subscribeCallback} cb - Callback function invoked when a subscription message is received.
     * @returns a promise.
     */

    /**
     * @callback subscribeCallback
     * @param {string} message - The message as string
     * @param {string} topic - The "namespace-less" topic
     */
     subscribe(topic, callback) {
        return this.client.subscribe(this.__prefixTopicNs(topic), (msg, topic) => {
            callback(msg, this.__stripTopicNamespace(topic));
        });
    }

    /**
     * Creates a shared subscription to the MQTT Broker by constructing a topic '$share/\<group\>/\<topicNamespace\>/\<topic\>'.
     *
     * @param {string} group - Group segment of the shared subscription topic.
     * @param {string} topic - Topic to subscribe for.
     * @param {subscribeCallback} cb - Callback function invoked when a subscription message is received.
     * @returns a promise.
     */
    subscribeShared(group, topic, callback) {
        return this.client.subscribe(`$share/${group}/${this.__prefixTopicNs(topic)}`, (msg, topic) => {
            callback(msg, this.__stripTopicNamespace(topic));
        });
    }

    /**
     * Gets the unique id that identifies this MqttClient among other MqttClient instances.
     *
     * @returns {string} - The url to the broker.
     */
    getId() {
        return this.brokerUrl;
    }

    __prefixTopicNs(topic) {
        if (this.topicNs === null) {
            return topic;
        }

        return topic.replace(new RegExp(`^(\\$share\\/[^\\/]+\\/)?(?:${this.topicNs})?(.+)$`), `$1${this.topicNs}$2`);
    }

    __stripTopicNamespace(topic) {
        let topicNsIndex = topic.indexOf(this.topicNs);

        if (topicNsIndex === -1 || topicNsIndex > 0) {
            return topic;
        }

        return topic.substring(this.topicNs.length);
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
            topicNamespace: 'iotea/'
        }
    };
};




/**
 * Instances of this class are used to establish connection to an MQTT Broker, to publish and subscribe for messages.
 */
class MqttClient {
    /**
     * Creates an instance of MqttClient with the passed parameters.
     *
     * @param {string} brokerUrl - URL to connect to the broker, e.g. mqtt://mosquitto-local:1883.
     * @param {string} [topicNs = null] - Topic prefix prepended to topics in MqttClient subscribe, unsubscribe and
     * publish methods. Must end with a slash.
     * @param {boolean} [checkMqtt5Compatibility = true] - Indicates if an MQTT 5 compatibility check will be performed
     * upon connection to the broker.
     * @param {object} [logger = null] - a Logger object to send log messages to.
     * @param {string} clientId - Client Id to establish connection with. If missing, the MqttClient will generate one.
     */
    constructor(brokerUrl, topicNs = null, checkMqtt5Compatibility = true, logger = null, clientId = MqttClient.createClientId('MqttClient')) {
        if (logger === null) {
            logger = new Logger(clientId);
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

    /**
     * Publishes a message under multiple topics to the MQTT Broker. The topics are prefixed by this instance _topicNs_.
     *
     * @param {string|string[]} topics - Topic to publish the message to.
     * @param {string} message - Message to be published.
     * @param {*} [mqttPublishOptions = {}] - Connection options.
     * @returns a promise for chaining.
     */
    publish(topics, message, mqttPublishOptions = {}, stash = true) {
        if (this.clientPromise && this.clientPromise.done === false && stash === false) {
            // MQTT client is offline. Do not wait until it is online again.
            // If we just try to publish it anyway, we end up with a huge stack of unsent messages, which will be published
            // when the client is reconnected again
            this.logger.warn(`MQTT Broker ${this.brokerUrl} is offline. Cannot publish message to topics ${topics}`);
            return;
        }

        if (!Array.isArray(topics)) {
            topics = [ topics ];
        }

        return Promise.all(topics.map(topic => this.__publish(topic, message, mqttPublishOptions)));
    }

    /**
     * Publishes a json message to the MQTT Broker. The topics are prefixed by _topicNs_. Only valid json messages are
     * published.
     *
     * @param {string|string[]} topics - Topic to publish the message to.
     * @param {*} json - JSON object to publish as a message.
     * @param {*} [mqttPublishOptions = {}] - Connection options.
     * @returns a promise for chaining.
     */
    publishJson(topics, json, mqttPublishOptions = {}, stash) {
        // Check if it's actually json, which should be sent
        try {
            this.__validateJson(json);
            return this.publish(topics, JSON.stringify(json), mqttPublishOptions, stash);
        }
        catch(err) {
            return Promise.reject(err);
        }
    }

    /**
     * Subscribes for messages with a specified topic to the MQTT Broker. The topic is prefixed by the
     * predefined _topicNS_ of this MqttClient instance.
     *
     * @param {string} topic - Topic to subscribe for.
     * @param {*} cb - Callback function invoked when a subscription message is received.
     * @returns a promise.
     */
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

    /**
     * Subscribes for json messages with a specified topic to the MQTT Broker. The topic is prefixed by the predefined
     * _topicNS_ of this MqttClient object. If a non-json message is received from the subscription, an error will be
     * logged and the message will not be forwarded to the callback.
     *
     * @param {string} topic - Topic to subscribe for.
     * @param {*} cb - Callback function invoked when a subscription message is received.
     * @returns a promise.
     */
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

    /**
     * Unsubscribes for a list of topics. Each topic is prefixed with the predefined _topicNS_ of this MqttClient
     * instance.
     *
     * @param {string|string[]} topics - Topic(s) to unsubscribe for.
     * @returns a promise.
     */
    unsubscribe(topics) {
        if (!Array.isArray(topics)) {
            topics = [ topics ];
        }
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

    /**
     * Disconnects the MQTT client from the broker.
     *
     * @param {boolean} [force = true] - Do not wait for acknowledgement.
     * @returns a promise.
     */
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

    __publish(topic, message, mqttPublishOptions = {}) {
        return this.__getClient()
            .then(client => {
                return new Promise((resolve, reject) => {
                    try {
                        topic = this.__prefixTopicNs(topic);

                        this.logger.verbose(`Publishing ${message} to ${topic} @ ${this.brokerUrl}`);

                        client.publish(topic, message, mqttPublishOptions, (err, packet) => {
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
                        this.logger.debug('MQTT connection successfully established');
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
                reject(new Error(`Probe on topic ${publishTo} was not received on topic ${subscribeTo}. An MQTT5 compliant broker is required`));
            }, timeoutMs);

            client.subscribe(this.__prefixTopicNs(subscribeTo), {}, (err) => {
                if (err) {
                    reject(err);
                    return;
                }

                client.on('message', onMessage);

                client.publish(this.__prefixTopicNs(publishTo), 'probe', {}, err => {
                    if (err) {
                        client.off('message', onMessage);
                        reject(err);
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

/**
 * Having a prefix _topicNs_ and a _topic_, constructs a new MQTT topic. In case the original topic is a shared one,
 * having the '$share' prefix, _topicNs_ is inserted at the right place between $share and _topic_.
 *
 * @param {string} topic - Topic to be prefixed.
 * @param {string} topicNs - Prefix to be inserted.
 * @returns {string}
 */
MqttClient.prefixTopicNs = function prefixTopicNs(topic, topicNs) {
    if (topicNs === null) {
        return topic;
    }

    return topic.replace(new RegExp(`^(\\$share\\/[^\\/]+\\/)?(?:${topicNs})?(.+)$`), `$1${topicNs}$2`);
};

/**
 * Creates a unique client id to be used when connecting to the MQTT Broker. The id is constructed from a generated UUID
 * string and prefixed by the optionally passed <prefix> parameter.
 *
 * @param {string} [prefix = null] - Prefix to be prepended.
 * @returns {string}
 */
MqttClient.createClientId = function createClientId(prefix = null) {
    const uniquePart = uuid.v4().substring(0, 8);

    if (prefix === null) {
        return uniquePart;
    }

    return [prefix, uniquePart].join('-');
};

/**
 * Utility class in which the MQTT client Id includes the readable name of the MqttBroker instance.
 */
class NamedMqttClient extends MqttClient {
    /**
     * Creates an instance of NamedMqttClient with the passed parameters.
     *
     * @param {string} name - A readable name to identify, which module uses this NamedMqttBroker instance.
     * @param {string} brokerUrl - URL to connect to the broker, e.g. mqtt://mosquitto-local:1883.
     * @param {string} topicNs - Topic prefix prepended to topics in {@link module:mqttClient~MqttClient#subscribe},
     * {@link module:mqttClient~MqttClient#unsubscribe} and {@link module:mqttClient~MqttClient#publish} methods.
     * @param {boolean} checkMqtt5Compatibility - Indicates if an MQTT 5 compatibility check will be performed upon
     * connection to the broker.
     * @param {string} __clientId - An optional paramter. If missing a client Id will be generated.
     */
    constructor(name, brokerUrl, topicNs, checkMqtt5Compatibility, __clientId = MqttClient.createClientId(`${name}.MqttClient`)) {
        super(brokerUrl, topicNs, checkMqtt5Compatibility, new Logger(__clientId), __clientId);
    }
}

module.exports = {
    MqttClient,
    NamedMqttClient,
    MqttProtocolAdapter
};