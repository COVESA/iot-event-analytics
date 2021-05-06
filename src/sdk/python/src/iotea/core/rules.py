##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import uuid
import re
import logging

from .constants import DEFAULT_TYPE

class Constraint:
    def __init__(self, feature, operation, value, type_selector, value_type, path, instance_id_filter, limit_feature_selection):
        self.feature = feature
        self.op = operation
        self.path = path
        self.value = value
        self.value_type = value_type
        self.type_selector = type_selector
        self.instance_id_filter = instance_id_filter
        self.limit_feature_selection = limit_feature_selection

        if self.limit_feature_selection is False and self.feature != Constraint.ANY_FEATURE:
            Constraint.logger.warning('The flag limit_feature_selection=False has only an effect if feature is set to constant ANY_FEATURE. Currently the feature is set to "{}"'.format(self.feature))

        self.segment = None

        regex = re.compile("(?:^(\\*|(?:[^\\.]+))\\.)?([^\\.]+)$")
        match = regex.match(self.type_selector)

        if match is None:
            raise Exception('Invalid constraint')

        if match[1] is not None:
            self.segment = match[1]

        self.type = match[2]

    def get_type_feature(self):
        return Rules.get_type_feature(self.type, self.feature, self.segment)

    def to_string(self):
        return '{} value of "{}{}.{}" at path "{}"'.format(
            Constraint.stringify_value_type(self.value_type),
            '{}.'.format(self.segment) if self.segment is not None else '',
            self.type,
            self.feature,
            self.path
        )

    @staticmethod
    def stringify_value_type(value_type):
        for key, _value_type in Constraint.VALUE_TYPE.items():
            if value_type == _value_type:
                return key

        raise Exception('Value type not found for key {}'.format(value_type))

Constraint.OPS = {
    'SCHEMA': 0,
    'CHANGE': 1,
    'NELSON': 2
}

Constraint.VALUE_TYPE = {
    'RAW': 0,
    'ENCODED': 1
}

Constraint.PATH_IDENTITY = ''
Constraint.ANY_FEATURE = '*'
Constraint.ALL_TYPES = '*'
Constraint.ALL_SEGMENTS = '*'
Constraint.ALL_INSTANCE_IDS_FILTER = '.*'

Constraint.logger = logging.getLogger('Constraint')


class SchemaConstraint(Constraint):
    def __init__(
        self,
        feature,
        value,
        type_selector,
        value_type,
        path=Constraint.PATH_IDENTITY,
        instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
        limit_feature_selection=True,
        sid=None
    ):
        super(SchemaConstraint, self).__init__(
            feature,
            Constraint.OPS['SCHEMA'],
            value,
            type_selector,
            value_type,
            path,
            instance_id_filter,
            limit_feature_selection
        )

        if sid is None:
            sid = self.create_schema_id()

        self.value['$id'] = sid

    def create_schema_id(self):
        return uuid.uuid1().hex


class OpConstraint(SchemaConstraint):
    def __init__(
        self,
        feature,
        op, value,
        type_selector,
        value_type,
        path=Constraint.PATH_IDENTITY,
        instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
        limit_feature_selection=True
    ):
        super(OpConstraint, self).__init__(
            feature,
            OpConstraint.create_schema(op, value),
            type_selector,
            value_type,
            path,
            instance_id_filter,
            limit_feature_selection
        )

    @staticmethod
    def create_schema(op, value):
        if op == OpConstraint.OPS['ISSET']:
            return {
                'not': {
                    'type': 'null'
                }
            }

        if op == OpConstraint.OPS['EQUALS']:
            return {
                'const': value
            }

        if op == OpConstraint.OPS['NEQUALS']:
            return {
                'not': {
                    'const': value
                }
            }

        if op == OpConstraint.OPS['LESS_THAN']:
            return {
                'type': 'number',
                'exclusiveMaximum': value
            }

        if op == OpConstraint.OPS['LESS_THAN_EQUAL']:
            return {
                'type': 'number',
                'maximum': value
            }

        if op == OpConstraint.OPS['GREATER_THAN']:
            return {
                'type': 'number',
                'exclusiveMinimum': value
            }

        if op == OpConstraint.OPS['GREATER_THAN_EQUAL']:
            return {
                'type': 'number',
                'minimum': value
            }

        if op == OpConstraint.OPS['REGEX']:
            return {
                'type': 'string',
                'pattern': value
            }

        raise Exception('Invalid operation {} given'.format(op))


OpConstraint.OPS = {
    'ISSET': 10,
    'EQUALS': 11,
    'NEQUALS': 12,
    'LESS_THAN': 20,
    'LESS_THAN_EQUAL': 21,
    'GREATER_THAN': 22,
    'GREATER_THAN_EQUAL': 23,
    'REGEX': 30
}


class ChangeConstraint(Constraint):
    def __init__(self, feature, type_selector=DEFAULT_TYPE, value_type=Constraint.VALUE_TYPE['ENCODED'], path=Constraint.PATH_IDENTITY, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(ChangeConstraint, self).__init__(feature, Constraint.OPS['CHANGE'], None, type_selector, value_type, path, instance_id_filter, limit_feature_selection)


class Rule:
    def __init__(self, constraint=None):
        self.constraint = constraint

    def save(self):
        return {
            'feature': self.constraint.feature,
            'path': self.constraint.path,
            'op': self.constraint.op,
            'value': self.constraint.value,
            'valueType': self.constraint.value_type,
            'typeSelector': self.constraint.type_selector,
            'instanceIdFilter': self.constraint.instance_id_filter,
            'limitFeatureSelection': self.constraint.limit_feature_selection
        }

Rule.logger = logging.getLogger('Rule')

class Rules(Rule):
    def __init__(self, exclude_on=None):
        super(Rules, self).__init__()
        self.rules = []
        self.exclude_on = None

        if exclude_on is not None:
            if not isinstance(exclude_on, list):
                raise Exception('exclude_on parameter of Rules must be either None or a list of strings')

            if len(exclude_on) > 0:
                self.exclude_on = exclude_on

        self.init()

    def init(self):
        if self.exclude_on is None:
            return

        regex = re.compile('^([^\.]+)\.([^\.]+)(?:\.([^\.]+))?$')

        for type_feature_selector in self.exclude_on:
            matches = regex.match(type_feature_selector)

            if matches is None:
                raise Exception(f'Invalid typeFeature selector "{type_feature_selector}" found in excludeOn constraints in given Rules')

            if matches[3] is not None:
                if matches[1] != DEFAULT_TYPE:
                    raise Exception(f'Invalid typeFeature selector "{type_feature_selector}". Has to be default type')

                if matches[2] == '*':
                    raise Exception(f'Talent id has to be defined in typeFeature selector "{type_feature_selector}"')

    def add(self, rules):
        if isinstance(rules, list):
            self.rules.extend(rules)
        else:
            self.rules.append(rules)

        return self

    def save(self):
        return {
            'excludeOn': self.exclude_on,
            'rules': list(map(lambda rule: rule.save(), self.rules))
        }

    def for_each(self, cb):
        for rule in self.rules:
            cb(rule)

            if isinstance(rule, Rules):
                rule.for_each(cb)

    @staticmethod
    def get_type_feature(_type, feature, segment=None):
        return {
            'type': _type,
            'feature': feature,
            'segment': segment
        }


class AndRules(Rules):
    def __init__(self, rules, exclude_on=None):
        super(AndRules, self).__init__(exclude_on)
        self.add(rules)

    def save(self):
        serialized_rule = super().save()
        serialized_rule['type'] = 'and'
        return serialized_rule

class OrRules(Rules):
    def __init__(self, rules, exclude_on=None):
        super(OrRules, self).__init__(exclude_on)
        self.add(rules)

    def save(self):
        serialized_rule = super().save()
        serialized_rule['type'] = 'or'
        return serialized_rule
