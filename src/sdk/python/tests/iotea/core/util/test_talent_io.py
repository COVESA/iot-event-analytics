import json
import time
from os import path
from unittest import TestCase
from unittest.mock import Mock

import pytest

from src.iotea.core.constants import DEFAULT_TYPE, DEFAULT_INSTANCE
from src.iotea.core.util.talent_io import TalentInput, FeatureMetadata, TalentOutput


@pytest.fixture
def test_case():
    return TestCase()


@pytest.fixture
def event():
    with open(path.normpath(path.join(path.dirname(__file__), '../../../resources/talentIO.event.json')), 'r') as f:
        return json.load(f)


def test_get_raw_value(test_case, event):
    test_case.assertEqual(2, TalentInput.get_raw_value(event), 'Does not match expected raw value!')

    test_case.assertEqual(3, len(TalentInput.get_raw_value(event, 100)), 'Does not match the count of all raw values!')

    all_raw_values = TalentInput.get_raw_value(event, 100, True)
    test_case.assertEqual(3, len(all_raw_values),
                          'Requested raw values with timestamp. Does not match the count of retrieved array!')
    for raw_value in all_raw_values:
        test_case.assertTrue('value' in raw_value and 'ts' in raw_value,
                             'Raw value expected to contain "value" and "ts" properties!')

    test_case.assertEqual(0.6666666666666666, TalentInput.get_encoded_value(event),
                          'Does not match expected encoded value!')

    test_case.assertEqual(['4711'], TalentInput.get_instances_for(event), 'Does not match expected event instances!')

    metadata = TalentInput.get_metadata(event)
    test_case.assertEqual(event['$features']['Vehicle']['Body$Lights$IsBrakeOn']['$metadata'], metadata,
                          'Retrieved metadata does not match the entry in the json event!')

    stats = TalentInput.get_stats(event)
    test_case.assertEqual(event['$features']['Vehicle']['Body$Lights$IsBrakeOn']['4711']['$feature']['stat'], stats,
                          'Retrieved stats does not match the entry in the json event!')

    # ignoreValuePath is not a parameter in python version of TalentInput.get_raw_value, so skipping the tests for it


def test_feature_metadata(test_case, event):
    unit = FeatureMetadata.get_unit(TalentInput.get_metadata(event))
    test_case.assertEqual(event['$features']['Vehicle']['Body$Lights$IsBrakeOn']['$metadata']['$unit'], unit,
                          'Retrieved unit does not match the entry in the json event!')


def test_talent_output_create(test_case, event):
    talent_mock = Mock()
    talent_mock.id = 'test'
    now = time.time() * 1000
    output = TalentOutput.create(talent_mock, event, 'myfeature', 3, event['subject'], DEFAULT_TYPE, DEFAULT_INSTANCE,
                                 now)

    test_case.assertEqual({
        'subject': 'someuserid',
        'type': DEFAULT_TYPE,
        'instance': DEFAULT_INSTANCE,
        'value': 3,
        'feature': 'test.myfeature',
        'whenMs': now
    }, output, 'Created output feature not created properly!')


def test_talent_output_create_for(test_case, event):
    now = time.time() * 1000
    output = TalentOutput.create_for(event['subject'], DEFAULT_TYPE, DEFAULT_INSTANCE, 'anotherfeature', 22, now)
    test_case.assertEqual({'subject': 'someuserid',
                           'type': DEFAULT_TYPE,
                           'instance': DEFAULT_INSTANCE,
                           'value': 22,
                           'feature': 'anotherfeature',
                           'whenMs': now}, output, 'Expected to create an arbitrary output')


def test_talent_output_create_multiple_outputs(test_case, event):
    now = time.time() * 1000
    to = TalentOutput()
    talent_mock = Mock()
    talent_mock.id = 'test'

    to.add(talent_mock, event, 'myfeature2', 1337, event['subject'], DEFAULT_TYPE, DEFAULT_INSTANCE, now)
    to.add(talent_mock, event, 'myfeature3', 22, event['subject'], DEFAULT_TYPE, DEFAULT_INSTANCE, now);
    to.add_for('someuserid2', 'mytype', 'myinstance', 'myfeature4', 33, now);
    test_case.assertEqual([
        {
            'subject': 'someuserid',
            'type': DEFAULT_TYPE,
            'instance': DEFAULT_INSTANCE,
            'value': 1337,
            'feature': 'test.myfeature2',
            'whenMs': now
        },
        {
            'subject': 'someuserid',
            'type': DEFAULT_TYPE,
            'instance': DEFAULT_INSTANCE,
            'value': 22,
            'feature': 'test.myfeature3',
            'whenMs': now
        },
        {
            'subject': 'someuserid2',
            'type': 'mytype',
            'instance': 'myinstance',
            'value': 33,
            'feature': 'myfeature4',
            'whenMs': now
        }
    ], to.to_json())
