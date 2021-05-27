##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import time
import json

from .talent import Talent
from .rules import Rule, Constraint, OpConstraint, AndRules, OrRules
from .constants import DEFAULT_TYPE, VALUE_TYPE_RAW, DEFAULT_INSTANCE
from .util.talent_io import TalentInput, TalentOutput

class Mapper(Talent):
    def __init__(self, mapper_id, reducer_id, protocol_gateway_config):
        super().__init__(mapper_id, protocol_gateway_config)
        self.reducer_id = reducer_id

        self.skip_cycle_check_for(
            self.__get_reduce_end_feature(DEFAULT_TYPE),
            self.__get_map_start_feature(DEFAULT_TYPE)
        )
        self.add_output(Mapper.FEATURE_MAP_START, {
            'encoding':{
                'type': 'number',
                'encoder': None
            },
            'description': 'Mapping has started at that given time in ms since 01.01.1970',
            'default': -1
        })

        self.add_output(Mapper.FEATURE_MAP_ASSIGN, {
            'encoding': {
                'type': 'any',
                'encoder': None
            },
            'description': 'Single input for worker'
        })

        self.add_output(Mapper.FEATURE_MAP_PARTIAL, {
            'encoding': {
                'type': 'object',
                'encoder': None
            },
            'default': [],
            'description': 'Output for all workers'
        })

    def get_trigger_rules(self):
        raise Exception('Override get_trigger_rules()')

    def get_rules(self):
        rules = [
            Rule(OpConstraint(self.__get_map_start_feature(), OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'])),
            Rule(OpConstraint(self.__get_reduce_end_feature(), OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW']))
        ]

        # pylint: disable=assignment-from-no-return
        trigger_rules = self.get_trigger_rules()

        if isinstance(trigger_rules, OrRules) is False and isinstance(trigger_rules, AndRules) is False and isinstance(trigger_rules, Rule) is False:
            raise Exception('get_trigger_rules() has to be an instance either of OrRules or AndRules or Rule')

        return OrRules([AndRules(rules), trigger_rules])

    async def map(self, ev):
        raise Exception('Override map(ev) and return an array. Each array entry will be sent to a worker.')

    async def on_event(self, ev, evtctx):
        if (ev['feature'] == self.__get_map_start_feature() or ev['feature'] == self.__get_reduce_end_feature()):
            # These events should not trigger any calculation process
            return

        map_start_at = -1
        reduce_end_at = -1

        try:
            map_start_at = TalentInput.get_raw_value(ev, 1, False, self.__get_map_start_feature(), DEFAULT_TYPE, DEFAULT_INSTANCE)
            reduce_end_at = TalentInput.get_raw_value(ev, 1, False, self.__get_reduce_end_feature(), DEFAULT_TYPE, DEFAULT_INSTANCE)
        except:
            # Omit error
            pass

        if map_start_at > 0 and map_start_at > reduce_end_at:
            # Reduction process is currently running
            self.logger.info('Waiting for reduction to finish...')
            return

        work_packages = await self.map(ev)

        self.logger.info(('Mapper {} distributes work at {}. Work packages {}'.format(self.id, ev['whenMs'], json.dumps(work_packages))), extra=self.logger.create_extra(evtctx))

        partial_results = [None for _ in range(len(work_packages))]

        talent_output = TalentOutput()

        now = round(time.time() * 1000)

        # Store mapping start time
        talent_output.add(self, ev, Mapper.FEATURE_MAP_START, now)

        # Place for workers to write result
        talent_output.add(self, ev, Mapper.FEATURE_MAP_PARTIAL, partial_results)

        # Jobs for workers
        # Ensure different timestamps for all work packages
        for i, work_package in enumerate(work_packages):
            talent_output.add(self, ev, Mapper.FEATURE_MAP_ASSIGN, {
                'idx': i,
                'value': work_package
            }, ev['subject'], DEFAULT_TYPE, DEFAULT_INSTANCE, now + i)

        return talent_output.to_json()

    def __get_reduce_end_feature(self, _type=None):
        return self.get_full_feature(self.reducer_id, Reducer.FEATURE_MAP_END, _type)

    def __get_map_start_feature(self, _type=None):
        return self.get_full_feature(self.id, Mapper.FEATURE_MAP_START, _type)

    def __get_map_partial_feature(self, _type=None):
        return self.get_full_feature(self.id, Mapper.FEATURE_MAP_PARTIAL, _type)

    def __get_map_assign_feature(self, _type=None):
        return self.get_full_feature(self.id, Mapper.FEATURE_MAP_ASSIGN, _type)

Mapper.FEATURE_MAP_START = 'map_start'
Mapper.FEATURE_MAP_ASSIGN = 'map_assign'
Mapper.FEATURE_MAP_PARTIAL = 'map_partial'

class Worker(Talent):
    def __init__(self, worker_id, mapper_id, protocol_gateway_config):
        super().__init__(worker_id, protocol_gateway_config)
        self.mapper_id = mapper_id

    def get_rules(self):
        return AndRules(
            [Rule(OpConstraint(self.__get_map_assign_feature(), OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, VALUE_TYPE_RAW))]
        )

    async def work(self, data):
        raise Exception('Override work(data)')

    async def on_event(self, ev, evtctx):
        data = TalentInput.get_raw_value(ev)

        partial_index = data['idx']
        work_package = data['value']

        self.logger.debug('Worker calculates value for index {} with data {}'.format(partial_index, json.dumps(work_package)), extra=self.logger.create_extra(evtctx))

        try:
            partial_result = await self.work(work_package)
        # pylint: disable=broad-except
        except Exception as ex:
            self.logger.warning('Worker failed', exc_info=ex, extra=self.logger.create_extra(evtctx))
            partial_result = Worker.ERROR

        return [
            TalentOutput.create_for(ev['subject'], DEFAULT_TYPE, DEFAULT_INSTANCE, self.__get_map_partial_feature(), {
                'value': partial_result,
                '$part': partial_index
            })
        ]

    def __get_map_partial_feature(self, _type=None):
        return self.get_full_feature(self.mapper_id, Mapper.FEATURE_MAP_PARTIAL, _type)

    def __get_map_assign_feature(self, _type=None):
        return self.get_full_feature(self.mapper_id, Mapper.FEATURE_MAP_ASSIGN, _type)

Worker.ERROR = 'ERROR'

class Reducer(Talent):
    def __init__(self, reducer_id, mapper_id, protocol_gateway_config):
        super().__init__(reducer_id, protocol_gateway_config)
        self.mapper_id = mapper_id

        self.skip_cycle_check_for(self.__get_reduce_end_feature(DEFAULT_TYPE))

        self.add_output(Reducer.FEATURE_MAP_END, {
            'description': 'Reduction has ended at that given time in ms since 01.01.1970',
            'encoding': {
                'type': 'number',
                'encoder': None
            },
            'default': -1
        })

    def get_rules(self):
        return AndRules([
            Rule(OpConstraint(self.__get_map_start_feature(), OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, VALUE_TYPE_RAW)),
            Rule(OpConstraint(self.__get_map_partial_feature(), OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, VALUE_TYPE_RAW, '[:]')),
            Rule(OpConstraint(self.__get_reduce_end_feature(), OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, VALUE_TYPE_RAW))
        ])

    async def reduce(self, data):
        raise Exception('Override reduce(data)')

    async def on_event(self, ev, evtctx):
        if ev['feature'] != self.__get_map_partial_feature():
            return

        if TalentInput.get_raw_value(ev, 1, False, self.__get_map_start_feature(), DEFAULT_TYPE, DEFAULT_INSTANCE) <= TalentInput.get_raw_value(ev, 1, False, self.__get_reduce_end_feature(), DEFAULT_TYPE, DEFAULT_INSTANCE):
            # No calculation is currently pending
            return

        reduced_result_features = None

        try:
            reduced_result_features = await self.reduce(TalentInput.get_raw_value(ev))
        # pylint: disable=broad-except
        except Exception as ex:
            self.logger.warning('Error reducing values', exc_info=ex, extra=self.logger.create_extra(evtctx))

        talent_output = TalentOutput()

        talent_output.add(self, ev, Reducer.FEATURE_MAP_END, round(time.time() * 1000))

        if isinstance(reduced_result_features, list):
            for result_feature in reduced_result_features:
                talent_output.outputs.append(result_feature)

        return talent_output.to_json()

    def __get_reduce_end_feature(self, _type=None):
        return self.get_full_feature(self.id, Reducer.FEATURE_MAP_END, _type)

    def __get_map_start_feature(self, _type=None):
        return self.get_full_feature(self.mapper_id, Mapper.FEATURE_MAP_START, _type)

    def __get_map_partial_feature(self, _type=None):
        return self.get_full_feature(self.mapper_id, Mapper.FEATURE_MAP_PARTIAL, _type)

Reducer.FEATURE_MAP_END = 'map_end'
