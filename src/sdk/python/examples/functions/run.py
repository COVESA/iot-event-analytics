##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import random
import asyncio
import logging

from iotea.core.util.logger import Logger
from iotea.core.util.mqtt_client import MqttProtocolAdapter
from iotea.core.protocol_gateway import ProtocolGateway

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

from iotea.core.util.time_ms import time_ms


# pylint: disable=wrong-import-position
from iotea.core.talent_func import FunctionTalent
from iotea.core.rules import OrRules, OpConstraint, Rule, Constraint

class MathFunctions(FunctionTalent):
    def __init__(self, protocol_gateway_config):
        super().__init__('math', protocol_gateway_config)
        self.register_function('multiply', self.__multiply)
        self.register_function('fibonacci', self.__fibonacci)
        self.register_function('sum', self.__sum)

    def callees(self):
        return [
            f'{self.id}.fibonacci'.format(self.id),
            f'{self.id}.sum'.format(self.id),
            f'{self.id}.multiply'.format(self.id)
        ]

    def get_rules(self):
        return OrRules([
            Rule(OpConstraint('anyfeature', OpConstraint.OPS['ISSET'], None, 'anytype', Constraint.VALUE_TYPE['RAW']))
        ])

    async def on_event(self, ev, evtctx):
        try:
            self.logger.info(f'Calling function for {ev["value"]}...', extra=self.logger.create_extra(evtctx))

            result = await self.call('math', 'fibonacci', [ev['value']], ev['subject'], ev['returnTopic'], 60000)
            # result = await self.call('math', 'sum', [ev['value']], ev['subject'], ev['returnTopic'], 60000)
            # result = await self.call(self.id, 'multiply', [ev['value'], ev['value']], ev['subject'], ev['returnTopic'], 60000)

            self.logger.info('Result is {}'.format(result), extra=self.logger.create_extra(evtctx))
        # pylint: disable=broad-except
        except Exception as err:
            self.logger.error('An error occurred while calling a function', extra=self.logger.create_extra(evtctx))
            self.logger.error(err)

    # pylint: disable=unused-argument
    async def __multiply(self, operand_a, operand_b, ev, evtctx, called_at_ms, timeout_ms):
        await asyncio.sleep(random.randint(0, 2))
        return operand_a * operand_b

    # pylint: disable=unused-argument
    async def __sum(self, operand, ev, evtctx, timeout_at_ms):
        if operand == 1:
            return 1

        return operand + await self.call(self.id, 'sum', [operand - 1], ev['subject'], ev['returnTopic'], timeout_at_ms - time_ms())

    async def __fibonacci(self, nth, ev, evtctx, timeout_at_ms):
        self.logger.info(f'Calculating {nth}th fibonacci number...', extra=self.logger.create_extra(evtctx))

        if nth <= 1:
            self.logger.info(f'Result for {nth}th fibonacci number is {nth}', extra=self.logger.create_extra(evtctx))
            return nth

        return await self.call(self.id, 'fibonacci', [nth - 1], ev['subject'], ev['returnTopic'], timeout_at_ms - time_ms()) + await self.call(self.id, 'fibonacci', [nth - 2], ev['subject'], ev['returnTopic'], timeout_at_ms - time_ms())

async def main():
    mqtt_config = MqttProtocolAdapter.create_default_configuration()
    pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

    math_function_talent_1 = MathFunctions(pg_config)
    math_function_talent_2 = MathFunctions(pg_config)
    await asyncio.gather(math_function_talent_1.start(), math_function_talent_2.start())

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
