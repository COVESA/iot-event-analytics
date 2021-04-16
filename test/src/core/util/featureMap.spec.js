/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const FeatureMap = require('../../../../src/core/util/featureMap');

describe('core.util.featureGraph', () => {
    let fm = null;

    beforeEach(() => {
        fm = new FeatureMap();
    });

    it('should set a feature', () => {
        fm.set('default', 'test', '4711', { foo: 1 }, { bar: 2 });

        expect(fm.contains('default', 'test', '4711')).toBeTruthy();
        expect(fm.contains('default', 'test', '4712')).toBeFalsy();
    });

    it('should get an existing feature', () => {
        fm.set('default', 'test', '4711', { foo: 1 }, { bar: 2 });

        expect(fm.get('default', 'test', '4711').$feature).toEqual({ foo: 1 });
    });

    it('should overwrite an existing feature', () => {
        fm.set('default', 'test', '4711', { foo: 1 }, { bar: 2 });
        fm.set('default', 'test', '4711', { foo: 2 }, { bar: 2 });

        expect(fm.get('default', 'test', '4711').$feature).toEqual({ foo: 2 });
    });

    it('should record a match', () => {
        fm.set('default', 'test', '4711', { foo: 1 }, { bar: 2 });
        fm.recordRuleMatchFor('default', 'test', '4711');

        expect(fm.get('default', 'test', '4711').matches).toBe(1);
    });

    it('should store the metadata on a type.feature level', () => {
        fm.set('default', 'test', '4711', { foo: 1 }, { bar: 2 });
        const dump = fm.dump();

        // $metadata for feature is stored on type.feature level
        expect(dump.default.test.$metadata).toEqual({ bar: 2 });
    });
});