/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const jsonQuery = require('../../../../src/core/util/jsonQuery');

describe('core.util.jsonQuery', () => {
    let json = null;

    beforeEach(() => {
        json = {
            foo: [
                {
                    test: 1
                },
                {
                    test: {
                        bla: 1234
                    }
                },
                {
                    test: 3
                }
            ],
            bar: {
                baz: 'Hello World',
                'bla.blubb': 555
            },
            baz: 42,
            test: {
                "a": { "value": "a" },
                "b": { "value": "b" },
                "c": { "value": "c" },
                "d": { "value": "d" }
            },
            py: [ 1, 2, 3, 4 ]
        };
    });

    it('should return the value identity', () => {
        const results = Array.from(jsonQuery(json, ''));
        const results2 = Array.from(jsonQuery(json));

        expect(results.length).toBe(1);
        expect(results[0].label).toBeNull();
        expect(results[0].query).toBe('');
        expect(results[0].value).toEqual(json);
        expect(results).toEqual(results2);
    });

    it('should return a label', () => {
        const results = Array.from(jsonQuery(json, ':foo'));

        expect(results[0].label).toBe('foo');
    });

    it('should get a nested value', () => {
        const results = Array.from(jsonQuery(json, 'bar.baz'));

        expect(results.length).toBe(1);
        expect(results[0].value).toBe('Hello World');
    });

    it('should get all fields of a given object', () => {
        const results = Array.from(jsonQuery(json, '*'));

        expect(results.length).toBe(5);

        for(let i = 0; i < results.length; i++) {
            // Replace the '' around the query, since we have an object
            expect(results[i].value).toBe(json[results[i].query]);
        }
    });

    it('should get a field of all nested object', () => {
        const results = Array.from(jsonQuery(json, 'test.*.value'));

        expect(results.length).toBe(4);

        let expectedQueries = [
            'test.a.value',
            'test.b.value',
            'test.c.value',
            'test.d.value'
        ];

        for(let i = 0; i < results.length; i++) {
            expect(expectedQueries.indexOf(results[i].query)).toBeGreaterThan(-1);
        }
    });

    it('should be able to retrieve an element at a given array index', () => {
        const results = Array.from(jsonQuery(json, 'foo[1].test.bla'));

        expect(results.length).toBe(1);
        expect(results[0].value).toBe(1234);
    });

    it('should be able to retrieve all array items', () => {
        const results = Array.from(jsonQuery(json, 'foo[:]'));

        expect(results.length).toBe(3);

        for (let i = 0; i < results.length; i++) {
            expect(results[i].value).toEqual(json.foo[i]);
        }
    });

    it('should be able to retrieve an specific array item', () => {
        const results = Array.from(jsonQuery(json, 'foo[1]'));

        expect(results[0].value).toEqual(json.foo[1]);
    });

    it('should be able to retrieve the last item of an array', () => {
        const results = Array.from(jsonQuery(json, 'foo[-1]'));

        expect(results[0].value).toEqual(json.foo[json.foo.length - 1]);
    });

    it('should be able to retrieve a range of items in an array', () => {
        const results = Array.from(jsonQuery(json, 'foo[1:3]'));

        expect(results.length).toBe(2);
        expect(results[0].value).toEqual(json.foo[1]);
        expect(results[1].value).toEqual(json.foo[2]);
    });

    it('should behave like the python array range selector', () => {
        expect(Array.from(jsonQuery(json, 'py[1:]')).map(m => m.value)).toEqual([ 2, 3, 4 ]);
        expect(Array.from(jsonQuery(json, 'py[:1]')).map(m => m.value)).toEqual([ 1 ]);
        expect(Array.from(jsonQuery(json, 'py[3:0]')).map(m => m.value)).toEqual([ ]);
        expect(Array.from(jsonQuery(json, 'py[:-1]')).map(m => m.value)).toEqual([ 1, 2, 3 ]);
        expect(Array.from(jsonQuery(json, 'py[-3:-1]')).map(m => m.value)).toEqual([ 2, 3 ]);
        expect(Array.from(jsonQuery(json, 'py[-1:]')).map(m => m.value)).toEqual([ 4 ]);
        expect(Array.from(jsonQuery(json, 'py[0:0]')).map(m => m.value)).toEqual([ ]);
        expect(() => Array.from(jsonQuery(json, 'py[0:5]'))).toThrowError('Index 5 is out of bounds');
    });

    it('should be able to accept fields containing a dot character', () => {
        const results = Array.from(jsonQuery(json, "bar.'bla.blubb'"));

        expect(results.length).toBe(1);
        expect(results[0].value).toEqual(json.bar['bla.blubb']);
    });

    describe('.first', () => {
        it('should be able to return only the first search result', () => {
            const result = jsonQuery.first(json, 'foo[:]');

            expect(result.value).toEqual(json.foo[0]);
        });
    });

    describe('.updateFirst', () => {
        it('should be able to update the first search result', () => {
            jsonQuery.updateFirst(json, 'foo[:]', 'huhuhu');

            expect(json.foo[0]).toBe('huhuhu');
        });
    });

    describe('.updateAll', () => {
        it('should be able to update all search result', () => {
            jsonQuery.updateAll(json, 'foo[:]', {
                foo: [
                    1, 2, 3
                ]
            })

            expect(json.foo).toEqual([1, 2, 3]);
        });
    });
});