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
from os import path
from unittest import TestCase

import pytest

from src.iotea.core.constants import DEFAULT_TYPE, DEFAULT_INSTANCE, MSG_TYPE_EVENT, MSG_TYPE_ERROR
from src.iotea.core.protocol_gateway import ProtocolGateway
from src.iotea.core.rules import AndRules, Rule, Constraint
from src.iotea.core.talent import Talent
from src.iotea.core.util.mqtt_client import MqttProtocolAdapter
from src.iotea.core.util.talent_io import TalentOutput, TalentInput
from tests.helpers.constraints import create_change_constraint
from tests.helpers.json_loader import load_json


@pytest.fixture
def test_case():
    return TestCase()


def mock_uuid():
    return '00000000'


@pytest.fixture
def talent(mocker, callees):
    class MyTalent(Talent):
        def __init__(self):
            mocker.patch('src.iotea.core.talent.uuid4', wraps=mock_uuid)
            mqtt_config = MqttProtocolAdapter.create_default_configuration()
            pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

            super().__init__('test-talent', pg_config)

        def callees(self):
            return callees

        def get_rules(self):
            return AndRules([
                Rule(create_change_constraint('temp', 'kuehlschrank', Constraint.VALUE_TYPE['RAW']))
            ])

        async def on_event(self, ev, evtctx):
            print(f'Raw value {TalentInput.get_raw_value(ev)}')

    return MyTalent()


class TestTalent:
    @pytest.mark.parametrize('callees', [['math.sum']])
    # pylint: disable=redefined-outer-name
    # callees are passed to talent fixture
    # pylint: disable=unused-argument
    def test_create_discovery_response(self, test_case, talent, callees, mocker):
        '''
        check the discovery response of a talent with trigger rules which calls other talent functions
        '''
        mocker.patch(
            'src.iotea.core.rules.SchemaConstraint.create_schema_id',
            return_value='4711'
        )

        test_case.assertDictEqual(
            talent._Talent__create_discovery_response(),  # pylint: disable=protected-access
            load_json(
                path.normpath(path.join(path.dirname(__file__), '../../resources/talent.discovery-response.json')))
        )

    @pytest.mark.parametrize('callees', [[]])
    # pylint: disable=redefined-outer-name
    # pylint: disable=unused-argument
    def test_create_discovery_response_no_callees(self, test_case, talent, callees, mocker):
        mocker.patch(
            'src.iotea.core.rules.SchemaConstraint.create_schema_id',
            return_value='4711'
        )
        test_case.assertDictEqual(
            talent._Talent__create_discovery_response(),# pylint: disable=protected-access
            load_json(path.normpath(
                path.join(path.dirname(__file__), '../../resources/talent.no-callees-discovery-response.json')))
        )

    @pytest.mark.parametrize('callees', [['math.sum']])
    @pytest.mark.asyncio
    # pylint: disable=redefined-outer-name
    # callees are used by talent fixture
    # pylint: disable=unused-argument
    async def test_function_call(self, test_case, talent, callees, mocker):
        # pylint: disable=unused-argument
        def mock_publish_json(topic, json_o, publish_options=None):
            f = asyncio.Future()
            f.set_result(None)
            return f

        mocker.patch('src.iotea.core.talent.uuid4', wraps=mock_uuid)
        mocker.patch(
            'src.iotea.core.protocol_gateway.ProtocolGateway.publish_json',
            wraps=mock_publish_json
        )

        now_ms = 1619524724000
        subject = 'testsubject'
        tsuffix = '/00000000/00000000'
        function_result_value = 42

        function_response = TalentOutput.create(talent, {
            'subject': subject
        }, 'mat.sum-out', {
            '$tsuffix': tsuffix,
            '$vpath': 'value',
            'value': function_result_value
        }, 'testsubject', DEFAULT_TYPE, DEFAULT_INSTANCE, now_ms)

        # Channels in ingestion add msgType automatically >> do it manually here
        function_response['msgType'] = MSG_TYPE_EVENT

        asyncio.ensure_future(talent._Talent__on_common_event(# pylint: disable=protected-access
            function_response,
            f'talent/{talent.id}/events{tsuffix}'
        ))

        # Wait for the result
        response = await talent.call('math', 'sum', [1, 1], subject, 'ingestion/events', 10000, now_ms)

        assert response == function_result_value

        assert talent.pg.publish_json.call_count == 1

        talent.pg.publish_json.assert_called_once_with(
            'ingestion/events',
            {'subject': 'testsubject', 'type': 'default', 'instance': 'default',
             'value': {'func': 'sum', 'args': [1, 1], 'chnl': 'test-talent.00000000', 'call': '00000000',
                       'timeoutAtMs': 1619524734000}, 'feature': 'math.sum-in', 'whenMs': 1619524724000}
        )

        expected_error_message = 'An error occurred'

        function_response['msgType'] = MSG_TYPE_ERROR
        function_response['value'] = {
            '$tsuffix': tsuffix,
            '$vpath': 'error',
            'error': expected_error_message
        }

        asyncio.ensure_future(talent._Talent__on_common_event(# pylint: disable=protected-access
            function_response,
            f'talent/{talent.id}/events{tsuffix}'
        ))

        with pytest.raises(Exception) as exc_info:
            await talent.call('math', 'sum', [1, 1], subject, 'ingestion/events', 10000, now_ms)

        assert expected_error_message == str(exc_info.value)
