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

# pylint: disable=wrong-import-position
from iotea.core.talent_func import FunctionTalent
from iotea.core.talent import Talent
from iotea.core.rules import Rule, OrRules, OpConstraint
from iotea.core.constants import *

# Setup logging for debugging 
from iotea.core.logger import Logger

class Test:
    def __init__(self, name, expected_value, func, timeout=2000):
        self.name = name
        self.expected_value = expected_value
        self.func = func
        self.timeout = timeout

class TestResult:
    def __init__(self, name, actual, duration):
        self.name = name
        self.actual = actual
        self.duration = duration
    
    def to_dict(self):
        test_result_dict ={
            'name': self.name,
            'actual': self.actual,
            'duration': self.duration
        }

        return test_result_dict


class TalentDependencies:
    def __init__(self):
        self.talent_dependencies = dict()

    def add_talent(self, talent_id):
        self.talent_dependencies[talent_id] = False

    def remove_talent(self, talent_id):
        self.talent_dependencies.pop(talent_id)

    async def on_platform_event(self, ev, evctx):
        talent_id = ev['data']['talent']

        if ev['type'] == PLATFORM_EVENT_TYPE_SET_RULES: 
            if talent_id in self.talent_dependencies: # TODO Use Constants
                self.talent_dependencies[talent_id] = True
        elif ev['type'] == PLATFORM_EVENT_TYPE_UNSET_RULES:
            if talent_id in self.talent_dependencies:
                self.talent_dependencies[talent_id] = False

    def check(self, talent_id):
        return self.talent_dependencies[talent_id] # TODO Check needed or return None?

    def check_all(self):
        not_connected_array = []
        result = True

        # TODO Make sure this same fix is in the JS code please!
        if len(self.talent_dependencies) > 0:
            for key in self.talent_dependencies:
                value = self.talent_dependencies[key]
                if value is False:
                    not_connected_array.append(key)
                    result = False

        return result, not_connected_array

class TestSetInfo:
    def __init__(self, name):
        self.name = name
        self.test_map = dict()

    def get_test_list(self):
        test_list = []
        for key in self.test_map:
            item = self.test_map[key]
            test = dict()
            test['name'] = item.name # TODO Use constants for Test API stuff
            test['expectedValue'] = item.expected_value
            test['timeout'] = item.timeout

            test_list.append(test)

        return test_list

# TODO Add Test Runner Talent
class TestRunnerTalent(Talent):
    def __init__(self, name, test_set_list, connection_string):
        super(TestRunnerTalent, self).__init__(name, connection_string)

        # TODO run-tests should be made into constant
        self.add_output('run-tests', {
            'description': 'Event to start the integration tests',
            'encoding': {
                'type': ENCODING_TYPE_BOOLEAN,
                'encoder': None
            }
        })

        self.callee_array = []
        self.test_set_array = []
        self.talent_dependencies = TalentDependencies()

        # TODO could Use a list comprehension - couldn't get my head around the multiple operations
        for test_set in test_set_list:
            
            # Set Test Callees (Test API)
            self.callee_array.append(f"{test_set}.{GET_TEST_INFO_METHOD_NAME}")
            self.callee_array.append(f"{test_set}.{PREPARE_TEST_SET_METHOD_NAME}")
            self.callee_array.append(f"{test_set}.{RUN_TEST_METHOD_NAME}")

            # Add talent id to dependency
            self.talent_dependencies.add_talent(test_set)
            self.test_set_array.append(test_set)

        self.skip_cycle_check(True)
    
    async def start(self):
        await self.broker.subscribe_json(PLATFORM_EVENTS_TOPIC, self.talent_dependencies.on_platform_event)
        await super(TestRunnerTalent, self).start()

    def callees(self):
        return self.callee_array
    
    def get_rules(self):
        return OrRules([
            Rule(OpConstraint(f"{self.id}.run-tests", OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, VALUE_TYPE_RAW))
        ])
    
    async def run_test_set(self, ev, test_set_name):
        result = True
        test_set = None

        try: 
           self.logger.info(f'Get Tests for {test_set_name}')
           test_set = await self.call(test_set_name, GET_TEST_INFO_METHOD_NAME, [], ev['subject'], ev['returnTopic'], 2000)

        except Exception as e:
            self.logger.error(f'Could not get TestSetInfo from {test_set_name} ({e})')
            return False

        try:
            self.logger.info(f'Prepare {test_set_name}')
            prepared = await self.call(test_set_name, PREPARE_TEST_SET_METHOD_NAME, [], ev['subject'], ev['returnTopic'], 2000)
           
            if prepared is False:
                raise 'Result of prepare was false'

        except Exception as e:
            self.logger.error(f'Could not prepare {test_set_name} ({e})')
            return False
        
        self.logger.info(f'Running {test_set_name}')

        num_tests = len(test_set['tests'])
        for idx in range(num_tests):
            test = test_set['tests'][idx]
            expected = test['expectedValue']

            test_name = test['name']
            self.logger.info(f'[{idx + 1}/{num_tests}] Running Test: {test_name}')
            self.logger.debug(f' - Expected: {expected}')

            try:
                test_result = await self.call(test_set_name, 'runTest', [ test_name ], ev['subject'], ev['returnTopic'], test['timeout'])

                if test_result['actual'] == TEST_ERROR:
                    raise 'TEST_ERROR returned'

                actual = test_result['actual']
                duration = test_result['duration']

                self.logger.debug(f'- Actual: {actual}')

                if expected == actual:
                    self.logger.info(f'- Result: OK ({duration}ms)')
                    continue
                else:
                    self.logger.info(f' - Result: NOT OK ({expected} != {actual})')
                    result = False
                    continue

            except Exception as e:
                self.logger.info(f' - Result: NOT OK Exception({e})')
                result = False
                continue

        return result

    async def run_test_sets(self, ev):
        result = True

        for test_set in self.test_set_array:
            test_set_result = await self.run_test_set(ev, test_set)
            self.logger.info(f'Result of {test_set} is {test_set_result}')

            if test_set_result == False:
                result = False
        return result
            
    async def on_event(self, ev, evtctx):
        result, not_connected_array = self.talent_dependencies.check_all()
        
        self.logger.info('NOT site-pkg talent_test.py')

        if result is True:
            self.logger.info('Start Integration Tests')
            result = await self.run_test_sets(ev)
            self.logger.info(f'Overall test result is {result}')
        else:
            self.logger.error(f'Can\'t start tests because of not connected TestSetTalent(s): {not_connected_array}')
        

class TestSetTalent(FunctionTalent):
    def __init__(self, testset_name, connection_string):
        super(TestSetTalent, self).__init__(testset_name, connection_string)

        self.register_test_api_functions()
        self.test_set_info = TestSetInfo(testset_name)
        self.talent_dependencies = TalentDependencies()


    def register_test(self, test_name, expected_value, test_function, timeout=2000):
        test = Test(test_name, expected_value, test_function, timeout)
        self.test_set_info.test_map[test_name] = test


    async def start(self):
        await self.broker.subscribe_json(PLATFORM_EVENTS_TOPIC, self.talent_dependencies.on_platform_event)
        await super(TestSetTalent, self).start()


    def register_test_api_functions(self):
        self.register_function('getTestSetInfo', self.get_test_set_info)
        self.register_function('prepare', self.prepare)
        self.register_function('runTest', self.run_test)


    def get_test_set_info(self, ev, evctx):
        return {
            'name' : self.test_set_info.name,
            'tests' : self.test_set_info.get_test_list()
        }

    async def run_test(self, test_name, ev, evtctx):
        self.logger.info("Run Test {}".format(test_name))

        if test_name in self.test_set_info.test_map:
            start = time.time()
            actual = await self.test_set_info.test_map[test_name].func(ev, evtctx)
            duration = round((time.time() - start) * 1000)
            return TestResult(test_name, actual, duration).to_dict() # Has to be a dict because of json serialization
        else:
            self.logger.error("Test {} has not been registered".format(test_name))
            return TestResult(test_name, TEST_ERROR, -1).to_dict() # Has to be a dict because of json serialization


    async def prepare(self, ev, evtctx):
        result, not_connected_dependencies = self.talent_dependencies.check_all()

        if result is True:
            self.logger.debug("All talent dependencies resolved")
            return True
        else:
            self.logger.error("Prepare test set failed because not connected TestSetTalent(s): {}". format(not_connected_dependencies))
            return False
            # TODO we need to check the JS code for this also 
