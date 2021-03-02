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
import json
import os
import logging
import sys

# TODO: create 
sys.path.append(os.path.abspath('../../../../src/sdk/python/pkg/iotea/core/test'))
from talent_test import TestRunnerTalent

from iotea.core.logger import Logger
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)
os.environ['MQTT_TOPIC_NS'] = 'iotea/'

class TestRunner(TestRunnerTalent):
    def __init__(self, connection_string):
        super(TestRunner, self).__init__('testRunner-py', ['testSet-sdk-js', 'testSet-sdk-py', 'testSet-sdk-cpp'], connection_string)
        
async def main():
    testrunner = TestRunner('mqtt://localhost:1883')
    await testrunner.start()

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
