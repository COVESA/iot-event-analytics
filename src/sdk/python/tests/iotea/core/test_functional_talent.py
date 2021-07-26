##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import asyncio
import unittest.mock
from collections import Counter
from os import path
from unittest import TestCase

import pytest

from src.iotea.core.protocol_gateway import ProtocolGateway
from src.iotea.core.rules import AndRules, Rule, Constraint
from src.iotea.core.talent_func import FunctionTalent
from src.iotea.core.util.mqtt_client import MqttProtocolAdapter
from src.iotea.core.util.talent_io import TalentInput
from tests.helpers.constraints import create_change_constraint
from tests.helpers.json_loader import load_json

# timeout for blocking operations on asyncio synchronization primitives
TIMEOUT = 10


def async_sum_function():
    # pylint: disable=unused-argument
    async def __sum(operand1, operand2, ev, evtctx, timeout_at_ms):
        # just to have some async code
        await asyncio.sleep(0.01)
        return operand1 + operand2

    return __sum


def sync_sum_function():
    # pylint: disable=unused-argument
    def __sum(operand1, operand2, ev, evtctx, timeout_at_ms):
        return operand1 + operand2

    return __sum


def failing_function():
    def __sum(operand1, operand2, ev, evtctx, timeout_at_ms):
        raise Exception('Testing failed function execution!')

    return __sum


@pytest.fixture
def test_case():
    return TestCase()


@pytest.fixture
def func_talent(callees, is_triggerable):
    class MyTalent(FunctionTalent):
        def __init__(self):
            mqtt_config = MqttProtocolAdapter.create_default_configuration()
            pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

            super().__init__('test-func-talent', pg_config)

        def callees(self):
            return callees

        def get_rules(self):
            if is_triggerable:
                return AndRules([
                    Rule(create_change_constraint('temp', 'kuehlschrank', Constraint.VALUE_TYPE['RAW']))
                ])
            return None

        async def on_event(self, ev, evtctx):
            print(f'Raw value {TalentInput.get_raw_value(ev)}')

    return MyTalent()


def json_function_response():
    return {'subject': 'someuserid',
            'type': 'default',
            'instance': 'default',
            'value': {
                '$tsuffix': '/test-func-talent.00000000/00000000',
                '$vpath': 'value',
                'value': 7},
            'feature': 'test-func-talent.sum-out'}


def get_mock_publish_json(publish_json_called):
    # pylint: disable=unused-argument
    def mock_publish_json(topic, json_o, publish_options=None):
        f = asyncio.Future()
        f.set_result(None)
        publish_json_called.set()
        return f

    return mock_publish_json


class TestTalent:
    # check the discovery response of a talent with trigger rules which calls other talent functions
    @pytest.mark.parametrize('callees', [['math.sum']])
    @pytest.mark.parametrize('is_triggerable', [False])
    # pylint: disable=redefined-outer-name
    # callees and is_triggerable are passed to func_talent fixture:
    # pylint: disable=unused-argument
    def test_register_function(self, test_case, func_talent, callees, is_triggerable):
        func_talent.register_function('math.plus', lambda: {})
        test_case.assertEqual(2, len(func_talent.output_features))
        test_case.assertEqual("math.plus-in", func_talent.output_features[0].feature)
        test_case.assertDictEqual(
            {'description': 'Argument(s) for function math.plus',
             'ttl': 0,
             'history': 0,
             'encoding': {'type': 'object', 'encoder': None},
             'unit': 'ONE'}, func_talent.output_features[0].metadata)

        test_case.assertEqual("math.plus-out", func_talent.output_features[1].feature)
        test_case.assertDictEqual(
            {'description': 'Result of function math.plus',
             'ttl': 0,
             'history': 0,
             'encoding': {'type': 'any', 'encoder': None},
             'unit': 'ONE'}, func_talent.output_features[1].metadata)

        test_case.assertEqual(1, len(func_talent.function_input_features))
        test_case.assertEqual('test-func-talent.math.plus-in', func_talent.function_input_features[0])

    # test non-triggerable FunctionalTalent which provides and consumes functions
    @pytest.mark.parametrize('callees', [['math.sum']])
    @pytest.mark.parametrize('is_triggerable', [False])
    # pylint: disable=redefined-outer-name
    # pylint: disable=too-many-arguments
    def test_create_discovery_response_1(self, test_case, func_talent, callees, is_triggerable, mocker):
        self.__test_discovery_response(test_case, func_talent, callees, is_triggerable,
                                       mocker,
                                       '../../resources/func.talent.no-trigger-discovery-response.json',
                                       True)

    # pylint: disable=redefined-outer-name
    # callees and is_triggerable are passed to func_talent fixture:
    # pylint: disable=unused-argument
    # pylint: disable=too-many-arguments
    def __test_discovery_response(self, test_case, func_talent, callees, is_triggerable, mocker, response_resource,
                                  check_cc):
        mocker.patch(
            'src.iotea.core.rules.SchemaConstraint.create_schema_id',
            return_value='4712'
        )
        func_talent.register_function('math.plus', lambda: {})
        expected_rsp = load_json(path.normpath(
            path.join(path.dirname(__file__), response_resource)))
        actual_disc_rsp = func_talent._Talent__create_discovery_response()#pylint: disable=protected-access
        if check_cc:
            # skip cycling check elements:
            #       "default.test-func-talent.math.plus-in",
            #       "default.math.sum-out"
            # might appear in different order
            # that's why they are removed from the response first and compared separately
            expected_scc = expected_rsp['config'].pop('scc')
            actual_scc = actual_disc_rsp['config'].pop('scc')
            test_case.assertEqual(Counter(expected_scc), Counter(actual_scc))# pylint: disable=too-many-function-args
        test_case.assertDictEqual(expected_rsp, actual_disc_rsp)

    # test triggerable FunctionalTalent which provides and consumes functions
    @pytest.mark.parametrize('callees', [['math.sum']])
    @pytest.mark.parametrize('is_triggerable', [True])
    # pylint: disable=redefined-outer-name
    # pylint: disable=too-many-arguments
    def test_create_discovery_response_2(self, test_case, func_talent, callees, is_triggerable, mocker):
        self.__test_discovery_response(test_case, func_talent, callees, is_triggerable,
                                       mocker,
                                       '../../resources/func.talent.trigger-discovery-response.json',
                                       True)

    # test triggerable FunctionalTalent which provides functions
    @pytest.mark.parametrize('callees', [[]])
    @pytest.mark.parametrize('is_triggerable', [True])
    # pylint: disable=redefined-outer-name
    # pylint: disable=too-many-arguments
    def test_create_discovery_response_3(self, test_case, func_talent, callees, is_triggerable, mocker):
        self.__test_discovery_response(test_case, func_talent, callees, is_triggerable,
                                       mocker,
                                       '../../resources/func.talent.trigger.no-callees.discovery-response.json',
                                       False)

    # pylint: disable=redefined-outer-name
    # callees and is_triggerable are passed to func_talent fixture:
    # pylint: disable=unused-argument
    # pylint: disable=too-many-arguments
    async def __test_func_exec(self, test_case, func_talent, callees, is_triggerable, function, mocker):
        publish_json_called = asyncio.Event()

        mocker.patch(
            'src.iotea.core.protocol_gateway.ProtocolGateway.publish_json',
            wraps=get_mock_publish_json(publish_json_called)
        )

        func_talent.register_function('sum', function)

        event = load_json(path.normpath(path.join(path.dirname(__file__),
                                                  '../../resources/function-event-call-sum.json')))
        func_talent._Talent__on_event(event, "talent/test-func-talent/events")#pylint: disable=protected-access

        await asyncio.wait_for(publish_json_called.wait(), TIMEOUT)

        test_case.assertTrue(publish_json_called.is_set(),
                             'Expected that function result should have been published!')

        func_talent.pg.publish_json.assert_called_with('123456/ingestion/events', unittest.mock.ANY)


    @pytest.mark.parametrize('callees', [[]])
    @pytest.mark.parametrize('is_triggerable', [False])
    @pytest.mark.parametrize('function', [async_sum_function(), sync_sum_function()])
    @pytest.mark.asyncio
    # pylint: disable=redefined-outer-name
    # pylint: disable=too-many-arguments
    async def test_func_exec(self, test_case, func_talent, callees, is_triggerable, function, mocker):
        await self.__test_func_exec(test_case, func_talent, callees, is_triggerable, function, mocker)
        args, _ = func_talent.pg.publish_json.call_args
        actual_response = args[1]
        # whenMs timestamp will be different
        test_case.assertIsNotNone(actual_response['whenMs'])
        del actual_response['whenMs']
        # check the result of the function without the timestamp
        test_case.assertDictEqual(json_function_response(), actual_response)

    @pytest.mark.parametrize('callees', [[]])
    @pytest.mark.parametrize('is_triggerable', [False])
    @pytest.mark.parametrize('function', [failing_function()])
    @pytest.mark.asyncio
    # pylint: disable=redefined-outer-name
    # pylint: disable=too-many-arguments
    async def test_failed_func_exec(self, test_case, func_talent, callees, is_triggerable, function, mocker):
        await self.__test_func_exec(test_case, func_talent, callees, is_triggerable, function, mocker)
        args, _ = func_talent.pg.publish_json.call_args
        actual_response = args[1]

        # whenMs timestamp will be different
        test_case.assertIsNotNone(actual_response['whenMs'])
        del actual_response['whenMs']

        expected_error_response = {
            'subject': 'someuserid',
            'type': 'default',
            'instance': 'default',
            'value': {
                '$tsuffix': '/test-func-talent.00000000/00000000',
                '$vpath': 'error',
                'error': 'Testing failed function execution!'
            },
            'feature': 'test-func-talent.sum-out',
            'msgType': 4}

        # check the result of the function without the timestamp
        test_case.assertDictEqual(expected_error_response, actual_response)

    @pytest.mark.parametrize('callees', [[]])
    @pytest.mark.parametrize('is_triggerable', [False])
    @pytest.mark.asyncio
    # pylint: disable=redefined-outer-name
    # callees and is_triggerable are passed to func_talent fixture:
    # pylint: disable=unused-argument
    async def test_empty_func_talent(self, test_case, func_talent, callees, is_triggerable, mocker):
        """
        A FunctionalTalent with no registered function, callees or getRules implementation is useless and
        create_discovery_response ends with an Exception.
        """
        mocker.patch(
            'src.iotea.core.rules.SchemaConstraint.create_schema_id',
            return_value='4712'
        )
        with pytest.raises(Exception) as exc_info:
            func_talent._Talent__create_discovery_response()#pylint: disable=protected-access

        test_case.assertEqual('You have to at least register a function or override the get_rules() method.',
                              str(exc_info.value))
