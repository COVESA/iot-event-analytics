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
const equals = require('../../../../src/core/util/equals');

describe('core.util.clone', () => {
    const relInputPath = '../../../resources/equals.input.json';
    let json = null;

    beforeEach(() => {
        json = importFresh(relInputPath);
    });

    it('Should be able to test for deep equality', () => {
        const json2 = importFresh(relInputPath);

        expect(equals(json, json2)).toBeTruthy();
    });

    it('should be able to test equality of arrays despite different order', () => {
        const a1 = [ 1, 2, 3, 4, 5 ]
        const a2 = [ 5, 2, 4, 3, 1 ]
        const a3 = [ 5, 2, 4, 6, 1 ]

        expect(equals(a1, a2, { strictArrayOrder: false} )).toBeTruthy();
        expect(equals(a1, a3, { strictArrayOrder: false} )).toBeFalsy();
    });

    it('should return false on wrong comparisons', () => {
        expect(equals([1, 2], [1, 3])).toBeFalsy();
        expect(equals([1, 2, 3], [1, 3, 2])).toBeFalsy();
        expect(equals([1, 2, 5], [1, 2])).toBeFalsy();
        expect(equals({ foo: "bar" }, { foo: "bar", bar: "baz" })).toBeFalsy();
        expect(equals({ foo: "bar" }, { bar: "baz" })).toBeFalsy();
        expect(equals({ foo: [ 1, 2, 3 ] }, { foo: "baz" })).toBeFalsy();
    });
});