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

from iotea.core.talent_test import TestRunnerTalent
from iotea.core.util.logger import Logger

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)


class TestRunner(TestRunnerTalent):
    def __init__(self, protocol_gateway_config):
        #super(TestRunner, self).__init__('testRunner-py', ['testSet-sdk-py'], ['testSet-sdk-js'], ['testSet-sdk-cpp'], protocol_gateway_config)
        super(TestRunner, self).__init__('testRunner-py', ['testSet-sdk-py'], ['testSet-sdk-js'],  protocol_gateway_config)

def read_config(abs_path):
    with open(abs_path, mode='r', encoding='utf-8') as config_file:
        return json.loads(config_file.read())

async def main():
    # TODO: add dynamic path for directory
    #pg_config = read_config(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'config', 'config.json'))
    
    # TODO: make this local vs container setup configurable with ifdef
    pg_config = read_config('config/tests/python/config.json')

    test_runner = TestRunner(pg_config['protocolGateway'])
    await test_runner.start()

if __name__ == '__main__':
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
