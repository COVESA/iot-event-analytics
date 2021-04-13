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
const InstanceManager = require('../../../src/core/instanceManager');
const MetadataManagerMock = require('../../helpers/mock/metadataManager.mock');
const ProtocolGateway = require('../../../src/core/protocolGateway');
const { MqttProtocolAdapter } = require('../../../src/core/util/mqttClient');
const { DEFAULT_FEATURE_TTL_MS, DEFAULT_SEGMENT, DEFAULT_TYPE } = require('../../../src/core/constants');

describe('core.instanceManager', () => {
    let im = null;
    let mm = null;

    beforeAll(() => {
        mock('../../../src/core/util/logger', NullLogger);
    });

    afterAll(() => {
        mock.stop('../../../src/core/util/logger');
    });

    beforeEach(async () => {
        prepareMockedMqttClient();
        im = createInstanceManager();
        await im.start()
    });

    function createInstanceManager() {
        // Clear cache, since Protocol Gateway reinstantiates the module
        delete require.cache[require.resolve('../../../src/core/util/mqttClient')];
        const im = new InstanceManager(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(true, '')]));
        mm = im.metadataManager = new MetadataManagerMock(importFresh('../../resources/instanceManager.mm.types.json'));
        return im;
    }

    it('should get all instances for a given subject', async () => {
        await im.updateFeature('somesubject', '4711', 'feat1', null, 10, Date.now(), 'test');
        await im.updateFeature('somesubject', '4712', 'feat2', null, 20, Date.now(), 'test');

        let instances = im.getInstances('somesubject');

        expect(instances.length).toBe(2);
        expect(instances[0].id).toBe('4711');
        expect(instances[1].id).toBe('4712');

        instances = im.getInstances('somesubject', /^4712$/g);

        expect(instances.length).toBe(1);
        expect(instances[0].id).toBe('4712');
    });

    it('should get one instance subject', async () => {
        await im.updateFeature('somesubject', '4711', 'feat1', null, 10, Date.now(), 'test');

        let instance = im.getInstance('somesubject', '4711');

        expect(instance).toBeDefined();
        expect(instance.id).toBe('4711');
    });

    it('should get a feature', async () => {
        let feature = await im.getFeature('somesubject', '4711', 'feat3', 'test');

        expect(feature).toEqual({ whenMs: -1, exp: -1, history: [], raw: 42, enc: null, stat: null });

        const now = Date.now();

        await im.updateFeature('somesubject', '4711', 'feat3', null, 22, now, 'test');

        feature = await im.getFeature('somesubject', '4711', 'feat3', 'test', false);

        expect(feature).toEqual({ whenMs: now, exp: now + DEFAULT_FEATURE_TTL_MS, history: [], raw: 22, enc: null, stat: null });
    });

    it('should update a feature', async () => {
        const now = Date.now();

        let updateResult = await im.updateFeature('somesubject', '4711', 'feat3', null, 22, now, 'test');

        expect(updateResult).toEqual({ $hidx: -1, $feature: { whenMs: now, exp: now + DEFAULT_FEATURE_TTL_MS, history: [], raw: 22, enc: null, stat: null }});

        updateResult = await im.updateFeature('somesubject', '4711', 'feat3', null, 22, now, 'test');

        expect(updateResult).toBeNull();
    });

    it('should not save a feature in an instance, whose time to live is 0', async () => {
        const now = Date.now();

        await mm.registerFeature(DEFAULT_SEGMENT, 'foo.bar', { idx: 0, description: 'a', ttl: 0, encoding: { encoder: null, type: 'number' } }, DEFAULT_TYPE);

        let updateResult = await im.updateFeature('somesubject', '4711', 'foo.bar', null, 33, now, DEFAULT_TYPE);

        expect(updateResult).toEqual({ $hidx: -1, $feature: { whenMs: now, exp: now, history: [], raw: 33, enc: null, stat: null }});

        await expectAsync(im.getFeature('somesubject', '4711', 'foo.bar', DEFAULT_TYPE)).toBeRejectedWithError('Feature at index 0 is null and no default value was given');
    });

    it('should get all subjects', async () => {
        await im.updateFeature('somesubject1', '4711', 'feat3', null, 22, Date.now(), 'test');
        await im.updateFeature('somesubject2', '4711', 'feat3', null, 22, Date.now(), 'test');

        expect(im.getSubjects()).toEqual([ 'somesubject1', 'somesubject2' ]);
    });

    it('should update it\'s features', async () => {
        const now = Date.now();

        const updatePayload = {
            sender: im.uid,
            subject: 'somesubject',
            instanceId: '4711',
            feature: 'feat2',
            type: 'test',
            whenMs: now,
            enc: null,
            raw: 99
        };

        await im.__onFeatureUpdate(updatePayload);

        await expectAsync(im.getFeature('somesubject', '4711', 'feat2', 'test')).toBeRejected();

        updatePayload.sender = 'someotherim';

        await im.__onFeatureUpdate(updatePayload);

        const feature = await im.getFeature('somesubject', '4711', 'feat2', 'test');

        expect(feature).toEqual({ whenMs: now, exp: now + DEFAULT_FEATURE_TTL_MS, history: [], raw: 99, enc: null, stat: null });
    });
});