/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const fs = require('fs');
const path = require('path');
const temp = require('temp');
const importFresh = require('import-fresh');
const NullLogger = require('../../helpers/null.logger');
const mock = require('mock-require');
const { prepareMockedMqttClient } = require('../../helpers/mock/mqtt.mock');
const ProtocolGateway = require('../../../src/core/protocolGateway');
const { MqttProtocolAdapter } = require('../../../src/core/util/mqttClient');
const { DEFAULT_SEGMENT, DEFAULT_TYPE } = require('../../../src/core/constants');
const {
    forceWaitOnSubscribe
} = require('../../helpers/protocolGateway.forcewait');

describe('core.metadataManager', () => {
    let mm = null;
    let clientMock = null;

    beforeAll(() => {
        mock('../../../src/core/util/logger', NullLogger);
    });

    afterAll(() => {
        mock.stop('../../../src/core/util/logger');
    });

    beforeEach(() => {
        temp.track();
        clientMock = prepareMockedMqttClient();
        clientMock.mConnect();
        mm = createMetadataManager();
    });

    afterEach(() => {
        temp.cleanupSync();
    });

    function writeVolatileTempFile(contents) {
        const path = temp.path();
        fs.writeFileSync(path, typeof contents === 'string' ? contents : JSON.stringify(contents));
        return path;
    }

    function createMetadataManager() {
        const MetadataManager = importFresh('../../../src/core/metadataManager');
        delete require.cache[require.resolve('../../../src/core/util/mqttClient')];
        return new MetadataManager(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(true, '')]));
    }

    function startAsMaster(mm, typesConfigPath = null) {
        return mm.startAsMaster(
            typesConfigPath || path.resolve(__dirname, '..', '..', 'resources', 'metadataManager.types.json'),
            path.resolve(__dirname, '..', '..', 'resources', 'metadataManager.uom.json')
        );
    }

    describe('Start as main', () => {
        it ('should send an update message to all other MetadataManagers', async () => {
            spyOn(mm, '__publishTypes').and.callThrough();
            spyOn(mm.pg, 'publishJson').and.callThrough();

            await startAsMaster(mm);

            expect(mm.__publishTypes).toHaveBeenCalled();
            await expectAsync(mm.ready).toBeResolved();

            expect(mm.pg.publishJson).toHaveBeenCalled();
            expect(mm.pg.publishJson.calls.argsFor(0)[2].retain).toBeTrue();
        });
    });

    describe('Start as worker', () => {
        it ('should receive an update message from the main instance', async () => {
            spyOn(clientMock, 'mOnSubscription').and.callThrough();

            forceWaitOnSubscribe(mm.pg);

            await mm.start();

            expect(clientMock.mOnSubscription.calls.mostRecent().args[0]).toBe('iotea/metadataManager/update');

            await new Promise(resolve => clientMock.mPublish('iotea/metadataManager/update', JSON.stringify({ version: 100, types: { foo: 'bar' } }), {}, () => resolve()));

            expect(mm.types).toEqual({ foo: 'bar' });
            expect(mm.version).toBe(100);
        });
    });

    it('should resolve a feature name at a given index', async () => {
        await startAsMaster(mm);
        // Invalid type
        await expectAsync(mm.resolveFeatureAt('invalidtype', 0)).toBeRejected();
        // Valid segment as type, but invalid index for segment
        await expectAsync(mm.resolveFeatureAt('100000', 2)).toBeRejected();
        // Invalid index
        await expectAsync(mm.resolveFeatureAt('test', 5)).toBeRejected();
        // Feature from segment
        await expectAsync(mm.resolveFeatureAt('100000', 0)).toBeResolvedTo('feat0');
        await expectAsync(mm.resolveFeatureAt('test', 0)).toBeResolvedTo('feat0');
        // Feature from specific type
        await expectAsync(mm.resolveFeatureAt('test', 1)).toBeResolvedTo('feat1');
    });

    it ('should get the full metadata of feature at a given index', async() => {
        await startAsMaster(mm);

        await expectAsync(mm.resolveMetaFeatureAt('test', 1)).toBeResolvedTo({
            idx: 1,
            description: '1',
            history: 20,
            encoding: {
                encoder: null,
                type: 'number'
            },
            $unit: null,
            unit: null
        });

        await expectAsync(mm.resolveMetaFeatureAt('test', 0)).toBeResolvedTo({
            idx: 0,
            description: '000',
            history: 5,
            encoding: {
                encoder: null,
                type: 'number'
            },
            unit: 'µA',
            $unit: {
                fac: 0.000001,
                unit: 'µA',
                desc: 'Mikroampere',
                base: {
                    fac: 1,
                    unit: 'A',
                    desc: 'Ampere'
                }
            }
        });
    });

    it ('should fail when requesting a non-existing data', async() => {
        await startAsMaster(mm);
        await expectAsync(mm.resolveMetaFeatureAt('foo', 1)).toBeRejectedWithError('Type foo cannot be resolved');
        // First looks into the indices of the type features, than it continues with the segment features
        await expectAsync(mm.resolveMetaFeatureAt('test', 6)).toBeRejectedWithError('Feature at index 6 of type 100000 does not exist');
        await expectAsync(mm.resolveMetaFeature('invalid', 'feat0')).toBeRejectedWithError('Type invalid cannot be resolved');
        await expectAsync(mm.resolveMetaFeature('test', 'invalid')).toBeRejectedWithError('The feature invalid of type test cannot be resolved');
    });

    it ('should resolve all defined types of a given segment', async () => {
        await startAsMaster(mm);
        await expectAsync(mm.resolveTypes('100000')).toBeResolvedTo([ 'test' ]);
        await expectAsync(mm.resolveTypes('test')).toBeRejectedWithError('test is a type, not a segment');
        await expectAsync(mm.resolveTypes('100001')).toBeRejectedWithError('Segment 100001 cannot be resolved');
    });

    it ('should resolve all features of a given type or segment', async () => {
        await startAsMaster(mm);
        await expectAsync(mm.resolveFeatures('100000')).toBeResolvedTo([ 'feat0' ]);

        let typeFeatures = await mm.resolveFeatures('test');

        expect(typeFeatures).toContain('feat0');
        expect(typeFeatures).toContain('feat1');
        expect(typeFeatures).toContain('feat2');
        expect(typeFeatures).toContain('feat3');

        await expectAsync(mm.resolveFeatures('invalid')).toBeRejectedWithError('Given type invalid could not be found');
    });

    it ('should resolve the segment for a given type', async () => {
        await startAsMaster(mm);
        await expectAsync(mm.resolveSegment('test')).toBeResolvedTo('100000');
        await expectAsync(mm.resolveSegment('invalid')).toBeRejectedWithError('Type invalid cannot be resolved');
        await expectAsync(mm.resolveSegment('100000')).toBeRejectedWithError('Segments can only be resolved from given types');
    });

    it ('should resolve all meta features for a given type', async () => {
        await startAsMaster(mm);

        const metaFeatures = await mm.resolveMetaFeatures('test');

        expect(Object.keys(metaFeatures).length).toBe(4);
        expect(Object.keys(metaFeatures)).toEqual([ 'feat1', 'feat2', 'feat3', 'feat0']);
    });

    it ('should return all given types', async () => {
        await startAsMaster(mm);

        await expectAsync(mm.getTypes()).toBeResolvedTo([ DEFAULT_TYPE, 'test', 'test2' ]);
    });

    it ('should export a typeMap', async () => {
        await startAsMaster(mm);

        const typeMap = await mm.getTypeMap();

        expect(Object.keys(typeMap)).toEqual([ DEFAULT_TYPE, 'test', 'test2' ]);
        expect(typeMap[DEFAULT_TYPE].segment).toBe(DEFAULT_SEGMENT);
        expect(typeMap[DEFAULT_TYPE].features).toEqual({});
        expect(typeMap['test'].segment).toBe('100000');
        expect(typeMap['test'].features).not.toEqual({});
    });

    it ('should get the update the version after each talent output feature registration', async () => {
        spyOn(clientMock, 'mPublish').and.callThrough();

        await startAsMaster(mm);

        expect(mm.getVersion()).toBe(0);

        await mm.registerTalentOutputFeatures({
            "talentId.foo": {
                description: "foo",
                encoding: {
                    encoder: null,
                    type: 'number'
                }
            },
            "bar": {
                description: "bar",
                encoding: {
                    encoder: null,
                    type: 'string'
                }
            }
        });

        expect(mm.getVersion()).toBe(1);

        const args = clientMock.mPublish.calls.mostRecent().args;

        expect(args[0]).toBe('iotea/metadataManager/update');
        expect(args[2]).toEqual({ retain: true });

        expect((await mm.resolveMetaFeature(DEFAULT_TYPE, 'bar')).description).toBe('bar');
        expect((await mm.resolveMetaFeature(DEFAULT_TYPE, 'talentId.foo')).description).toBe('foo');

        await mm.registerTalentOutputFeatures({
            "talentId.foo": {
                description: "foo",
                encoding: {
                    encoder: null,
                    type: 'number'
                }
            }
        });

        expect(mm.getVersion()).toBe(1);

        await expectAsync(mm.registerTalentOutputFeatures({
            "talentId.foo": {}
        })).toBeRejectedWithError('The type feature "000000".types."default".features."talentId.foo" must be a full feature.');

        expect(mm.getVersion()).toBe(1);
    });

    it ('should be able to validate incoming features', async () => {
        await startAsMaster(mm);

        await expectAsync(mm.__validateFeature('100000', 'baz', {}, null, true)).toBeRejectedWithError('The segment feature "100000".features."baz" must be a full feature.');
        await expectAsync(mm.__validateFeature('100000', 'baz', {}, 'test', true)).toBeRejectedWithError('The type feature "100000".types."test".features."baz" must be a full feature.');
        await expectAsync(mm.__validateFeature('100000', 'baz', { idx: "1" })).toBeRejectedWithError('Given feature index has to be of type number');
        await expectAsync(mm.__validateFeature('100000', 'baz', { idx: 42 })).toBeRejectedWithError('The segment feature "100000".features."baz" must be a fully indexed feature.');
        await expectAsync(mm.__validateFeature('100000', 'baz', { idx: 42 }, 'bla')).toBeRejectedWithError('The type feature "100000".types."bla".features."baz" must be a fully indexed feature. Remove the idx property, if you want to inherit from a segment feature.');

        // Register a new segment feature
        await mm.__registerFeature('100000', 'feat10', { idx: 22, description: 'How nice', encoding: { encoder: null, type: 'number' }});
        // Validation of other segment feature with different index should fail
        await expectAsync(mm.__validateFeature('100000', 'feat10', { idx: 44, description: '000', encoding: { encoder: null, type: 'string' }})).toBeRejectedWithError('Expect segment feature "100000".features."feat10" to have index 44. Found 22');
        // Validation of other segment feature with same index should resolve (Update scenario)
        await expectAsync(mm.__validateFeature('100000', 'feat10', { idx: 22, description: '0', encoding: { encoder: null, type: 'object' }})).toBeResolved();
        // Validation of other type feature, which inherits from the segment feature should fail
        await expectAsync(mm.__validateFeature('100000', 'feat10', { idx: 44, description: '000', encoding: { encoder: null, type: 'string' }}, 'test')).toBeRejectedWithError('Expected type feature "100000".types."test".features."feat10" to have index 22. Found 44');
        await expectAsync(mm.__validateFeature('100000', 'feat10', { description: 'aaa'}, 'test')).toBeResolved();
        // Validation of other segment feature with existing type feature with different index should fail
        await expectAsync(mm.__validateFeature('100000', 'feat1', { idx: 99, description: '000', encoding: { encoder: null, type: 'string' }})).toBeRejectedWithError('Expect type feature "100000".types."test".features."feat1" to have index 99. Found 1');
    });

    it('should throw errors in case of invalid types configuration on initialization', async () => {
        let typesConfigPath = writeVolatileTempFile({
            foo: 'bar'
        });

        await expectAsync(startAsMaster(mm, typesConfigPath)).toBeRejectedWithError(/^Validation\sof\stypes\sconfiguration\sfailed.\sErrors:.*$/);

        typesConfigPath = writeVolatileTempFile({
            "000000": {
                types: {
                    foo: {
                        features: {}
                    }
                },
                features: {}
            }
        });

        await expectAsync(startAsMaster(mm, typesConfigPath)).toBeRejectedWithError('Dynamic segment "000000" cannot be configured within the static types configuration');

        typesConfigPath = writeVolatileTempFile({
            "100000": {
                types: {
                    foo: {
                        features: {}
                    }
                },
                features: {}
            }
        });

        await expectAsync(startAsMaster(mm, typesConfigPath)).toBeRejectedWithError('Empty type foo found in segment 100000');

        typesConfigPath = writeVolatileTempFile({
            "100000": {
                types: {
                    foo: {
                        features: {
                            bla: {
                                idx: 0,
                                description: '0',
                                encoding: {
                                    enncoder: null,
                                    type: 'number'
                                }
                            }
                        }
                    }
                },
                features: {}
            },
            "200000": {
                types: {
                    foo: {
                        features: {}
                    }
                },
                features: {}
            }
        });

        await expectAsync(startAsMaster(mm, typesConfigPath)).toBeRejectedWithError('Duplicate type foo found in segment 200000');
    });
});