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
    DEFAULT_FEATURE_TTL_MS
} = require('../../../src/core/constants');

const {
    AndRules,
    OrRules,
    Rule
} = require('../../../src/core/rules');

const {
    NelsonConstraint,
    NelsonAlterConstraint,
    NelsonBiasConstraint,
    NelsonHighDevConstraint,
    NelsonLowDevConstraint,
    NelsonOut1SeConstraint,
    NelsonOut2SeConstraint,
    NelsonOut3SeConstraint,
    NelsonTrendConstraint
} = require('../../../src/core/rules.nelson');

const Instance = require('../../../src/core/instance');

const FeatureMap = require('../../../src/core/util/featureMap');

describe('core.rules.nelson', () => {
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
        im = new InstanceManagerMock(importFresh('../../resources/rules.types.tseries.json'));
        await im.start();
    });

    function createRuleFromConstraint(constraint) {
        return new Rule(NelsonConstraint.fromJson(new Rule(constraint).save()));
    }

    describe('NelsonOutSeConstraint', () => {
        it('', async () => {
            const orRules = new OrRules([
                createRuleFromConstraint(new NelsonOut1SeConstraint('feat1', 'test')),
                createRuleFromConstraint(new NelsonOut2SeConstraint('feat2', 'test')),
                createRuleFromConstraint(new NelsonOut3SeConstraint('feat3', 'test'))
            ]);

            const nowMs = 1618411442912;
            const instance = new Instance('test', '4711');

            for (let i = 0; i < 100; i ++) {
                const encodedUpperValue = i % 2 === 0 ? 0.1 : 0.3;
                const encodedLowerValue = i % 2 === 0 ? 0.9 : 0.7;
                // 4 out of 5 exceed mean + 1 x sdev >> 4 values are needed
                instance.updateFeatureAt(0, i < 100 - 4 ? encodedUpperValue : 0.5, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                // 2 out of 3 exceed mean + 2 x sdev >> 2 values are needed
                instance.updateFeatureAt(1, i < 100 - 2 ? encodedUpperValue : 0.5, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                // 1 out of 1 exceeds mean + 3 x sdev >> 1 value is needed
                instance.updateFeatureAt(2, i < 100 - 1 ? encodedUpperValue : 1, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                // 4 out of 5 is lower than mean - 1 x sdev >> 4 values are needed
                instance.updateFeatureAt(3, i < 100 - 4 ? encodedLowerValue : 0.5, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                // 2 out of 3 is lower than mean - 2 x sdev >> 2 values are needed
                instance.updateFeatureAt(4, i < 100 - 2 ? encodedLowerValue : 0.5, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                // 1 out of 1 is lower than mean - 3 x sdev >> 1 value is needed
                instance.updateFeatureAt(5, i < 100 - 1 ? encodedLowerValue : 0, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
            }

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(0), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(3), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            featureMap.clear();
            featureMap.set('test', 'feat2', '4711', instance.getFeatureAt(1), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat2', '4711', featureMap, im)).toBeResolvedTo(true);
            featureMap.set('test', 'feat2', '4711', instance.getFeatureAt(4), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat2', '4711', featureMap, im)).toBeResolvedTo(true);

            featureMap.clear();
            featureMap.set('test', 'feat3', '4711', instance.getFeatureAt(2), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat3', '4711', featureMap, im)).toBeResolvedTo(true);
            featureMap.set('test', 'feat3', '4711', instance.getFeatureAt(5), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat3', '4711', featureMap, im)).toBeResolvedTo(true);
        });
    });

    describe('NelsonDevConstraint', () => {
        it('', async () => {
            const orRules = new OrRules([
                createRuleFromConstraint(new NelsonLowDevConstraint('feat1', 'test')),
                createRuleFromConstraint(new NelsonHighDevConstraint('feat2', 'test'))
            ]);

            const nowMs = 1618411442912;
            const instance = new Instance('test', '4711');

            for (let i = 0; i < 100; i ++) {
                const encodedValue = i % 2 === 0 ? 0.25 : 0.75;
                instance.updateFeatureAt(0, i < 85 ? encodedValue : 0.5, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                instance.updateFeatureAt(1, i < 85 ? encodedValue : 1, 0, nowMs + i, 99, DEFAULT_FEATURE_TTL_MS, nowMs + i);
            }

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(0), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            featureMap.clear();
            featureMap.set('test', 'feat2', '4711', instance.getFeatureAt(1), {});
            await expectAsync(orRules.evaluate('somesubject', 'test', 'feat2', '4711', featureMap, im)).toBeResolvedTo(true);
        });
    });

    describe('NelsonTrendConstraint', () => {
        it('should evaluate to true, if 6 succeeding values are monotonous rising or falling', async () => {
            const andRule = new AndRules([
                createRuleFromConstraint(new NelsonTrendConstraint('feat1', 'test'))
            ]);

            const nowMs = 1618411442912;
            const instance = new Instance('test', '4711');

            for (let i = 0; i < 7; i ++) {
                let enc = 0.1 * i;
                instance.updateFeatureAt(0, enc, enc * 10, nowMs + i, 10, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                instance.updateFeatureAt(1, 1 - enc, enc * 10, nowMs + i, 10, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                instance.updateFeatureAt(2, i % i == 0 ? enc : 1 - enc, enc * 10, nowMs + i, 10, DEFAULT_FEATURE_TTL_MS, nowMs + i);
            }

            const featureMap = new FeatureMap();

            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(0), {});
            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(1), {});
            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(2), {});
            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);
        });
    })

    describe('NelsonAlterConstraint', () => {
        it('should evaluate to true, an alternating pattern around the mean value is detected', async () => {
            const andRule = new AndRules([
                createRuleFromConstraint(new NelsonAlterConstraint('feat1', 'test'))
            ]);

            const nowMs = 1618411442912;
            const instance = new Instance('test', '4711');

            for (let i = 0; i < 14; i += 2) {
                instance.updateFeatureAt(0, 0.9, 9, nowMs + i, 14, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                instance.updateFeatureAt(0, 0.1, 1, nowMs + i + 1, 14, DEFAULT_FEATURE_TTL_MS, nowMs + i + 1);
            }

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(0), {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            instance.updateFeatureAt(0, 0.1, 1, nowMs + 15, 14, DEFAULT_FEATURE_TTL_MS, nowMs + 15);
            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(0), {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);
        });
    });

    describe('NelsonBiasConstraint', () => {
        it('should evaluate to true, once a biased timeseries above or below mean is detected', async () => {
            const andRule = new AndRules([
                createRuleFromConstraint(new NelsonBiasConstraint('feat1', 'test'))
            ]);

            const nowMs = 1618411442912;
            const instance = new Instance('test', '4711');

            // 10 times 1, 10 times 0.9
            // Biased below mean, which is > 0.9
            for (let i = 0; i < 20; i ++) {
                instance.updateFeatureAt(0, i < 10 ? 1 : 0.9, 0, nowMs + i, 20, DEFAULT_FEATURE_TTL_MS, nowMs + i);
            }

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(0), {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            // 10 times 0.1, 10 times 0.5
            // Biased above mean, which is < 0.5
            for (let i = 0; i < 20; i ++) {
                instance.updateFeatureAt(1, i < 10 ? 0.1 : 0.5, 0, nowMs + i, 20, DEFAULT_FEATURE_TTL_MS, nowMs + i);
            }

            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(1), {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            for (let i = 0; i < 14; i += 2) {
                instance.updateFeatureAt(2, 0.9, 9, nowMs + i, 14, DEFAULT_FEATURE_TTL_MS, nowMs + i);
                instance.updateFeatureAt(2, 0.1, 1, nowMs + i + 1, 14, DEFAULT_FEATURE_TTL_MS, nowMs + i + 1);
            }

            featureMap.set('test', 'feat1', '4711', instance.getFeatureAt(2), {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);
        });
    });
});