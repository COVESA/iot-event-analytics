/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const { Duplex, Writable } = require('stream');
const ReconnectingWebSocket = require('reconnecting-websocket');
const WebSocket = require('ws');

const Logger = require('../../core/util/logger');

class AsyncQueue {
    constructor() {
        this.items = [];
        this.nextItemPromises = [];
    }

    put(item) {
        if (this.nextItemPromises.length === 0) {
            this.items.push(item);
            return;
        }

        this.nextItemPromises.shift().onNextItem(item);
    }

    get() {
        if (this.items.length === 0) {
            let onNextItem = null;

            const nextItemPromise = new Promise(resolve => onNextItem = resolve);

            this.nextItemPromises.push({
                onNextItem,
                nextItemPromise
            });

            return nextItemPromise;
        }

        return Promise.resolve(this.items.shift());
    }
}

class WebsocketStream extends Duplex {
    constructor(address, rwsOptions) {
        super();

        this.logger = new Logger('WebsocketStream');

        this.isOpened = null;

        this.messageQueue = new AsyncQueue();
        this.socket = new ReconnectingWebSocket(address, [], rwsOptions);

        this.socket.addEventListener('close', this.__onClose.bind(this));
        this.socket.addEventListener('open', this.__onOpen.bind(this));
        this.socket.addEventListener('error', this.__onError.bind(this));
        this.socket.addEventListener('message', this.__onMessage.bind(this));
    }

    __onClose() {
        this.isOpened = false;
    }

    __onError(err) {
        this.logger.error('__onError() invoked on given socket.', null, new Error(err.message));
    }

    __onOpen() {
        if (this.isOpened === false) {
            this.logger.info('Reopened WebSocket successfully');
            this.emit(WebsocketStream.REOPEN);
        }

        this.isOpened = true;
    }

    __onMessage(msg) {
        this.messageQueue.put(msg.data);
    }

    async _read() {
        this.push(await this.messageQueue.get());
    }

    _write(chunk, encoding, next) {
        this.socket.send(chunk);
        next();
    }
}

WebsocketStream.REOPEN = 'reopen';

class KuksaValWebsocket {
    constructor(address, token, rwsOptions = {}) {
        this.logger = new Logger('Kuksa.valWebsocket');

        this.subscriptions = [];
        this.isAuthorized = false;

        rwsOptions = Object.assign({
            WebSocket,
            maxReconnectionDelay: 1000,
            maxEnqueuedMessages: 0,
            reconnectionDelayGrowFactor: 1
        }, rwsOptions);

        this.jwt = token;

        this.duplex = new WebsocketStream(address, rwsOptions);

        this.duplex.on(WebsocketStream.REOPEN, this.__onReopen.bind(this));
    }

    publish(path, value) {
        return this.__publishAny('set', { path, value });
    }

    get(path) {
        return this.__publishAny('get', { path });
    }

    resolvePaths(path) {
        if (path.indexOf('*') === -1) {
            return [ path ]
        }

        return this.get(path)
            .then(async response => {
                // Children
                // response.value is array of objects with path as single key
                if (Array.isArray(response.value)) {
                    return response.value.map(pathObject => Object.keys(pathObject)[0]);
                }

                // Single path
                if (response.path) {
                    // Single path
                    return [ response.path ]
                }

                // Branch
                // response.value is object with paths as keys
                return Object.keys(response.value);
            });
    }

    getMetadata(path) {
        return this.__publishAny('getMetadata', { path });
    }

    subscribe(path, onMessage, onError = () => { }, publishRetainedValues = false) {
        if (path.indexOf('*') === -1) {
            return this.__subscribe(path, onMessage, onError, publishRetainedValues);
        }

        // If a star is in the path, get every node first and create a separate subscription for each
        return this.resolvePaths(path)
            .then(async absPaths => {
                // response.value holds an array of objects, which have the path as key
                const subscription = new MultiPathSubscription();

                for (let absPath of absPaths) {
                    try {
                        subscription.addPathSubscription(await this.__subscribe(absPath, onMessage, onError, publishRetainedValues));
                    }
                    catch (err) {
                        this.logger.warn(`Could not subscribe to path ${absPath}`, null, err);
                    }
                }

                return subscription;
            });
    }

    __subscribe(path, onMessage, onError = () => { }, publishRetainedValue = false) {
        this.logger.info(`Subscribing to path ${path}...`);

        return this.__publishAny('subscribe', { path })
            .then(subMsg => {
                if (subMsg.error) {
                    throw new Error(subMsg.error.message);
                }

                this.logger.info(`Successfully subscribed to path ${path}`);
                return new PathSubscription(path, subMsg.subscriptionId, onMessage, onError, this.duplex);
            })
            .then(async subscription => {
                this.subscriptions.push(subscription);

                if (!publishRetainedValue) {
                    return subscription;
                }

                try{

                    const retainedValue = await this.get(path);
                    /**
                     * TRANSFORM
                     *
                     * {
                     *   action: 'get',
                     *   path: 'Bike.Weight',
                     *   requestId: '11455299',
                     *   timestamp: 1594890989,
                     *   value: 100
                     * }
                     *
                     * TO
                     *
                     * {
                     *   action: 'subscribe',
                     *   subscriptionId: 2174267073,
                     *   timestamp: 1594890990,
                     *   value: 100
                     * }
                     *
                     * to simulate an incoming change
                     **/
                    await subscription.__onMessage({
                        action: 'subscribe',
                        subscriptionId: subscription.subId,
                        timestamp: retainedValue.timestamp,
                        value: retainedValue.value
                    });
                }
                catch(err) {
                    this.logger.debug(`Could not read retained value from ${path}`);
                }

                return subscription;
            });
    }

    async __publishAny(action, data = {}, needsAuthorization = true, timeoutMs = 30000) {
        return this.__try_connect()
            .then(() => {
                if (!needsAuthorization) {
                    // Authorization request itself does not need authorization
                    // Prevent looping
                    return;
                }

                return this.__authorize(this.jwt);
            })
            .then(() => {
                const requestId = Math.floor(Math.random() * 100000000) + '';
                const rs = new RequestSubscription(requestId, timeoutMs, this.duplex);

                if (!this.duplex.write(JSON.stringify(
                    Object.assign(data, {
                        requestId: requestId,
                        action
                    })
                ), 'utf8', err => {
                    if (err) {
                        throw err;
                    }
                })) {
                    // Could not write
                }

                return rs.wait();
            });
    }

    async __onReopen() {
        this.isAuthorized = false;

        // Resubscribe
        for (let i = this.subscriptions.length - 1; i >= 0; i--) {
            const subscription = this.subscriptions[i];

            if (!subscription.isActive) {
                // Remove inactive subscriptions
                this.logger.info(`Remove invalid subscription for path ${subscription.path}`);
                this.subscriptions.splice(i, 1);
                continue;
            }

            try {
                this.logger.info(`Resubscribing to path ${subscription.path}...`);
                await this.__publishAny('subscribe', { path: subscription.path })
                    .then(subMsg => {
                        this.logger.info(`Resubscribed to path ${subscription.path} successfully with subscription Id ${subMsg.subscriptionId}`);

                        subscription.setSubscriptionId(subMsg.subscriptionId);

                        if (subMsg.error) {
                            subscription.onError(new Error(subMsg.error));
                        }
                    });
            }
            catch (err) {
                this.logger.error(`An error occured during resubscription`, null, err);
            }
        }
    }

    async __authorize(token) {
        if (this.isAuthorized) {
            return;
        }

        return this.__publishAny('authorize', { tokens: token }, false)
            .then(msg => {
                if (msg.error) {
                    throw new Error(msg.error.message);
                }

                this.isAuthorized = true;
            });
    }

    async __try_connect() {
        do {
            await new Promise(resolve => setTimeout(resolve, 1000));
        } while (this.duplex.isOpened !== true);
    }
}

class Subscription {
    constructor(duplex) {
        this.duplex = duplex;
        this.isActive = true;
        this.callbackStream = new CallbackStream(this.__onMessage.bind(this));
        this.duplex.pipe(this.callbackStream);
    }

    unsubscribe() {
        this.isActive = false;
        this.duplex.unpipe(this.callbackStream);
    }

    __onMessage() {
        // Parameters: msg
    }
}

class RequestSubscription extends Subscription {
    constructor(requestId, timeoutMs = 30000, duplex) {
        super(duplex);

        this.requestId = requestId;

        this.responsePromise = new Promise((resolve, reject) => {
            this.onReceiveResponse = resolve;
            this.onRejectResponse = reject;
        });

        if (this.duplex.isOpened === true) {
            this.timeout = setTimeout(() => {
                // Is defined in any case
                this.unsubscribe();
                this.onRejectResponse(new Error(`Timeout after ${timeoutMs}ms`));
            }, timeoutMs);
        }
    }

    wait() {
        return this.responsePromise;
    }

    __onMessage(msg) {
        // Listens for a response with the same requestId and then resolve the promise
        if (msg.requestId !== this.requestId) {
            return;
        }

        clearTimeout(this.timeout);
        this.unsubscribe();

        this.onReceiveResponse(msg);
    }
}

class PathSubscription extends Subscription {
    constructor(path, subId, onMessage, onError = () => { }, duplex) {
        super(duplex);
        this.path = path;
        this.subId = subId;
        this.onMessage = onMessage;
        this.onError = onError;
    }

    setSubscriptionId(subId) {
        this.subId = subId;
    }

    async __onMessage(msg) {
        if (msg instanceof Error) {
            this.onError(msg);
            return;
        }

        if (msg.subscriptionId !== this.subId) {
            return;
        }

        msg.path = this.path;

        await this.onMessage(msg);
    }
}

class MultiPathSubscription {
    constructor() {
        this.subscriptions = [];
    }

    addPathSubscription(subscription) {
        this.subscriptions.push(subscription);
    }

    unsubscribe() {
        for (let subscription of this.subscriptions) {
            subscription.unsubscribe();
        }
    }
}

class CallbackStream extends Writable {
    constructor(cb) {
        super();
        this.cb = cb;
    }

    async _write(chunk, enc, next) {
        await this.cb(JSON.parse(chunk.toString('utf8')));
        next();
    }
}

module.exports = KuksaValWebsocket;