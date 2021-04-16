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
const Instance = require('../../../src/core/instance');
const { DEFAULT_FEATURE_TTL_MS } = require('../../../src/core/constants');

describe('core.instance', () => {
    function createInstance(type, instanceId) {
        return new Instance(type, instanceId, new NullLogger());
    }

    it('should get given features', () => {
        const nowMs = 1618394584450;
        const i = createInstance('test', '4711');

        expect(i.getFeatures()).toEqual([]);
        expect(() => i.getFeatureAt(0)).toThrowError('Feature at index 0 is null and no default value was given');
        expect(i.getFeatureAt(0, 'test')).toEqual({ whenMs: -1, exp: -1, history: [], raw: 'test', enc: null, stat: null });

        i.updateFeatureAt(2, null, 'test', nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);

        expect(i.getFeatureAt(2)).toEqual({ whenMs: nowMs, exp: nowMs + DEFAULT_FEATURE_TTL_MS, history: [], raw: 'test', enc: null, stat: null });

        expect(() => i.getFeatureAt('2')).toThrowError('Given index has to be a number greater than 0. Got 2:string');
    });

    it('should update a feature', () => {
        const nowMs = 1618394584450;
        const i = createInstance('test', '4711');

        expect(i.updateFeatureAt(2, null, 'test', undefined, 0, DEFAULT_FEATURE_TTL_MS, nowMs)).toBeNull();

        expect(i.updateFeatureAt(2, null, 'test', nowMs, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toEqual({
            $hidx: -1,
            $feature: { whenMs: nowMs, exp: nowMs + DEFAULT_FEATURE_TTL_MS, history: [], raw: 'test', enc: null, stat: null }
        });

        expect(i.updateFeatureAt(2, null, 'test', nowMs + 2, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toEqual({
            $hidx: -1,
            $feature: { whenMs: nowMs + 2, exp: nowMs + DEFAULT_FEATURE_TTL_MS + 2, history: [{
                whenMs: nowMs, exp: nowMs + DEFAULT_FEATURE_TTL_MS, raw: 'test', enc: null
            }], raw: 'test', enc: null, stat: null }
        });

        expect(i.updateFeatureAt(2, null, 'test', nowMs + 1, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toEqual({
            $hidx: 0,
            $feature: { whenMs: nowMs + 2, exp: nowMs + DEFAULT_FEATURE_TTL_MS + 2, history: [{
                whenMs: nowMs + 1, exp: nowMs + DEFAULT_FEATURE_TTL_MS + 1, raw: 'test', enc: null
            }], raw: 'test', enc: null, stat: null }
        });

        expect(i.updateFeatureAt(2, null, 'test', nowMs + 1, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toBeNull();
        expect(i.updateFeatureAt(2, null, 'test', nowMs + 2, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toBeNull();

        // Updated feature should have actually being pruned, since it's older than the oldest history entry whenMs - the ttl
        expect(i.updateFeatureAt(2, null, 'test', nowMs - DEFAULT_FEATURE_TTL_MS - 1, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toBeNull();

        // Update an already outdated non-existing feature
        expect(i.updateFeatureAt(4, null, 'test', nowMs - DEFAULT_FEATURE_TTL_MS - 1, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toBeNull();
    });

    it('should calculate statistical data', () => {
        const nowMs = 1618394584450;
        const i = createInstance('test', '4711');

        i.updateFeatureAt(0, 0.1, 1, nowMs, 3, DEFAULT_FEATURE_TTL_MS, nowMs);

        expect(i.getFeatureAt(0).stat).toEqual({ cnt: 1, mean: 0.1, var: null, sdev: null });

        i.updateFeatureAt(0, 0.3, 3, nowMs + 1, 3, DEFAULT_FEATURE_TTL_MS, nowMs + 1);

        expect(i.getFeatureAt(0).stat).toEqual({ cnt: 2, mean: 0.2, var: 0.019999999999999993, sdev: 0.14142135623730948 });

        i.updateFeatureAt(0, 0.5, 5, nowMs + 2, 3, DEFAULT_FEATURE_TTL_MS, nowMs + 2);

        expect(i.getFeatureAt(0).stat).toEqual({ cnt: 3, mean: 0.3, var: 0.039999999999999994, sdev: 0.19999999999999998 });

        i.updateFeatureAt(0, 0.7, 7, nowMs + 3, 3, DEFAULT_FEATURE_TTL_MS, nowMs + 3);

        expect(i.getFeatureAt(0).stat).toEqual({ cnt: 4, mean: 0.39999999999999997, var: 0.06666666666666665, sdev: 0.2581988897471611 });

        expect(i.getFeatureAt(0).history.length).toBe(3);
    });

    it('should be able to take encoded values from a given $vpath', () => {
        const nowMs = 1618394584450;
        const i = createInstance('test', '4711');

        i.updateFeatureAt(0, {
            $vpath: 'value',
            value: 0.1
        }, {
            $vpath: 'value',
            value: 10
        }, nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);

        expect(i.getFeatureAt(0).stat).toEqual({ cnt: 1, mean: 0.1, var: null, sdev: null });
    });

    it('should prune outdated features', () => {
        const nowMs = 1618394584450;
        const i = createInstance('test', '4711');
        // Set it one milliseconds before pruning
        i.updateFeatureAt(0, null, 10, nowMs - DEFAULT_FEATURE_TTL_MS + 1, 0, DEFAULT_FEATURE_TTL_MS, nowMs);
        i.prune(nowMs + 2);

        expect(() => i.getFeatureAt(0)).toThrowError('Feature at index 0 is null and no default value was given');
    });

    it('should process partial values', () => {
        const nowMs = 1618394584450;
        const i = createInstance('test', '4711');

        i.updateFeatureAt(0, null, [ null, null, null ], nowMs, 1, DEFAULT_FEATURE_TTL_MS, nowMs);

        i.updateFeatureAt(0, null, {
            $part: 1,
            value: 10
        }, nowMs, 1, DEFAULT_FEATURE_TTL_MS, nowMs);

        expect(i.getFeatureAt(0)).toEqual({ whenMs: nowMs, exp: nowMs + DEFAULT_FEATURE_TTL_MS, history: [], raw: [ null, 10, null ], enc: null, stat: null });

        expect(() => i.updateFeatureAt(0, null, {
            $part: 3,
            value: 10
        }, nowMs, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toThrowError('Invalid partial index given 3');

        expect(() => i.updateFeatureAt(1, null, {
            $part: 3,
            value: 10
        }, nowMs, 1, DEFAULT_FEATURE_TTL_MS, nowMs)).toThrowError('Partial results need a target feature of type Array.');
    });
});