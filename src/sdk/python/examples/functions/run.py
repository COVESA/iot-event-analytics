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
import os
import asyncio
import logging

from iotea.core.logger import Logger
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.DEBUG)

os.environ['MQTT_TOPIC_NS'] = 'iotea/'

# pylint: disable=wrong-import-position
from iotea.core.talent_func import FunctionTalent
from iotea.core.talent import Talent
from iotea.core.rules import OrRules, OpConstraint, Rule, Constraint

class MathFunctions(FunctionTalent):
    def __init__(self, connection_string):
        super(MathFunctions, self).__init__('math', connection_string)
        self.register_function('multiply', self.__multiply)
        self.register_function('fibonacci', self.__fibonacci)
        self.register_function('sum', self.__sum)

    def callees(self):
        return [
            '{}.fibonacci'.format(self.id),
            '{}.sum'.format(self.id)
        ]

    # pylint: disable=unused-argument
    async def __multiply(self, operand_a, operand_b, ev, evtctx):
        await asyncio.sleep(random.randint(0, 2))
        return operand_a * operand_b

    # pylint: disable=unused-argument
    async def __sum(self, operand, ev, evtctx):
        if operand == 1:
            return 1

        return operand + await self.call(self.id, 'sum', [operand - 1], ev['subject'], ev['returnTopic'], 60000)

    async def __fibonacci(self, nth, ev, evtctx):
        self.logger.info('Calculating {}th fibonacci number...'.format(nth), extra=self.logger.create_extra(evtctx))

        if nth <= 1:
            self.logger.info('Result for {nth}th fibonacci number is {val}'.format(nth=nth, val=nth), extra=self.logger.create_extra(evtctx))
            return nth

        return await self.call(self.id, 'fibonacci', [nth - 1], ev['subject'], ev['returnTopic'], 60000) + await self.call(self.id, 'fibonacci', [nth - 2], ev['subject'], ev['returnTopic'], 60000)

class TempTalent(Talent):
    def __init__(self, connection_string):
        super(TempTalent, self).__init__('temp-talent', connection_string)

    def callees(self):
        return [
            'math.fibonacci'
        ]

    def get_rules(self):
        return OrRules([
            Rule(OpConstraint('anyfeature', OpConstraint.OPS['ISSET'], None, 'anytype', Constraint.VALUE_TYPE['RAW']))
        ])

    async def on_event(self, ev, evtctx):
        try:
            self.logger.info('Calling function for {}...'.format(ev['value']), extra=self.logger.create_extra(evtctx))

            result = await self.call('math', 'fibonacci', [ev['value']], ev['subject'], ev['returnTopic'], 60000)
            # result = await self.call('math', 'sum', [ev['value']], ev['subject'], ev['returnTopic'], 60000)
            # result = await self.call('math', 'multiply', [ev['value'], ev['value']], ev['subject'], ev['returnTopic'], 60000)

            self.logger.info('Result is {}'.format(result), extra=self.logger.create_extra(evtctx))
        # pylint: disable=broad-except
        except Exception as err:
            self.logger.error('An error occurred while calling a function', extra=self.logger.create_extra(evtctx))
            self.logger.error(err)

async def main():
    math_function_talent_1 = MathFunctions('mqtt://localhost:1883')
    math_function_talent_2 = MathFunctions('mqtt://localhost:1883')
    trigger_talent = TempTalent('mqtt://localhost:1883')
    await asyncio.gather(trigger_talent.start(), math_function_talent_1.start(), math_function_talent_2.start())

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
