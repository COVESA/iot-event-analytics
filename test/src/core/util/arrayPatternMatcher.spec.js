/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

 const {
    Wildcard,
    ArrayPatternMatcher
} = require('../../../../src/core/util/arrayPatternMatcher');

describe('core.util.arrayPatternMatcher', () => {
    const input = [ 1, 2, 3, 5, 9, 10, 20, 25, 33, 1, 3, 20, 19, 3, 7, 9, 130, 99, 2, 2 ];

    function booleanMatch(pattern, data = input) {
        return new ArrayPatternMatcher(pattern).booleanMatch(data);
    }

    it('should match wildcards only', () => {
        expect(booleanMatch([ new Wildcard(), new Wildcard().accept(2), new Wildcard().minValues(2), new Wildcard(), new Wildcard() ])).toBeTruthy();
    });

    it('should match empty wildcards', () => {
        expect(booleanMatch([ 20, new Wildcard(), 19 ])).toBeTruthy();
    });

    it('should check for minimum value count', () => {
        expect(booleanMatch([ new Wildcard().minValues(3) ])).toBeTruthy();
    });

    it('should check for static numbers only', () => {
        expect(booleanMatch([ 5, 9, 10 ])).toBeTruthy();
    });

    it('should check for mixed static numbers and wildcards', () => {
        expect(booleanMatch([ new Wildcard(), 1, new Wildcard(), 2 ])).toBeTruthy();
        expect(booleanMatch([ 3, new Wildcard() ])).toBeTruthy();
    });

    it('should reject certain mumbers', () => {
        expect(booleanMatch([ 3, new Wildcard().reject(5), 19 ])).toBeTruthy();
        expect(booleanMatch([ 10, new Wildcard(), 20, new Wildcard().reject(3), 19 ])).toBeTruthy();
    });

    it('should check for minimum and maximum value count', () => {
        expect(booleanMatch([ 3, 5, new Wildcard().minValues(3).maxValues(5), 1])).toBeTruthy();
    });

    it('should check for minimum, maximum value count and reject given numbers', () => {
        expect(booleanMatch([ 3, 5, new Wildcard().minValues(3).maxValues(6).reject(26), 3])).toBeTruthy();
    });

    it('should check an empty pattern', () => {
        expect(booleanMatch([])).toBeFalsy();
    });

    it('should not match due to number rejection', () => {
        expect(booleanMatch([ 3, 5, new Wildcard().minValues(3).maxValues(6).reject(25), 3])).toBeFalsy();
    });

    it('should not match due to exceeding max value count', () => {
        expect(booleanMatch([ 3, 5, new Wildcard().minValues(3).maxValues(5), 3])).toBeFalsy();
    });

    it('should throw exception if pattern is not an array', () => {
        expect(() => ArrayPatternMatcher.fromJson({})).toThrow();
    });

    it('should parse a given pattern from Json', () => {
        const pattern = [
            {
                nmin: 1,
                nmax: 1,
                accepts: [ 1 ]
            },
            {
                nmin: 1,
                nmax: 1,
                rejects: [ 2 ]
            },
            {
                nmin: 2,
                nmax: 3,
                accepts: [ 6, 5 ]
            }
        ];

        expect(ArrayPatternMatcher.fromJson(pattern).booleanMatch([ 8, 9, 1, 8, 6, 5, 6, 0 ])).toBeTruthy();
        expect(ArrayPatternMatcher.fromJson(pattern).booleanMatch([ 8, 9, 1, 2, 5, 6, 5, 10 ])).toBeFalsy();
    });
});