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
from .util.talent_io import TalentInput, TalentOutput

class FunctionTalent(Talent):
    def __init__(self, tid, connection_string):
        super(FunctionTalent, self).__init__(tid, connection_string)
        self.functions = {}
        self.function_input_features = []

    def register_function(self, name, callback):
        self.functions[name] = callback

        self.skip_cycle_check_for(f'{DEFAULT_TYPE}.{self.id}.{name}-in')

        self.add_output(f'{name}-in', {
            'description': f'Argument(s) for function {name}',
            'ttl': 0,
            'history': 0,
            'encoding': {
                'type': ENCODING_TYPE_OBJECT,
                'encoder': None
            },
            'unit': 'ONE'
        })

        self.add_output(f'{name}-out', {
            'description': f'Result of function {name}',
            'ttl': 0,
            'history': 0,
            'encoding': {
                'type': ENCODING_TYPE_ANY,
                'encoder': None
            },
            'unit': 'ONE'
        })

        self.function_input_features.append(f'{self.id}.{name}-in')

    def _get_rules(self):
        function_input_rules = []

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

            function_input_rules.append(
                Rule(SchemaConstraint(f'{self.id}.{func}-in', event_schema, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'])),
            )

        try:
            talent_rules = self.get_rules()
            talent_rules.exclude_on = list(map(lambda func: f'{DEFAULT_TYPE}.{self.id}.{func}-in', self.functions.keys()))
            function_input_rules.append(talent_rules)
        except:
            # if no triggers are given. It offers talent functions only now
            pass

        return super()._get_rules(OrRules(function_input_rules))

    async def _process_event(self, ev, cb = None):
        await super()._process_event(ev, self.__run_in_executor)

    async def __run_in_executor(self, ev, evtctx):
        asyncio.get_running_loop().run_in_executor(None, self.__start_process_event, ev, evtctx)

    def __start_process_event(self, ev, evtctx):
        loop = asyncio.new_event_loop()
        loop.run_until_complete(functools.partial(self.__process_function_events, ev=ev, evtctx=evtctx)())
        loop.close()

    async def __process_function_events(self, ev, evtctx):
        self.logger.info(f'Processing {ev["feature"]}')

        try:
            self.function_input_features.index(ev['feature'])
        except:
            self.logger.info(f'Feature not found in function inputs {self.function_input_features}')
            # Throws an error, if not found
            try:
                await self.on_event(ev, evtctx)
            except:
                self.logger.info(f'Error calling on_event')
                # on_event not implemented for function
                return

        print(f'Processing function for feature {ev["feature"]}')

        # Process function invocations
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