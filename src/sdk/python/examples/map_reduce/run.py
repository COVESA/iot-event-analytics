##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from functools import reduce
import random
import asyncio
import logging

from iotea.core.protocol_gateway import ProtocolGateway
from iotea.core.util.logger import Logger
from iotea.core.util.mqtt_client import MqttProtocolAdapter

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

# pylint: disable=wrong-import-position
from iotea.core.talent_mr import Mapper, Worker, Reducer
from iotea.core.rules import OpConstraint, Rule
from iotea.core.constants import VALUE_TYPE_RAW

class RandomMapper(Mapper):
    def __init__(self, protocol_gateway_config):
        super().__init__('mapper', 'reducer', protocol_gateway_config)

    def get_trigger_rules(self):
        return Rule(OpConstraint('anyfeature', OpConstraint.OPS['ISSET'], None, 'anytype', VALUE_TYPE_RAW))

    async def map(self, ev):
        return [random.random() for _ in range(random.randint(1, 9))]

class RandomWorker(Worker):
    def __init__(self, connection_string):
        super().__init__('worker', 'mapper', connection_string)

    async def work(self, data):
        await asyncio.sleep(random.random() * 2)

        if random.random() > 0.95:
            raise Exception('Error computing result')

        self.logger.info(f'Calculating result for input value {data}...')

        return data * 10

class RandomReducer(Reducer):
    def __init__(self, connection_string):
        super().__init__('reducer', 'mapper', connection_string)

    async def reduce(self, data):
        reduced_result = reduce(lambda a, b: a + (b if (isinstance(b, int) or isinstance(b, float)) else 0), data, 0)
        self.logger.info(f'Reducer calculated sum {reduced_result}')

async def main():
    mqtt_config = MqttProtocolAdapter.create_default_configuration()
    pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

    mapper = RandomMapper(pg_config)
    worker1 = RandomWorker(pg_config)
    worker2 = RandomWorker(pg_config)
    reducer = RandomReducer(pg_config)
    await asyncio.gather(mapper.start(), worker1.start(), worker2.start(), reducer.start())

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
