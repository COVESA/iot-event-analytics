/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const importFresh = require('import-fresh');
const clone = require('../../../../src/core/util/clone');

describe('core.util.clone', () => {
    let json = null;

    beforeEach(() => {
        json = importFresh('../../../resources/clone.input.json');
    });

    it('should clone all fields of an object', () => {
        const expectedKeys = Object.keys(json);
        const clonedKeys = Object.keys(clone(json));

        expect(expectedKeys.length).toBe(clonedKeys.length);

        for (let i = 0; i < expectedKeys.length; i++) {
            expect(clonedKeys).toContain(expectedKeys[i]);
        }
    });

    it('should clone a complex object', () => {
        expect(clone(json)).toEqual(json);
    });

    it('should exclude a given key', () => {
        // eslint-disable-next-line no-unused-vars
        const { foo, ...expected } = json;

        expect(clone(json, key => key, key => key === 'foo')).toEqual(expected);
    });
});