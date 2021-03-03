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
import logging

from iotea.core.logger import Logger
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.DEBUG)

# pylint: disable=wrong-import-position
from iotea.core.talent import Talent
from iotea.core.rules import AndRules, Rule, OpConstraint, Constraint

class MyTalent(Talent):
    def __init__(self, connection_string):
        super(MyTalent, self).__init__('python-remote-talent', connection_string)

    def get_rules(self):
        return AndRules([
            Rule(OpConstraint('temp', OpConstraint.OPS['ISSET'], None, 'kuehlschrank', Constraint.VALUE_TYPE['RAW']))
        ])

    def is_remote(self):
        return True

    async def on_event(self, ev, evtctx):
        print(json.dumps(ev['$feature']))


async def main():
    remote_talent = MyTalent('mqtt://localhost:1884')
    await remote_talent.start()

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
