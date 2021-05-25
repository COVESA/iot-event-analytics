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

from iotea.core.talent_test import TestRunnerTalent
from iotea.core.util.logger import Logger
from iotea.core.protocol_gateway import ProtocolGateway
from iotea.core.util.mqtt_client import MqttProtocolAdapter

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)


class TestRunner(TestRunnerTalent):
    def __init__(self, pg_config):
        super().__init__('testRunner-py', ['testSet-sdk-py', 'testSet-sdk-js'], pg_config)

async def main():
    mqtt_config = MqttProtocolAdapter.create_default_configuration()
    pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

    test_runner = TestRunner(pg_config)
    await test_runner.start()

if __name__ == '__main__':
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
