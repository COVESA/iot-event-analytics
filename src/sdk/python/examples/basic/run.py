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

from iotea.core.util.logger import Logger
from iotea.core.util.talent_io import TalentInput
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

os.environ['MQTT_TOPIC_NS'] = 'iotea/'

# pylint: disable=wrong-import-position
from iotea.core.talent import Talent
from iotea.core.rules import AndRules, Rule, ChangeConstraint, Constraint

class MyTalent(Talent):
    def __init__(self, connection_string):
        super(MyTalent, self).__init__('python-basic-talent', connection_string)

    def get_rules(self):
        return AndRules([
            Rule(ChangeConstraint('temp', 'kuehlschrank', Constraint.VALUE_TYPE['RAW']))
        ])

    async def on_event(self, ev, evtctx):
        print(f'Raw value {TalentInput.get_raw_value(ev)}')


async def main():
    my_talent = MyTalent('mqtt://localhost:1883')
    await my_talent.start()

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
