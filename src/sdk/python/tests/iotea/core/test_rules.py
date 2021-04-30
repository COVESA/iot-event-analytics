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
from src.iotea.core.rules import AndRules, OrRules, Rule, Constraint, ChangeConstraint, OpConstraint
from tests.helpers.constraints import create_op_constraint, create_change_constraint

@pytest.fixture
def test_case():
    return TestCase()

@pytest.fixture
def rules():
    opc1 = create_op_constraint('feat2', OpConstraint.OPS['GREATER_THAN'], 10, 'anytype1', Constraint.VALUE_TYPE['ENCODED'], '/foo', Constraint.ALL_INSTANCE_IDS_FILTER, True)
    opc2 = create_op_constraint('*', OpConstraint.OPS['ISSET'], None, 'anytype2', Constraint.VALUE_TYPE['RAW'], Constraint.PATH_IDENTITY, '^test$', False)

    rules = AndRules([
        Rule(create_change_constraint('feat1', 'anytype1', Constraint.VALUE_TYPE['RAW'])),
        OrRules([
            Rule(opc1),
            Rule(opc2)
        ])
    ], [
        'anytype.feat3'
    ])
    yield rules
    # Teardown logic

class TestRules:
    def test_save(self, test_case, rules):
        test_case.assertDictEqual(rules.save(), {'excludeOn': ['anytype.feat3'], 'rules': [{'feature': 'feat1', 'path': '', 'op': 1, 'value': None, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True}, {'excludeOn': None, 'rules': [{'feature': 'feat2', 'path': '/foo', 'op': 0, 'value': {'type': 'number', 'exclusiveMinimum': 10, '$id': None }, 'valueType': 1, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True}, {'feature': '*', 'path': '', 'op': 0, 'value': {'not': {'type': 'null'}, '$id': None }, 'valueType': 0, 'typeSelector': 'anytype2', 'instanceIdFilter': '^test$', 'limitFeatureSelection': False}], 'type': 'or'}], 'type': 'and'})

    def test_append_single_rule(self):
        r = AndRules( Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'anytype1', Constraint.VALUE_TYPE['RAW'])))

        assert len(r.rules) == 1

    def test_invalid_exclude_on(self):
        with pytest.raises(Exception) as exc_info:
            AndRules([ Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'anytype1', Constraint.VALUE_TYPE['RAW'])) ], '')

        assert 'exclude_on parameter of Rules must be either None or a list of strings' == str(exc_info.value)

        with pytest.raises(Exception) as exc_info:
            # Has to contain at least type AND feature
            AndRules([ Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'anytype1', Constraint.VALUE_TYPE['RAW'])) ], [
                'foo'
            ])

        assert 'Invalid typeFeature selector "foo" found in excludeOn constraints in given Rules' == str(exc_info.value)

        with pytest.raises(Exception) as exc_info:
            # Has to contain at least type AND feature
            AndRules([ Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'anytype1', Constraint.VALUE_TYPE['RAW'])) ], [
                'notdefault.my-talent.output-feature'
            ])

        assert 'Invalid typeFeature selector "notdefault.my-talent.output-feature". Has to be default type' == str(exc_info.value)

        with pytest.raises(Exception) as exc_info:
            # Has to contain at least type AND feature
            AndRules([ Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'anytype1', Constraint.VALUE_TYPE['RAW'])) ], [
                'default.*.feature'
            ])

        assert 'Talent id has to be defined in typeFeature selector "default.*.feature"' == str(exc_info.value)

    def test_stringify_value_type(self):
        assert Constraint.stringify_value_type(Constraint.VALUE_TYPE['RAW']) == 'RAW'
        assert Constraint.stringify_value_type(Constraint.VALUE_TYPE['ENCODED']) == 'ENCODED'

        with pytest.raises(Exception) as exc_info:
            Constraint.stringify_value_type(2)

        assert 'Value type not found for key 2' == str(exc_info.value)

    def test_get_type_feature(self, test_case):
        c = create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, '100000.Vehicle', Constraint.VALUE_TYPE['RAW'])
        test_case.assertDictEqual(c.get_type_feature(), { 'feature': 'feat2', 'segment': '100000', 'type': 'Vehicle' })

        c = create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'Vehicle', Constraint.VALUE_TYPE['RAW'])
        test_case.assertDictEqual(c.get_type_feature(), { 'feature': 'feat2', 'segment': None, 'type': 'Vehicle' })

    def test_extract_segment_from_type_selector(self):
        c = create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, '100000.Vehicle', Constraint.VALUE_TYPE['RAW'])
        assert c.segment == '100000'
        assert c.type == 'Vehicle'

    def test_invalid_type_selector(self):
        with pytest.raises(Exception) as exc_info:
            Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'foo.bar.anytype1.test', Constraint.VALUE_TYPE['RAW']))

        assert 'Invalid constraint' == str(exc_info.value)

    def test_op_constraint_save(self, test_case):
        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['ISSET'], None, 'anytype1', Constraint.VALUE_TYPE['RAW'], '/foo', Constraint.ALL_INSTANCE_IDS_FILTER, True))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '/foo', 'op': 0, 'value': {'not': {'type': 'null'}, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True})

        rule = Rule(create_op_constraint('*', OpConstraint.OPS['EQUALS'], 1, 'anytype1', Constraint.VALUE_TYPE['RAW'], '/foo', Constraint.ALL_INSTANCE_IDS_FILTER, False))
        test_case.assertDictEqual(rule.save(), {'feature': '*', 'path': '/foo', 'op': 0, 'value': {'const': 1, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': False})

        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['NEQUALS'], 'test', 'anytype1', Constraint.VALUE_TYPE['RAW'], '/foo', '^test$'))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '/foo', 'op': 0, 'value': {'not': {'const': 'test'}, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '^test$', 'limitFeatureSelection': True})

        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['LESS_THAN'], 3, 'anytype1', Constraint.VALUE_TYPE['RAW']))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '', 'op': 0, 'value': {'type': 'number', 'exclusiveMaximum': 3, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True})

        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['LESS_THAN_EQUAL'], 10, 'anytype1', Constraint.VALUE_TYPE['RAW']))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '', 'op': 0, 'value': {'type': 'number', 'maximum': 10, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True})

        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['GREATER_THAN'], 5, 'anytype1', Constraint.VALUE_TYPE['RAW']))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '', 'op': 0, 'value': {'type': 'number', 'exclusiveMinimum': 5, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True})

        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['GREATER_THAN_EQUAL'], 99, 'anytype1', Constraint.VALUE_TYPE['RAW']))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '', 'op': 0, 'value': {'type': 'number', 'minimum': 99, '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True})

        rule = Rule(create_op_constraint('feat2', OpConstraint.OPS['REGEX'], '^test.*$', 'anytype1', Constraint.VALUE_TYPE['RAW']))
        test_case.assertDictEqual(rule.save(), {'feature': 'feat2', 'path': '', 'op': 0, 'value': {'type': 'string', 'pattern': '^test.*$', '$id': None}, 'valueType': 0, 'typeSelector': 'anytype1', 'instanceIdFilter': '.*', 'limitFeatureSelection': True})

        with pytest.raises(Exception) as exc_info:
            Rule(create_op_constraint('feat2', 99, None, 'anytype1', Constraint.VALUE_TYPE['RAW']))

        assert 'Invalid operation 99 given' == str(exc_info.value)