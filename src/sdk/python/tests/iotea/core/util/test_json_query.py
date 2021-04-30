##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import pytest
from unittest import TestCase
from src.iotea.core.util.json_query import json_query, json_query_first, json_query_update_first, json_query_update_all

@pytest.fixture
def test_case():
    return TestCase()

@pytest.fixture
def json():
    return {
        'foo': [
            {
                'test': 1
            },
            {
                'test': {
                    'bla': 1234
                }
            },
            {
                'test': 3
            }
        ],
        'bar': {
            'baz': 'Hello World',
            'bla.blubb': 555
        },
        'baz': 42,
        'test': {
            'a': { 'value': 'a' },
            'b': { 'value': 'b' },
            'c': { 'value': 'c' },
            'd': { 'value': 'd' }
        },
        'py': [ 1, 2, 3, 4 ]
    }

class TestJsonQuery:
    def test_should_return_value_identity(self, test_case, json):
        results = json_query(json, '')
        results2 = json_query(json)

        assert len(results) == 1
        assert results[0]['label'] == None
        assert results[0]['query'] == ''
        test_case.assertDictEqual(results[0]['value'], json)
        test_case.assertDictEqual(results[0], results2[0])

    def test_return_label(self, json):
        results = json_query(json, ':foo')

        assert results[0]['label'] == 'foo'

    def test_get_nested_value(self, json):
        results = json_query(json, 'bar.baz')

        assert len(results) == 1
        assert results[0]['value'] == 'Hello World'

    def test_get_all_fields_of_object(self, json):
        results = json_query(json, '*')

        assert len(results) == 5

        for i in range(len(results)):
            assert results[i]['value'] == json[results[i]['query']]

    def test_get_a_field_of_all_nested_objects(self, json):
        results = json_query(json, 'test.*.value')

        assert len(results) == 4

        expected_queries = [
            'test.a.value',
            'test.b.value',
            'test.c.value',
            'test.d.value'
        ]

        for i in range(len(results)):
            expected_queries.index(results[i]['query'])

    def test_get_element_at_given_index(self, json):
        results = json_query(json, 'foo[1].test.bla')

        assert len(results) == 1
        assert results[0]['value'] == 1234

    def test_get_all_array_items(self, json, test_case):
        results = json_query(json, 'foo[:]')

        assert len(results) == 3

        for i in range(len(results)):
            test_case.assertDictEqual(results[i]['value'], json['foo'][i])

    def test_get_specific_array_item(self, json, test_case):
        results = json_query(json, 'foo[1]')

        assert len(results) == 1
        test_case.assertDictEqual(results[0]['value'], json['foo'][1])

    def test_get_last_array_item(self, json, test_case):
        results = json_query(json, 'foo[-1]')
        test_case.assertDictEqual(results[0]['value'], json['foo'][len(json['foo']) - 1])

    def test_get_array_range(self, json, test_case):
        results = json_query(json, 'foo[1:2]')

        assert len(results) == 1
        test_case.assertDictEqual(results[0]['value'], json['foo'][1])

    def test_python_range_selector(self, json, test_case):
        def extract_values(results):
            return list(map(lambda result: result['value'], results))

        results = json_query(json, 'py[1:]')
        test_case.assertListEqual(extract_values(results), [ 2, 3, 4 ])

        results = json_query(json, 'py[:1]')
        test_case.assertListEqual(extract_values(results), [ 1 ])

        results = json_query(json, 'py[3:0]')
        test_case.assertListEqual(extract_values(results), [ ])

        results = json_query(json, 'py[:-1]')
        test_case.assertListEqual(extract_values(results), [ 1, 2, 3 ])

        results = json_query(json, 'py[-3:-1]')
        test_case.assertListEqual(extract_values(results), [ 2, 3 ])

        results = json_query(json, 'py[-1:]')
        test_case.assertListEqual(extract_values(results), [ 4 ])

        results = json_query(json, 'py[0:0]')
        test_case.assertListEqual(extract_values(results), [ ])

        with pytest.raises(Exception) as exc_info:
            json_query(json, 'py[0:5]')

        assert 'Index 5 is out of bounds' == str(exc_info.value)

    def test_accepts_masked_dot_character(self, json, test_case):
        results = json_query(json, "bar.'bla.blubb'")

        assert len(results) == 1
        assert results[0]['value'] == json['bar']['bla.blubb']

    def test_get_first_match(self, json, test_case):
        result = json_query_first(json, 'foo[:]')

        test_case.assertDictEqual(result['value'], json['foo'][0])

    def test_update_first_match(self, json):
        json_query_update_first(json, 'foo[:]', 'huhuhu')

        assert json['foo'][0] == 'huhuhu'

    def test_update_all_matches(self, json, test_case):
        json_query_update_all(json, 'foo[:]', {
            'foo': [
                1, 2, 3
            ]
        })

        test_case.assertEqual(json['foo'], [ 1, 2, 3 ])