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
import os
import logging

from iotea.core.logger import Logger

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

os.environ['MQTT_TOPIC_NS'] = 'iotea/'

# pylint: disable=wrong-import-position
from iotea.core.talent_func import FunctionTalent

class IoteaRPCTalent(FunctionTalent):
    def __init__(self, connection_string):
        super(IoteaRPCTalent, self).__init__('iotea-rpc_talent', connection_string)
        self.skip_cycle_check(True)
        self.register_function('iotea-echo', self.__iotea_echo)

    # pylint: disable=unused-argument
    def __iotea_echo(self, iotea_echo, iotea_name, ev, evtctx):
        self.logger.info('Function iotea-echo has been called')
        return '{} {}'.format(iotea_echo, iotea_name)

    def callees(self):
        return [
            '{}.iotea-echo'.format(self.id)
        ]


async def main():
    iotea_rpc_talent = IoteaRPCTalent('mqtt://localhost:1883')
    await iotea_rpc_talent.start()


LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
