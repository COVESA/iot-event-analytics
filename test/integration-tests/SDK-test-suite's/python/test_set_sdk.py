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
import logging

# Needed for protocol Gateway configuration file
# TODO: add dynamic path for directory
# import os
import json

from iotea.core.talent_test import TestSetTalent
from iotea.core.util.logger import Logger
 
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)


class TestSetSDK(TestSetTalent):
    def __init__(self, protocol_gateway_config):
        super(TestSetSDK, self).__init__('testSet-sdk-py', protocol_gateway_config)

        # Register Tests

        # Test primitives via echo
        self.register_test('echoString', "Hello World", self.test_echo_string, 2000)
        self.register_test('echoBoolean', True, self.test_echo_boolean, 2000)
        self.register_test('echoInteger', 123, self.test_echo_integer, 2000)
        self.register_test('echoDouble', 123.456, self.test_echo_double, 2000)

        # Test lists/arrays via echo
        self.register_test('echoEmptyList', [], self.test_echo_empty_list, 2000)
        self.register_test('echoIntegerList', [1, 2, 3], self.test_echo_integer_list, 2000)
        self.register_test('echoMixedList', [1, 'Hello World', 3.21], self.test_echo_mixed_list, 2000)
        self.register_test('echoDeepList', [1, [2, [3, [4, [5]]]]], self.test_echo_deep_list, 2000)

        self.talent_dependencies.add_talent('function-provider-py')
        
    def callees(self):
        return ['function-provider-py.echo']

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_string(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 ['Hello World'],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_boolean(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [True],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_integer(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [123],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_double(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [123.456],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_empty_list(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [[]],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_integer_list(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [[1, 2, 3]],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_mixed_list(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [[1, 'Hello World', 3.21]],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

    # pylint: disable=invalid-name,unused-argument
    async def test_echo_deep_list(self, ev, evtctx):
        result = await self.call('function-provider-py', 'echo',
                                 [[1, [2, [3, [4, [5]]]]]],
                                 ev['subject'],
                                 ev['returnTopic'],
                                 500)
        return result

def read_config(abs_path):
    with open(abs_path, mode='r', encoding='utf-8') as config_file:
        return json.loads(config_file.read())

async def main():
    # TODO: add dynamic path for directory
    pg_config = read_config('config/tests/python/config.json')

    test_set = TestSetSDK(pg_config['protocolGateway'])
    await test_set.start()

if __name__ == '__main__':
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
