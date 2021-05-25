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

from .talent import Talent
from .constants import ENCODING_TYPE_OBJECT, ENCODING_TYPE_ANY, DEFAULT_TYPE, MSG_TYPE_ERROR, MAX_TALENT_EVENT_WORKER_COUNT
from .rules import OrRules, SchemaConstraint, Rule, Constraint
from .util.talent_io import TalentInput, TalentOutput

class FunctionTalent(Talent):
    def __init__(self, id, protocol_gateway_config, talent_config={}, max_threadpool_workers=MAX_TALENT_EVENT_WORKER_COUNT):
        super().__init__(id, protocol_gateway_config, talent_config, max_threadpool_workers)
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

    def get_rules(self):
        # It's not required to be overridden
        return None

    def _get_rules(self):
        function_names = self.functions.keys()

        if len(function_names) == 0:
            # returns 1) or 2) -> see Talent._get_rules()
            rules = super()._get_rules()

            if rules is None:
                # getRules() not overridden, callees() returns empty array, no functions registered
                raise Exception('You have to at least register a function or override the get_rules() method.')

            return rules

        """
        3) OR --> Non-triggerable Function Talent, which does not call any functions by itself
             function input rules

        4) OR --> Triggerable Function Talent, which does not call any functions by itself
             function input rules
             OR/AND [exclude function input rules]
               triggerRules

        5) OR --> Triggerable Function Talent, which calls one or more functions
             function output rules i.e. callee rules
             OR [exclude function output rules]
               function result rules
               OR/AND [exclude function result rules]
                 triggerRules
        """

        function_input_rules = OrRules([])

        for function_name in function_names:
            event_schema = {
                'type': 'object',
                'required': [
                    'func',
                    'args',
                    'chnl',
                    'call',
                    'timeoutAtMs'
                ],
                'properties': {
                    'func': {
                        'type': 'string',
                        'const': function_name
                    },
                    'args': {
                        'type': 'array'
                    },
                    'chnl': {
                        'type': 'string'
                    },
                    'call': {
                        'type': 'string'
                    },
                    'timeoutAtMs': {
                        'type': 'integer'
                    }
                },
                'additionalProperties': False
            }

            function_input_rules.add(
                Rule(SchemaConstraint(f'{self.id}.{function_name}-in', event_schema, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW']))
            )

        trigger_rules = self.get_rules()
        function_result_rules = self._get_function_result_rules()

        if trigger_rules is None and function_result_rules is None:
            # return 3)
            return function_input_rules

        if trigger_rules is not None:
            trigger_rules.exclude_on = list(map(lambda function_name: f'{DEFAULT_TYPE}.{self.id}.{function_name}-in', function_names))
            function_input_rules.add(trigger_rules)

            if function_result_rules is None:
                # return 4)
                return function_input_rules

        function_input_rules.exclude_on = list(map(lambda callee: f'{DEFAULT_TYPE}.{callee}-out', self.callees()))
        function_result_rules.add(function_input_rules)

        # return 5)
        return function_result_rules

    async def _process_event(self, ev, cb = None):
        return await super()._process_event(ev, self.__process_function_events)

    async def __process_function_events(self, ev, evtctx):
        self.logger.info(f'Processing {ev["feature"]}')

        try:
            self.function_input_features.index(ev['feature'])
        except:
            self.logger.info(f'Feature not found in function inputs {self.function_input_features}')

            # Throws an error, if not found
            try:
                return await self.on_event(ev, evtctx)
            except Exception as err:
                # on_event not implemented or execution error occurred calling on_event
                self.logger.warning(err)
                return

        # Process function invocations
        raw_value = TalentInput.get_raw_value(ev)

        args = [*raw_value['args'], ev, evtctx, raw_value['timeoutAtMs']]

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

        return [ result_event ]