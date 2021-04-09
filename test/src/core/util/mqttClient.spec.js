/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const NullLogger = require('../../../helpers/null.logger');
const mock = require('mock-require');
const importFresh = require('import-fresh');

const { prepareMockedMqttClient } = require('../../../helpers/mock/mqtt.mock');

describe('core.util.mqttClient', () => {
    let MqttClientMock = null;

    beforeAll(() => {
        mock('../../../../src/core/util/logger', NullLogger);
        MqttClientMock = require('../../../helpers/mock/mqtt.mock').MqttClientMock;
    });

    afterAll(() => {
        mock.stop('../../../../src/core/util/logger');
    });

    let clientMock = null;

    function createMqttClientWithArgs() {
        let args = Array.from(arguments);

        if (args.length > 0 && args[args.length - 1] instanceof MqttClientMock) {
            clientMock = prepareMockedMqttClient(args[args.length - 1]);
            args = args.slice(0, -1);
        } else {
            clientMock = prepareMockedMqttClient();
        }

        const {
            MqttClient
        } = importFresh('../../../../src/core/util/mqttClient');

        return new MqttClient(...args);
    }

    it('should publish JSON documents to multiple topics', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();
        spyOn(clientMock, 'mPublish').and.callThrough();

        await client.publishJson(['test1', 'test2', 'test3'], { hello: 'world' });

        expect(clientMock.mPublish.calls.count()).toBe(3);
        expect(clientMock.mPublish.calls.argsFor(0)[0]).toBe('test1');
        expect(clientMock.mPublish.calls.argsFor(1)[0]).toBe('test2');
        expect(clientMock.mPublish.calls.argsFor(2)[0]).toBe('test3');
    });

    it('should fail, if JSON should be published, but no json message is provided', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();
        spyOn(clientMock, 'mPublish').and.callThrough();

        await expectAsync(client.publishJson('test1', 'world')).toBeRejected();
    });

    it('should not stash messages, if flag is given in the publish options', async () => {
        const client = createMqttClientWithArgs('', 'iotea/', false);

        const p1 = client.publishJson('test1', { hello: 'world' });
        const p2 = client.publishJson('test2', { hello: 'world2' });
        const p3 = client.publishJson('test3', { hello: 'world3' }, { stash: false });

        setImmediate(() => clientMock.mConnect());

        expect(await p1).toEqual([{}]);
        expect(await p2).toEqual([{}]);
        expect(await p3).toBeUndefined();
    });

    it('should reject publish promise, if a publish error occurs', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();

        spyOn(clientMock, 'mPublish').and.callFake((topic, message, options, callback) => {
            if (topic === 'test1') {
                throw new Error('Foo');
            }

            callback(new Error('Bar'), {});
        });

        await expectAsync(client.publishJson('test1', {})).toBeRejectedWithError('Foo');
        await expectAsync(client.publishJson('test2', {})).toBeRejectedWithError('Bar');
    });

    it('should automatically prefix the given topic namespace', async () => {
        const client = createMqttClientWithArgs('', 'iotea/', false);

        clientMock.mConnect();
        spyOn(clientMock, 'mPublish').and.callThrough();

        await client.publishJson('test1', { hello: 'world' });

        expect(clientMock.mPublish.calls.count()).toBe(1);
        expect(clientMock.mPublish.calls.argsFor(0)[0]).toBe('iotea/test1');
    });

    it('should reject an invalid topic namespace', async () => {
        expect(() => createMqttClientWithArgs('', 'iotea', false)).toThrow();
    });

    it('should subscribe to a given topic', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();
        spyOn(clientMock, 'mOnSubscription').and.callThrough();

        const callbackSpy = jasmine.createSpy('msgCallback', () => {});

        await client.subscribeJson('test1', callbackSpy);

        clientMock.mPublish('test1', JSON.stringify({ hello: 'World' }));

        expect(callbackSpy.calls.mostRecent().args).toEqual([ { hello: 'World' }, 'test1']);
    });

    it('should subscribe to multiple topics', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();
        spyOn(clientMock, 'mOnSubscription').and.callThrough();

        const callbackSpy1 = jasmine.createSpy('msgCallback1', () => {});
        const callbackSpy2 = jasmine.createSpy('msgCallback2', () => {});

        await client.subscribeJson('test1', callbackSpy1);
        await client.subscribeJson('test2', callbackSpy2);

        clientMock.mPublish('test1', JSON.stringify({ hello: 'World1' }));
        clientMock.mPublish('test2', JSON.stringify({ hello: 'World2' }));

        expect(callbackSpy1.calls.mostRecent().args).toEqual([ { hello: 'World1' }, 'test1']);
        expect(callbackSpy2.calls.mostRecent().args).toEqual([ { hello: 'World2' }, 'test2']);
    });

    it('should reject subscription promise, if a subscription error occurs', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();

        spyOn(clientMock, 'mOnSubscription').and.callFake((topic, options, callback) => {
            if (topic === 'test1') {
                throw new Error('Foo');
            }

            callback(new Error('Bar'), {});
        });

        await expectAsync(client.subscribeJson('test1')).toBeRejectedWithError('Foo');
        await expectAsync(client.subscribeJson('test2')).toBeRejectedWithError('Bar');
    });

    it('should output a warning, if the subscription callback throws an error', async () => {
        const client = createMqttClientWithArgs('', null, false);
        spyOn(client.logger, '__log').and.callThrough();

        clientMock.mConnect();

        const err = new Error('Error in callback');

        const callbackSpy = jasmine.createSpy().and.throwError(err);

        await client.subscribeJson('test1', callbackSpy);

        clientMock.mPublish('test1', JSON.stringify({ hello: 'World1' }));

        expect(callbackSpy).toHaveBeenCalledWith({ hello: 'World1' }, 'test1');
        expect(client.logger.__log.calls.mostRecent().args[0]).toBe('Error in callback');
    });

    it('should unsubscribe from a given topic', async () => {
        const client = createMqttClientWithArgs('', 'iotea/', false);

        clientMock.mConnect();
        spyOn(clientMock, 'mOnUnsubscribe').and.callThrough();

        await client.unsubscribe([ 'test1', 'test2' ]);

        expect(clientMock.mOnUnsubscribe.calls.count()).toBe(1);
        expect(clientMock.mOnUnsubscribe.calls.mostRecent().args[0]).toEqual([ 'iotea/test1', 'iotea/test2' ]);
    });

    it('should reject unsubscribe promise, if a unsubscribe error occurs', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();

        spyOn(clientMock, 'mOnUnsubscribe').and.callFake((topics, options, callback) => {
            if (topics[0] === 'test1') {
                throw new Error('Foo');
            }

            callback(new Error('Bar'), {});
        });

        await expectAsync(client.unsubscribe([ 'test1' ])).toBeRejectedWithError('Foo');
        await expectAsync(client.unsubscribe([ 'test2' ])).toBeRejectedWithError('Bar');
    });

    it('should disconnect from a broker', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();
        spyOn(clientMock, 'mClose').and.callThrough();

        await client.disconnect();

        expect(clientMock.mClose).toHaveBeenCalledWith();
    });

    it('should reject disconnect promise, if a disconnect error occurs', async () => {
        const client = createMqttClientWithArgs('', null, false);

        clientMock.mConnect();
        spyOn(clientMock, 'mClose').and.throwError('DisconnectionError');

        await expectAsync(client.disconnect()).toBeRejectedWithError('DisconnectionError');
    });

    it('should check the mqtt5 capability before subscribing', async () => {
        const client = createMqttClientWithArgs('', null, true);

        spyOn(clientMock, 'mPublish').and.callThrough();
        spyOn(clientMock, 'mOnSubscription').and.callThrough();
        spyOn(clientMock, 'mOnUnsubscribe').and.callThrough();

        clientMock.mConnect();

        await client.subscribeJson('test1', () => {});

        // Check if probe was sent
        const probePublishTopic = clientMock.mPublish.calls.mostRecent().args[0];
        const probeResponseTopic = clientMock.mOnSubscription.calls.argsFor(0)[0];

        // This topic is published to
        expect(probePublishTopic).toMatch(/^probe\/[a-z0-9-]{36}$/g);
        expect(clientMock.mPublish.calls.mostRecent().args[1]).toBe('probe');

        // The shared subscription must receive the message "probe", which is sent over the probePublishTopic
        expect(probeResponseTopic).toMatch(`^\\$share\\/[a-z0-9-]{36}\\/${probePublishTopic}$`);

        // Check if unsubscribed from the shared subscription
        expect(clientMock.mOnUnsubscribe.calls.mostRecent().args[0]).toBe(probeResponseTopic);
    });

    it('should fail, if no mqtt5 support is given', async () => {
        const client = createMqttClientWithArgs('', null, true, new NullLogger(), new MqttClientMock('', false));

        clientMock.mConnect();

        await expectAsync(client.subscribeJson('test1', () => {})).toBeRejected();
    });

    it('should fail, if there is an error during mqtt5 probe subscription', async () => {
        const client = createMqttClientWithArgs('', null, true);

        clientMock.mConnect();

        spyOn(clientMock, 'mOnSubscription').and.callFake((topic, options, callback) => {
            callback(new Error('ErrorOnProbeSubscription'), {});
        });

        await expectAsync(client.subscribeJson('test')).toBeRejectedWithError('ErrorOnProbeSubscription');
    });

    it('should fail, if there is an error during publishing mqtt5 probe', async () => {
        const client = createMqttClientWithArgs('', null, true);

        clientMock.mConnect();

        spyOn(clientMock, 'mPublish').and.callFake((topic, message, options, callback) => {
            callback(new Error('ErrorOnProbePublish'), {});
        });

        await expectAsync(client.subscribeJson('test')).toBeRejectedWithError('ErrorOnProbePublish');
    });

    describe('MqttProtocolAdapter', () => {
        let protocolAdapter = null;

        beforeEach(() => {
            clientMock = prepareMockedMqttClient();

            const {
                MqttProtocolAdapter
            } = importFresh('../../../../src/core/util/mqttClient');

            protocolAdapter = new MqttProtocolAdapter({
                brokerUrl: '',
                topicNamespace: 'iotea/'
            });
        });

        it('should publish a message', async () => {
            clientMock.mConnect();

            spyOn(clientMock, 'mPublish').and.callThrough();

            await protocolAdapter.publish('test1', JSON.stringify({ hello: 'world' }));

            expect(clientMock.mPublish.calls.mostRecent().args[0]).toBe('iotea/test1');
        });

        it('should subscribe to a topic', async () => {
            clientMock.mConnect();

            spyOn(clientMock, 'mOnSubscription').and.callThrough();

            await protocolAdapter.subscribe('test2', () => {});

            expect(clientMock.mOnSubscription.calls.mostRecent().args[0]).toBe('iotea/test2');
        });

        it('should subscribe to a shared topic', async () => {
            clientMock.mConnect();

            spyOn(clientMock, 'mOnSubscription').and.callThrough();

            await protocolAdapter.subscribeShared('foo', 'test3', () => {});

            expect(clientMock.mOnSubscription.calls.mostRecent().args[0]).toBe('$share/foo/iotea/test3');
        });
    });
});