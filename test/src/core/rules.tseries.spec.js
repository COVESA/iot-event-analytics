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
    VALUE_TYPE_RAW
} = require('../../../src/core/constants');

const {
    AndRules,
    Rule,
    PATH_IDENTITY
} = require('../../../src/core/rules');

const {
    TimeseriesConstraint,
    TimeseriesPatternConstraint
} = require('../../../src/core/rules.tseries');

const FeatureMap = require('../../../src/core/util/featureMap');

class TestConstraint extends TimeseriesConstraint {
    constructor(path) {
        super('feat1', 99, null, 'test', VALUE_TYPE_RAW, 3, path);
    }

    __evaluate() {
        return true;
    }
}

describe('core.rules.tseries', () => {
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

    describe('TimeseriesPatternConstraint', () => {
        it('should evaluate once a mimimum value count is reached', async () => {
            const andRule = new AndRules([
                new Rule(new TestConstraint(PATH_IDENTITY))
            ]);

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', { raw: 3, history: [ { raw: 2 } ] }, {});

            // Minimum 3 values are required
            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);

            featureMap.set('test', 'feat1', '4711', { raw: 3, history: [
                { raw: 2 }, { raw: 1 }
            ] }, {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);
        });

        it ('should evaluate values at absolute paths and values at $vpath', async () => {
            const tc = new TestConstraint('/foo');

            spyOn(tc, '__evaluate');

            const featureMap = new FeatureMap();

            let andRule = new AndRules([ new Rule(tc) ]);

            featureMap.set('test', 'feat1', '4711', {
                raw: {
                    foo: '1', $vpath: 'value', value: 'Ho'
                },
                history: [
                    { raw: { foo: '2', $vpath: 'value', value: 'Hi' } },
                    { raw: { foo: '3', $vpath: 'value', value: 'Ho' } }
                ]
            }, {});

            await andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im);

            expect(tc.__evaluate.calls.mostRecent().args[0]).toEqual([ '1', '2', '3' ]);

            // Reset it, so it now reads from $vpath
            tc.path = PATH_IDENTITY;
            tc.options.isAbsolutePath = false;

            await andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im);

            expect(tc.__evaluate.calls.mostRecent().args[0]).toEqual([ 'Ho', 'Hi', 'Ho' ]);
        });

        it('should evaluate to true, if the given pattern evaluates to true', async () => {
            const andRule = new AndRules([
                new Rule(new TimeseriesPatternConstraint('feat1', [ 1, 2, 3 ], 'test', VALUE_TYPE_RAW))
            ]);

            const featureMap = new FeatureMap();
            featureMap.set('test', 'feat1', '4711', { raw: 3, history: [ { raw: 2 }, { raw: 1 }, { raw: 0 } ] }, {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(true);

            featureMap.set('test', 'feat1', '4711', { raw: 3, history: [ { raw: 2 } ] }, {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);

            // The constraint is only evaluated to true, if the most recent value is the beginning of the pattern matched
            featureMap.set('test', 'feat1', '4711', { raw: 5, history: [ { raw: 3 }, { raw: 2 }, { raw: 1 } ] }, {});

            await expectAsync(andRule.evaluate('somesubject', 'test', 'feat1', '4711', featureMap, im)).toBeResolvedTo(false);
        });
    });
});