/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const NullLogger = require('../../helpers/null.logger');
const importFresh = require('import-fresh');
const mock = require('mock-require');
const { prepareMockedMqttClient } = require('../../helpers/mock/mqtt.mock');
const ProtocolGateway = require('../../../src/core/protocolGateway');

describe('core.protocolGateway', () => {
    let clientMock = null;

    beforeAll(() => {
        mock('../../../src/core/util/logger', NullLogger);
    });

    afterAll(() => {
        mock.stop('../../../src/core/util/logger');
    });

    beforeEach(() => {
        clientMock = prepareMockedMqttClient();
    });

    function createProtocolGateway(displayName, usePlatformProtocolOnly) {
        // Clear cache, since Protocol Gateway reinstantiates the module
        delete require.cache[require.resolve('../../../src/core/util/mqttClient')];
        return new ProtocolGateway(importFresh('../../resources/protocolGateway.config.json'), displayName, usePlatformProtocolOnly);
    }

    it('should publish messages to all defined adapters', async () => {
        const pg = createProtocolGateway('default', false);

        spyOn(clientMock, 'mPublish').and.callThrough();

        clientMock.mConnect();

        await pg.publishJson('test', { hello: 123 }, ProtocolGateway.createPublishOptions(), true);

        // 2 mqtt5 probes and 2 message publications
        expect(clientMock.mPublish.calls.count()).toBe(4);

        expect(clientMock.mPublish.calls.argsFor(1)[0]).toBe('iotea/test');
        expect(clientMock.mPublish.calls.argsFor(1)[1]).toBe('{"hello":123}');

        expect(clientMock.mPublish.calls.argsFor(3)[0]).toBe('iotea/test');
        expect(clientMock.mPublish.calls.argsFor(3)[1]).toBe('{"hello":123}');
    });

    it('It should publish messages to a specific adapters', async () => {
        const pg = createProtocolGateway('default', false);

        spyOn(clientMock, 'mPublish').and.callThrough();

        const adapter1 = pg.adapters[0];
        spyOn(adapter1.instance, 'publish').and.callThrough();

        const adapter2 = pg.adapters[1];
        spyOn(adapter2.instance, 'publish').and.callThrough();

        clientMock.mConnect();

        await pg.publishJson('test', { hello: 123 }, ProtocolGateway.createPublishOptions(false, adapter1.id), true);

        // 1 mqtt5 probe and 1 message publication
        expect(clientMock.mPublish.calls.count()).toBe(2);
        expect(adapter1.instance.publish).toHaveBeenCalled();
        expect(adapter2.instance.publish).not.toHaveBeenCalled();
    });

    it('It should not stash', async () => {
        const pg = createProtocolGateway('default', true);

        spyOn(clientMock, 'mPublish').and.callThrough();

        const adapter1 = pg.adapters[0];
        spyOn(adapter1.instance.client, 'publish').and.callThrough();

        clientMock.mConnect();

        const options = ProtocolGateway.createPublishOptions();
        options.stash = false;
        await pg.publishJson('test', { hello: 123 }, options, true);

        // Disconnect the MQTT client
        clientMock.mClose();

        await pg.publishJson('test2', { hello: 1234 }, options, true);

        // Check stash flag set to false
        expect(adapter1.instance.client.publish.calls.mostRecent().args[3]).toBeFalse();

        // Check if latest message was not sent
        expect(clientMock.mPublish.calls.mostRecent().args[0]).toBe('iotea/test');
    });

    it('It should subscribe a topic from all defined adapters', async () => {
        const pg = createProtocolGateway('default', false);

        spyOn(clientMock, 'mOnSubscription').and.callThrough();

        clientMock.mConnect();

        const spy = jasmine.createSpy('callback', () => {});

        await pg.subscribeJson('foo', spy, ProtocolGateway.createSubscribeOptions(false), true);

        // 2 mqtt5 probe subscriptions and 2 subscriptions
        expect(clientMock.mOnSubscription.calls.count()).toBe(4);
        expect(clientMock.mOnSubscription.calls.argsFor(1)[0]).toBe('iotea/foo');
        expect(clientMock.mOnSubscription.calls.argsFor(3)[0]).toBe('iotea/foo');

        await pg.publishJson('foo', { foo: 56 }, ProtocolGateway.createPublishOptions(), true);

        expect(spy.calls.count()).toBe(2);

        expect(spy.calls.argsFor(0)).toEqual([ { foo: 56 }, 'foo', 'mock-mqtt1' ]);
        expect(spy.calls.argsFor(1)).toEqual([ { foo: 56 }, 'foo', 'mock-mqtt2' ]);
    });

    it('It should subscribe a topic from a specific adapter', async () => {
        const pg = createProtocolGateway('default', false);

        spyOn(clientMock, 'mOnSubscription').and.callThrough();

        const adapter1 = pg.adapters[0];
        spyOn(adapter1.instance, 'subscribe').and.callThrough();

        const adapter2 = pg.adapters[1];
        spyOn(adapter2.instance, 'subscribe').and.callThrough();

        clientMock.mConnect();

        const spy = jasmine.createSpy('callback', () => {});

        await pg.subscribeJson('bar', spy, ProtocolGateway.createSubscribeOptions(null, adapter1.id), true);

        // 1 mqtt5 probe subscription and 1 subscription
        expect(clientMock.mOnSubscription.calls.count()).toBe(2);
        expect(adapter1.instance.subscribe).toHaveBeenCalled();
        expect(adapter2.instance.subscribe).not.toHaveBeenCalled();

        await pg.publishJson('bar', { hello: 128 }, ProtocolGateway.createPublishOptions(false, adapter1.id), true);

        expect(spy.calls.count()).toBe(1);

        expect(spy.calls.argsFor(0)).toEqual([ { hello: 128 }, 'bar', adapter1.id ]);
    });

    it('It should subscribe a shared topic from all adapters', async () => {
        const pg = createProtocolGateway('default', false);

        spyOn(clientMock, 'mOnSubscription').and.callThrough();

        clientMock.mConnect();

        await pg.subscribeJsonShared('somegroup', 'baz', () => {}, ProtocolGateway.createSubscribeOptions(false), true);

        // 2 mqtt5 probe subscriptions and 2 subscriptions
        expect(clientMock.mOnSubscription.calls.count()).toBe(4);
        expect(clientMock.mOnSubscription.calls.argsFor(1)[0]).toBe('$share/somegroup/iotea/baz');
        expect(clientMock.mOnSubscription.calls.argsFor(3)[0]).toBe('$share/somegroup/iotea/baz');
    });

    it('It should subscribe a shared topic from a specific adapter', async () => {
        const pg = createProtocolGateway('default', false);

        spyOn(clientMock, 'mOnSubscription').and.callThrough();

        const adapter1 = pg.adapters[0];
        spyOn(adapter1.instance, 'subscribeShared').and.callThrough();

        const adapter2 = pg.adapters[1];
        spyOn(adapter2.instance, 'subscribeShared').and.callThrough();

        clientMock.mConnect();

        const spy = jasmine.createSpy('callback', () => {});

        await pg.subscribeJsonShared('somegroup', 'bar', spy, ProtocolGateway.createSubscribeOptions(null, adapter1.id), true);

        // 1 mqtt5 probe subscription and 1 subscription
        expect(clientMock.mOnSubscription.calls.count()).toBe(2);
        expect(adapter1.instance.subscribeShared).toHaveBeenCalled();
        expect(adapter2.instance.subscribeShared).not.toHaveBeenCalled();

        await pg.publishJson('bar', { hello: 128 }, ProtocolGateway.createPublishOptions(true), true);

        expect(spy.calls.count()).toBe(1);

        expect(spy.calls.argsFor(0)).toEqual([ { hello: 128 }, 'bar', adapter1.id ]);
    });

    it('should not be possible to only using platform protocols and publish to all adapters', async () => {
        const pg = createProtocolGateway('default', true);
        await expectAsync(pg.publishJson('bar', { hello: 128 }, ProtocolGateway.createPublishOptions(false), true)).toBeRejected();
    });

    it('should validate a given configuration', () => {
        expect(() => ProtocolGateway.validateConfiguration('ttt')).toThrow();
        expect(() => ProtocolGateway.validateConfiguration({ adapters: 'foo' })).toThrow();

        expect(() => ProtocolGateway.validateConfiguration({
            adapters: [
                {}, {}
            ]
        }, true)).toThrow();

        expect(() => ProtocolGateway.validateConfiguration({
            adapters: [
                { platform: true }, { platform: true }
            ]
        }, true)).toThrow();
    });

    it('should get configuration metadata', () => {
        expect(ProtocolGateway.hasPlatformAdapter(ProtocolGateway.createDefaultConfiguration([ { platform: true }]))).toBeTruthy();
        expect(ProtocolGateway.hasPlatformAdapter(ProtocolGateway.createDefaultConfiguration([ { }]))).toBeFalsy();
        expect(ProtocolGateway.getAdapterCount(ProtocolGateway.createDefaultConfiguration([ { platform: true }, {}, {}]))).toBe(3);
    });
});