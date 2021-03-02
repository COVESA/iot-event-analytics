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
 
from iotea.core.talent_func import FunctionTalent
from iotea.core.logger import Logger
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)
os.environ['MQTT_TOPIC_NS'] = 'iotea/'


class FunctionProvider(FunctionTalent):
    def __init__(self, connection_string):
        super(FunctionProvider, self).__init__('function-provider-py', connection_string)

        # Register Functions
        self.register_function('echo', self.echo)

    async def echo(self, value, ev, evctx):
        self.logger.debug('Echo called')
        return value


async def main():
    function_provider = FunctionProvider('mqtt://localhost:1883')
    await function_provider.start()

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
