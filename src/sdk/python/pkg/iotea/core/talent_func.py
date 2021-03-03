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
import functools

from .talent import Talent
from .constants import ENCODING_TYPE_OBJECT, ENCODING_TYPE_ANY, DEFAULT_TYPE, MSG_TYPE_ERROR
from .rules import OrRules, SchemaConstraint, Rule, Constraint
from .talent_io import TalentInput, TalentOutput

class FunctionTalent(Talent):
    def __init__(self, tid, connection_string, disable_mqtt5_support=False):
        super(FunctionTalent, self).__init__(tid, connection_string, disable_mqtt5_support)
        self.skip_cycle_check(True)
        self.functions = {}

    def register_function(self, name, callback):
        self.functions[name] = callback

        self.add_output('{}-in'.format(name), {
            'description': 'Argument(s) for function {}'.format(name),
            'encoding': {
                'type': ENCODING_TYPE_OBJECT,
                'encoder': None
            },
            'unit': 'ONE'
        })

        self.add_output('{}-out'.format(name), {
            'description': 'Result of function {}'.format(name),
            'encoding': {
                'type': ENCODING_TYPE_ANY,
                'encoder': None
            },
            'unit': 'ONE'
        })

    async def __process_event(self, ev, evtctx):
        raw_value = TalentInput.get_raw_value(ev)

        args = [*raw_value['args'], ev, evtctx]

        feature = '{}-out'.format(raw_value['func'])

        tsuffix = '/{}/{}'.format(raw_value['chnl'], raw_value['call'])

        result_event = None

        try:
            func_result = None
            func = self.functions[raw_value['func']]

            if asyncio.iscoroutinefunction(func):
                func_result = await func(*args)
            else:
                func_result = func(*args)

            result_event = TalentOutput.create(self, ev, feature, {
                '$tsuffix': tsuffix,
                '$vpath': 'value',
                'value': func_result
            })
        # pylint: disable=broad-except
        except Exception as ex:
            result_event = TalentOutput.create(self, ev, feature, {
                '$tsuffix': tsuffix,
                '$vpath': 'error',
                'error': str(ex)
            })

            result_event['msgType'] = MSG_TYPE_ERROR
        finally:
            await self.publish_out_events(ev['returnTopic'], [result_event])

    def __start_process_event(self, ev, evtctx):
        loop = asyncio.new_event_loop()
        loop.run_until_complete(functools.partial(self.__process_event, ev=ev, evtctx=evtctx)())
        loop.close()

    async def on_event(self, ev, evtctx):
        asyncio.get_running_loop().run_in_executor(None, self.__start_process_event, ev, evtctx)

    def get_rules(self):
        arg_rules = []

        for func in self.functions:
            event_schema = {
                'type': 'object',
                'required': ['func', 'args', 'chnl', 'call'],
                'properties': {
                    'func': {
                        'type': 'string',
                        'const': func
                    },
                    'args': {
                        'type': 'array'
                    },
                    'chnl': {
                        'type': 'string'
                    },
                    'call': {
                        'type': 'string'
                    }
                },
                'additionalProperties': False
            }

            arg_rules.append(
                Rule(SchemaConstraint('{}.{}-in'.format(self.id, func), event_schema, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'])),
            )

        return OrRules(arg_rules)
