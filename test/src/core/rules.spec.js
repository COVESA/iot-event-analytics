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
const mock = require('mock-require');
const importFresh = require('import-fresh');

const {
    VALUE_TYPE_RAW,
    VALUE_TYPE_ENCODED,
    DEFAULT_SEGMENT,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE
} = require('../../../src/core/constants');

const {
    AndRules,
    OrRules,
    OpConstraint,
    ChangeConstraint,
    Rule,
    PATH_IDENTITY
} = require('../../../src/core/rules');

const FeatureMap = require('../../../src/core/util/featureMap');

describe('core.rules', () => {
    let InstanceManagerMock = null;
    let im = null;

    beforeAll(() => {
        mock('../../../src/core/util/logger', NullLogger);
        InstanceManagerMock = require('../../helpers/mock/instanceManager.mock');
    });

    afterAll(() => {
        mock.stop('../../../src/core/util/logger');
    });

    beforeEach(async () => {
        im = new InstanceManagerMock();
        await im.start(importFresh('../../resources/rules.types.json'));
    });

    describe('AndRules', () => {
        it('should evaluate to true, if all rules evaluate to true, else, false', async () => {
            const andRule = new AndRules([
                new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '', '.*', true)),
                new Rule(new OpConstraint('feat2', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ]);

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});

            await im.updateFeature('somesubject', '4711', 'feat2', null, 10, Date.now(), 'test');

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);

            featureMap.clear();
            featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});

            // Force the updated value to arrive at a later millisecond, otherwise it will be discarded
            await im.updateFeature('somesubject', '4711', 'feat2', null, 16, Date.now() + 1, 'test');

            await expectAsync(andRule.evaluate('somesubject', 'default', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
        });
    });

    describe('OrRules', () => {
        it('should evaluate to true, if one or more rules evaluate to true, else, false', async () => {
            const orRule = new OrRules([
                new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '', '.*', true)),
                new Rule(new OpConstraint('feat2', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ]);

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', { raw: 4, history: [] }, {});

            await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);

            featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});
            await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            await im.updateFeature('somesubject', '4711', 'feat1', null, 5, Date.now(), 'test');

            featureMap.clear();
            featureMap.set('test', 'feat2', '4711', { raw: 16, history: [] }, {});

            await expectAsync(orRule.evaluate('somesubject', 'test', 'feat', '4711', featureMap, im)).toBeResolvedTo(true);
        });
    });

    it('should evaluate rules using types wildcards', async () => {
        const orRule = new OrRules([
            new Rule(new OpConstraint('feat1', OpConstraint.OPS.ISSET, null, '100000.*', VALUE_TYPE_RAW))
        ]);

        const featureMap = new FeatureMap();
        featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});

        await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
    });

    it('should exclude specific instances', async () => {
        const orRule = new OrRules([
            new Rule(new OpConstraint('feat1', OpConstraint.OPS.GREATER_THAN_EQUAL, 5, 'test', VALUE_TYPE_RAW, PATH_IDENTITY, '^4712$'))
        ]);

        const featureMap = new FeatureMap();
        featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});
        await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);

        featureMap.set('test', 'feat1', '4712', { raw: 5, history: [] }, {});
        await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4712', featureMap, im)).toBeResolvedTo(true);
    });

    it('should evaluate rules on absolute value paths', async () => {
        const orRule = new OrRules([
            new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 'Hello', 'test', VALUE_TYPE_RAW, '/someMetaValue'))
        ]);

        const featureMap = new FeatureMap();
        featureMap.set('test', 'feat1', '4711', { raw: {
            $vpath: 'value',
            value: 5,
            someMetaValue: 'Hello'
        }, history: [] }, {});

        await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
    });

    it('should evaluate values at a given $vpath', async () => {
        const orRule = new OrRules([
            new Rule(new OpConstraint('feat1', OpConstraint.OPS.NEQUALS, 5, 'test', VALUE_TYPE_RAW))
        ]);

        const featureMap = new FeatureMap();
        featureMap.set('test', 'feat1', '4711', { raw: {
            $vpath: 'value',
            value: 6
        }, history: [] }, {});

        await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
    });

    it('should evaluate a value against a given regex', async () => {
        im.getMetadataManager().registerFeature(DEFAULT_SEGMENT, 'foo.bar', {
            description: 'a',
            encoding: {
                encoder: null,
                type: 'string'
            }
        }, DEFAULT_TYPE);

        const orRule = new OrRules([
            new Rule(new OpConstraint('foo.bar', OpConstraint.OPS.REGEX, '^Test.*', DEFAULT_TYPE, VALUE_TYPE_RAW))
        ]);

        const featureMap = new FeatureMap();
        featureMap.set(DEFAULT_TYPE, 'foo.bar', DEFAULT_INSTANCE, { raw: 'Testbar', history: [] }, {});

        await expectAsync(orRule.evaluate('somesubject', DEFAULT_TYPE, 'foo.bar', DEFAULT_INSTANCE, featureMap, im)).toBeResolvedTo(true);

        featureMap.set(DEFAULT_TYPE, 'foo.bar', DEFAULT_INSTANCE, { raw: 'Tetbar', history: [] }, {});

        await expectAsync(orRule.evaluate('somesubject', DEFAULT_TYPE, 'foo.bar', DEFAULT_INSTANCE, featureMap, im)).toBeResolvedTo(false);
    });

    it('should work on encoded values', async () => {
        const orRule = new OrRules([
            new Rule(new OpConstraint('feat1', OpConstraint.OPS.LESS_THAN_EQUAL, 0.5, 'test', VALUE_TYPE_ENCODED))
        ]);

        const featureMap = new FeatureMap();
        featureMap.set('test', 'feat1', '4711', { enc: 0.4, history: [] }, {});

        await expectAsync(orRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
    });

    it('should exclude rulesets on given typeFeatures', async () => {
        const andRule = new AndRules([
            new AndRules([
                new Rule(new OpConstraint('feat2', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ], [ 'test.feat1' ]),
            new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '', '.*', true))
        ]);

        const featureMap = new FeatureMap();

        featureMap.set('test', 'feat1', '4711', { raw: 4, history: [] }, {});

        await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);

        featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});
        await im.updateFeature('somesubject', '4711', 'feat1', null, 5, Date.now(), 'test');

        await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

        featureMap.clear();
        featureMap.set('test', 'feat2', '4711', { raw: 10, history: [] }, {});

        await expectAsync(andRule.evaluate('somesubject', 'test', 'feat2', '4711', featureMap, im)).toBeResolvedTo(false);

        featureMap.set('test', 'feat2', '4711', { raw: 16, history: [] }, {});

        await expectAsync(andRule.evaluate('somesubject', 'test', 'feat2', '4711', featureMap, im)).toBeResolvedTo(true);
    });

    it('should exclude rulesets using wildcards', async () => {
        im.getMetadataManager().registerFeature(DEFAULT_SEGMENT, 'foo.bar', {
            description: 'a',
            encoding: {
                encoder: null,
                type: 'number'
            }
        }, DEFAULT_TYPE);

        const andRule = new AndRules([
            new OrRules([
                new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW))
            ], [ 'default.foo.*' ]),
            new OrRules([
                new Rule(new OpConstraint('feat2', OpConstraint.OPS.EQUALS, 1, 'test', VALUE_TYPE_RAW))
            ], [ 'default.foo.bar' ]),
            new OrRules([
                new Rule(new OpConstraint('foo.bar', OpConstraint.OPS.EQUALS, 10, DEFAULT_TYPE, VALUE_TYPE_RAW))
            ], [ 'test.*' ])
        ]);

        const featureMap = new FeatureMap();
        featureMap.set('test', 'feat1', '4711', { raw: 5, history: [] }, {});

        im.updateFeature('somesubject', '4711', 'feat2', null, 1, Date.now(), 'test');

        await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
        featureMap.clear();
        featureMap.set(DEFAULT_TYPE, 'foo.bar', DEFAULT_INSTANCE, { raw: 10, history: [] }, {});
        await expectAsync(andRule.evaluate('somesubject', DEFAULT_TYPE, 'foo.bar', DEFAULT_INSTANCE, featureMap, im)).toBeResolvedTo(true);
    });

    it('should populate all features, if a feature wildcard selector is used and limitFeatureSelection is set to false', async () => {
        const andRule = new AndRules([
            new Rule(new OpConstraint('*', OpConstraint.OPS.LESS_THAN, 5, 'test', VALUE_TYPE_RAW, '', '.*', false))
        ]);

        await im.updateFeature('somesubject', '4711', 'feat1', 0.1, 4, Date.now(), 'test');
        await im.updateFeature('somesubject', '4711', 'feat2', 0.3, 10, Date.now(), 'test');
        await im.updateFeature('somesubject', '4711', 'feat3', 0.3, 15, Date.now(), 'test');

        const featureMap = new FeatureMap();
        await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

        expect(Object.keys((featureMap.dump()).test)).toEqual([ 'feat1', 'feat2', 'feat3' ]);
    });

    it('should save a valid JSON model', () => {
        const andRule = new AndRules([
            new AndRules([
                new Rule(new OpConstraint('feat2', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ], [ 'test.feat1' ]),
            new OrRules([
                new Rule(new OpConstraint('feat3', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '/foo')),
                new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ])
        ]);

        const Ajv = require('ajv');
        const validator = new Ajv().compile(require('../../../resources/rules.schema.json'));

        expect(validator(andRule.save())).toBeTruthy();
    });

    it('should return a list of unique typeFeatures', () => {
        const andRule = new AndRules([
            new AndRules([
                new Rule(new OpConstraint('feat2', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ], [ 'test.feat1' ]),
            new OrRules([
                new Rule(new OpConstraint('feat3', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '/foo')),
                new Rule(new OpConstraint('feat1', OpConstraint.OPS.EQUALS, 5, 'test', VALUE_TYPE_RAW, '', '.*', true))
            ])
        ]);

        const typeFeatures = andRule.getUniqueTypeFeatures();

        expect(typeFeatures).toContain({ type: 'test', feature: 'feat1', segment: null });
        expect(typeFeatures).toContain({ type: 'test', feature: 'feat2', segment: null });
        expect(typeFeatures).toContain({ type: 'test', feature: 'feat3', segment: null });
    });

    it('should iterate through all given rules', () => {
        const rules = new OrRules([
            new OrRules([
                new OrRules(new Rule(new OpConstraint('feat2', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW))),
                new Rule(new OpConstraint('feat1', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW))
            ]),
            new Rule(new OpConstraint('feat3', OpConstraint.OPS.GREATER_THAN, 15, 'test', VALUE_TYPE_RAW))
        ]);

        const singleRules = [...rules.forEach()];

        expect(singleRules.length).toBe(5);
    });

    it('should evaluate a change constraint', async () => {
        const rules = new OrRules([
            new Rule(new ChangeConstraint('feat1', 'test', VALUE_TYPE_RAW))
        ]);

        await im.updateFeature('somesubject', '4711', 'feat1', null, 10, Date.now(), 'test');
        await im.updateFeature('somesubject', '4711', 'feat1', null, 11, Date.now() + 1, 'test');

        await expectAsync(rules.evaluate('somesubject', 'test', 'feat1', '4711', new FeatureMap(), im)).toBeResolvedTo(true);

        await im.updateFeature('somesubject', '4711', 'feat1', null, 11, Date.now() + 2, 'test');

        await expectAsync(rules.evaluate('somesubject', 'test', 'feat1', '4711', new FeatureMap(), im)).toBeResolvedTo(false);
    });
});