##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import time
from .json_model import JsonModel
from .json_query import json_query_first
from ..constants import DEFAULT_TYPE, DEFAULT_INSTANCE, VALUE_TYPE_ENCODED, VALUE_TYPE_RAW

class TalentIO:
    def __init__(self):
        pass

    @staticmethod
    def ensure_model(model):
        if isinstance(model, TalentIO):
            return model

        return JsonModel(model)

class TalentInput:
    def __init__(self):
        pass

    @staticmethod
    def get_raw_value(ev, max_value_count=1, include_timestamp=False, feature=None, _type=None, instance=None):
        return TalentInput.__get_value(ev, VALUE_TYPE_RAW, feature, _type, instance, max_value_count, include_timestamp)

    @staticmethod
    def get_encoded_value(ev, max_value_count=1, include_timestamp=False, feature=None, _type=None, instance=None):
        return TalentInput.__get_value(ev, VALUE_TYPE_ENCODED, feature, _type, instance, max_value_count, include_timestamp)

    @staticmethod
    def get_instances_for(ev, feature=None, _type=None):
        if feature is None:
            feature = ev['feature']

        if _type is None:
            _type = ev['type']

        return list(filter(lambda instance: instance[0] != '$', TalentIO.ensure_model(ev).get('$features.{}.\'{}\''.format(_type, feature)).keys()))

    @staticmethod
    def get_stats(ev, feature=None, _type=None, instance=None):
        if feature is None:
            feature = ev['feature']

        if _type is None:
            _type = ev['type']

        if instance is None:
            instance = ev['instance']

        model = TalentIO.ensure_model(ev)

        _feature = model.get("$features.{}.'{}'.{}.$feature".format(_type, feature, instance))

        if 'stat' not in _feature:
            raise Exception('No statistical information available for {}.{}. It\'s only available for encoded features.'.format(_type, feature))

        return _feature['stat']

    @staticmethod
    def get_metadata(ev, feature=None, _type=None):
        if feature is None:
            feature = ev['feature']

        if _type is None:
            _type = ev['type']

        return TalentIO.ensure_model(ev).get("$features.{}.'{}'.$metadata".format(_type, feature))

    @staticmethod
    def __get_value(ev, value_type, feature=None, _type=DEFAULT_TYPE, instance=DEFAULT_INSTANCE, max_value_count=1, include_timestamp=False):
        if feature is None:
            feature = ev['feature']

        if _type is None:
            _type = ev['type']

        if instance is None:
            instance = ev['instance']

        if max_value_count < 1:
            raise Exception('max_value_count must be greater than 0')

        model = TalentIO.ensure_model(ev)

        _feature = model.get('$features.{}.\'{}\'.{}.$feature'.format(_type, feature, instance))

        field_name = 'enc' if value_type == VALUE_TYPE_ENCODED else 'raw'

        values = []

        for i in range(0, max_value_count):
            if i == 0:
                values.append(TalentInput.__resolve_value(_feature, field_name, include_timestamp))

                if max_value_count == 1:
                    return values[0]
            else:
                history_idx = i - 1

                if history_idx == len(_feature['history']):
                    break

                values.append(TalentInput.__resolve_value(_feature['history'][history_idx], field_name, include_timestamp))

        return values

    @staticmethod
    def __resolve_value(_feature, field_name, include_timestamp):
        if _feature[field_name] is None:
            return None

        if not isinstance(_feature[field_name], dict) or '$vpath' not in _feature[field_name]:
            return TalentInput.__enrich_value(_feature, _feature[field_name], include_timestamp)

        return TalentInput.__enrich_value(_feature, json_query_first(_feature[field_name], _feature[field_name]['$vpath'])['value'])

    @staticmethod
    def __enrich_value(_feature, value, include_timestamp=False):
        if include_timestamp is False:
            return value

        return {
            'value': value,
            'ts': _feature['whenMs']
        }

class FeatureMetadata:
    def __init__(self):
        pass

    @staticmethod
    def get_unit(_metadata):
        return TalentIO.ensure_model(_metadata).get('$unit')

class TalentOutput:
    def __init__(self):
        self.outputs = []

    def add(self, talent, ev, feature, value, subject=None, _type=DEFAULT_TYPE, instance=DEFAULT_INSTANCE, timestamp=None):
        self.outputs.append(TalentOutput.create(talent, ev, feature, value, subject, _type, instance, timestamp))

    def add_for(self, subject, _type, instance, feature, value, timestamp=None):
        self.outputs.append(TalentOutput.create_for(subject, _type, instance, feature, value, timestamp))

    def to_json(self):
        return self.outputs

    @staticmethod
    def create(talent, ev, feature, value, subject=None, _type=DEFAULT_TYPE, instance=DEFAULT_INSTANCE, timestamp=None):
        if timestamp is None:
            timestamp = round(time.time() * 1000)

        talent_id = None

        if talent is not None:
            talent_id = talent.id

        if subject is None:
            subject = ev['subject']

        return {
            'subject': subject,
            'type': _type,
            'instance': instance,
            'value': value,
            'feature': '.'.join(filter(lambda s: s is not None, [talent_id, feature])),
            'whenMs': timestamp
        }

    @staticmethod
    def create_for(subject, _type, instance, feature, value, timestamp=None):
        if not isinstance(subject, str):
            raise Exception('The subject needs to be a string, which identifies the instances affiliation')

        return TalentOutput.create(None, None, feature, value, subject, _type, instance, timestamp)

# import json

# pylint: disable=line-too-long
# ev = json.loads('{"returnTopic":"iotea/ingestion/events","$features":{"Vehicle":{"test.Body$Lights$IsBrakeOn":{"4711":{"$feature":{"whenMs":1606820429584,"ttlMs":1606820459584,"history":[{"whenMs":1606820427604,"ttlMs":1606820457604,"raw":1,"enc":0.3333333333333333},{"whenMs":1606820425626,"ttlMs":1606820455626,"raw":0,"enc":0}],"raw":2,"enc":0.6666666666666666,"stat":{"cnt":3,"mean":0.3333333333333333,"var":0.1111111111111111,"sdev":0.3333333333333333}},"matches":1},"$metadata":{"description":"Is brake light on","idx":0,"history":20,"encoding":{"type":"number","encoder":"minmax","min":0,"max":3},"unit":"µA","$unit":{"fac":0.000001,"unit":"µA","desc":"Mikroampere","base":{"fac":1,"unit":"A","desc":"Ampere"}}}}}},"type":"Vehicle","feature":"test.Body$Lights$IsBrakeOn","value":2,"whenMs":1606820429584,"instance":"4711","subject":"someuserid","now":1606820429587,"msgType":1,"$metadata":{"description":"Is brake light on","idx":0,"history":20,"encoding":{"type":"number","encoder":"minmax","min":0,"max":3},"unit":"µA","$unit":{"fac":0.000001,"unit":"µA","desc":"Mikroampere","base":{"fac":1,"unit":"A","desc":"Ampere"}}},"segment":"100000","cid":"620ab4a4-a461-43b9-9bba-7def876ec696","$feature":{"whenMs":1606820429584,"ttlMs":1606820459584,"history":[{"whenMs":1606820427604,"ttlMs":1606820457604,"raw":1,"enc":0.3333333333333333},{"whenMs":1606820425626,"ttlMs":1606820455626,"raw":0,"enc":0}],"raw":2,"enc":0.6666666666666666,"stat":{"cnt":3,"mean":0.3333333333333333,"var":0.1111111111111111,"sdev":0.3333333333333333}}}')

# print(TalentInput.get_raw_value(ev))
# print(TalentInput.get_encoded_value(ev, 10, True))
# print(TalentInput.get_encoded_value(ev, 1, True, 'test.Body$Lights$IsBrakeOn', 'Vehicle'))
# print(TalentInput.get_stats(ev))
# print(TalentInput.get_instances_for(ev))
# print(FeatureMetadata.get_unit(TalentInput.get_metadata(ev)))

# print(TalentOutput.create({'id': 'test'}, { 'subject': 'someuserid'}, 'myfeature', 3))
# print(TalentOutput.create_for('someuserid', 'mytype', 'myinstance', 'thefeature', 5))

# to = TalentOutput()
# to.add({'id': 'test2'}, { 'subject': 'someuserid2'}, 'myfeature2', 1337)
# to.add({'id': 'test2'}, { 'subject': 'someuserid2'}, 'myfeature3', 22)
# to.add_for('someuserid2', 'mytype', 'myinstance', 'myfeature4', 33)
# print(to.to_json())
