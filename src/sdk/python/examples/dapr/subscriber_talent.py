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
import sys
import logging

from iotea.core.logger import Logger

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

os.environ['MQTT_TOPIC_NS'] = 'iotea/'

# pylint: disable=wrong-import-position
from iotea.core.talent_func import Talent
from iotea.core.rules import OrRules, Rule, Constraint, OpConstraint
from iotea.core.constants import DEFAULT_TYPE

class SubscriberTalent(Talent):
    def __init__(self, connection_string):
        super(SubscriberTalent, self).__init__('subscriber_talent', connection_string)
        self.skip_cycle_check(True)

    def callees(self):
        return [
            'dapr_adapter.dapr-echo',
        ]

    def get_rules(self):
        return OrRules([
            Rule(OpConstraint('dapr_adapter.speed', OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'])),
            Rule(OpConstraint('dapr_adapter.rpm', OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'])),
            Rule(OpConstraint('dapr_adapter.driver', OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW']))
        ])

    async def on_event(self, ev, evtctx):
        print('Event of feature {} with value \'{}\' arrived'.format(ev['feature'], ev['value']))

        print('Call dapr_adapter.dapr-echo')

        echo = 'Hello'
        name = 'IoTEA'
        self.logger.info('Return topic: {}, Subject {}'.format(ev['returnTopic'], ev['subject']))
        result = None

        try:
            result = await self.call('dapr_adapter', 'dapr-echo', [echo, name], ev['subject'], ev['returnTopic'], 10000)
        # pylint: disable=broad-except
        except Exception as ex:
            self.logger.error('Function call was not successful (Exception: {})'.format(ex))
        except:
            ex = sys.exc_info()[0]
            self.logger.error('Function call was not successful ({})'.format(ex))

        print('Result of dapr.echo = {}'.format(result))


async def main():
    subscriber_talent = SubscriberTalent('mqtt://localhost:1883')
    await subscriber_talent.start()


LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
