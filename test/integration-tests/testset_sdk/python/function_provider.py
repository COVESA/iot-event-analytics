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

from iotea.core.talent_func import FunctionTalent
from iotea.core.util.logger import Logger
from iotea.core.protocol_gateway import ProtocolGateway
from iotea.core.util.mqtt_client import MqttProtocolAdapter

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

class FunctionProvider(FunctionTalent):
    def __init__(self, pg_config):
        super().__init__('function-provider-py', pg_config)

        # Register Functions
        self.register_function('echo', self.echo)

    # pylint: disable=invalid-name,unused-argument
    async def echo(self, value, ev, evtctx, timeout_at_ms):
        self.logger.debug('Echo called')
        return value


async def main():
    mqtt_config = MqttProtocolAdapter.create_default_configuration()
    pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

    function_provider = FunctionProvider(pg_config)
    await function_provider.start()

if __name__ == '__main__':
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
