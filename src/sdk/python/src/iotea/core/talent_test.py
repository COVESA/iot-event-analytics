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

# pylint: disable=wrong-import-position
from .talent_func import FunctionTalent
from .talent import Talent
from .rules import Rule, OrRules, OpConstraint

from .constants import (
    PLATFORM_EVENTS_TOPIC,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG,
    GET_TEST_INFO_METHOD_NAME,
    PREPARE_TEST_SET_METHOD_NAME,
    RUN_TEST_METHOD_NAME,
    ENCODING_TYPE_BOOLEAN,
    TEST_ERROR,
    VALUE_TYPE_RAW,
    DEFAULT_TYPE
)

# pylint: disable=too-few-public-methods
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
        return {
            'name': self.name,
            'actual': self.actual,
            'duration': self.duration
        }


class TalentDependencies:
    def __init__(self, talent_ids=None):
        self.dependencies = {}

        if talent_ids:
            for tid in talent_ids:
                self.add_talent(tid)

    def add_talent(self, talent_id):
        self.dependencies[talent_id] = False

    def remove_talent(self, talent_id):
        self.dependencies.pop(talent_id)

    # pylint: disable=unused-argument,invalid-name
    async def on_platform_event(self, ev, evctx):
        talent_id = ev['data']['talent']

        if ev['type'] == PLATFORM_EVENT_TYPE_SET_CONFIG:
            if talent_id in self.dependencies:
                self.dependencies[talent_id] = True
        elif ev['type'] == PLATFORM_EVENT_TYPE_UNSET_CONFIG:
            if talent_id in self.dependencies:
                self.dependencies[talent_id] = False

    def check(self, talent_id):
        return self.dependencies[talent_id] # TODO Check needed or return None?

    def check_all(self):
        # Collect the names of all unmet dependencies
        return [k for k, v in self.dependencies.items() if not v]


class TestSetInfo:
    def __init__(self, name):
        self.name = name
        self.test_map = {}

    def get_test_list(self):
        return [{
            'name': v.name,
            'expectedValue': v.expected_value,
            'timeout': v.timeout
        } for v in self.test_map.values()]


class TestRunnerException(Exception):
    pass


class TestRunnerTalent(Talent):
    def __init__(self, name, test_sets, connection_string):
        methods = [GET_TEST_INFO_METHOD_NAME, PREPARE_TEST_SET_METHOD_NAME, RUN_TEST_METHOD_NAME]

        self.test_callees = [f'{test_set}.{m}' for m in methods for test_set in test_sets]

        super(TestRunnerTalent, self).__init__(name, connection_string)

        # TODO run-tests should be made into constant
        self.add_output('run-tests', {
            'description': 'Event to start the integration tests',
            'encoding': {
                'type': ENCODING_TYPE_BOOLEAN,
                'encoder': None
            }
        })

        self.test_sets = test_sets[:]
        self.dependencies = TalentDependencies(test_sets)

        self.skip_cycle_check(True)

    async def start(self):
        await self.mqtt_client.subscribe_json(PLATFORM_EVENTS_TOPIC, self.dependencies.on_platform_event)
        await super(TestRunnerTalent, self).start()

    def callees(self):
        return self.test_callees

    def get_rules(self):
        return OrRules([
            Rule(OpConstraint(f"{self.id}.run-tests", OpConstraint.OPS['ISSET'],
                              None, DEFAULT_TYPE, VALUE_TYPE_RAW))
        ])

    # pylint: disable=invalid-name
    async def run_test_set(self, ev, test_set_name):
        result = True
        test_set = None

        try:
            self.logger.info('Get Tests for %s', test_set_name)
            test_set = await self.call(test_set_name, GET_TEST_INFO_METHOD_NAME,
                                       [], ev['subject'], ev['returnTopic'], 2000)

        # Pylint complains here but since call() raises a plain Exception we can't
        # be narrower in our except.
        # pylint: disable=broad-except
        except Exception as e:
            self.logger.error('Could not get TestSetInfo from %s (%s)', test_set_name, e)
            return False

        try:
            self.logger.info('Prepare %s', test_set_name)
            prepared = await self.call(test_set_name, PREPARE_TEST_SET_METHOD_NAME,
                                       [], ev['subject'], ev['returnTopic'], 2000)

            if not prepared:
                self.logger.error('Could not prepare, %s.%s returned False',
                                  test_set_name, PREPARE_TEST_SET_METHOD_NAME)
                return False

        # Pylint complains here but since call() raises a plain Exception we can't
        # be narrower in our except.
        # pylint: disable=broad-except
        except Exception as e:
            self.logger.error('Could not prepare %s (%s)', test_set_name, e)
            return False

        self.logger.info('Running %s', test_set_name)

        num_tests = len(test_set['tests'])
        for idx in range(num_tests):
            test = test_set['tests'][idx]
            expected = test['expectedValue']

            test_name = test['name']
            self.logger.info('[%d/%d] Running Test: %s', idx + 1, num_tests, test_name)
            self.logger.debug(' - Expected: %s', expected)

            try:
                test_result = await self.call(test_set_name, 'runTest', [test_name],
                                              ev['subject'], ev['returnTopic'], test['timeout'])

                if test_result['actual'] == TEST_ERROR:
                    self.logger.info(' - Result: NOT OK ( % s.%s) returned TEST_ERROR',
                                     test_set_name, RUN_TEST_METHOD_NAME)
                    return

                actual = test_result['actual']
                duration = test_result['duration']

                self.logger.debug('- Actual: %s', actual)

                if expected == actual:
                    self.logger.info('- Result: OK (%dms)', duration)
                else:
                    self.logger.info(' - Result: NOT OK (%s != %s)', expected, actual)
                    result = False

            # Pylint complains here but since call() raises a plain Exception we can't
            # be narrower in our except.
            # pylint: disable=broad-except
            except Exception as e:
                self.logger.info(' - Result: NOT OK Exception(%s)', e)
                result = False

        return result

    async def run_test_sets(self, ev):
        result = True

        for test_set in self.test_sets:
            test_set_result = await self.run_test_set(ev, test_set)
            self.logger.info('Result of %s is %s', test_set, test_set_result)

            if not test_set_result:
                result = False

        return result

    async def on_event(self, ev, evtctx):
        unmet_dependencies = self.dependencies.check_all()

        if unmet_dependencies:
            self.logger.error("Can't start tests because of not connected "
                              'TestSetTalent(s): %s', unmet_dependencies)
            return

        self.logger.info('Start Integration Tests')
        result = await self.run_test_sets(ev)
        self.logger.info('Overall test result is %s', result)


class TestSetTalent(FunctionTalent):
    def __init__(self, testset_name, connection_string):
        super().__init__(testset_name, connection_string)

        self.register_test_api_functions()
        self.test_set_info = TestSetInfo(testset_name)
        self.talent_dependencies = TalentDependencies()

    def register_test(self, test_name, expected_value, test_function, timeout=2000):
        test = Test(test_name, expected_value, test_function, timeout)
        self.test_set_info.test_map[test_name] = test

    async def start(self):
        await self.mqtt_client.subscribe_json(PLATFORM_EVENTS_TOPIC,
                                         self.talent_dependencies.on_platform_event)
        await super().start()

    def register_test_api_functions(self):
        self.register_function('getTestSetInfo', self.get_test_set_info)
        self.register_function('prepare', self.prepare)
        self.register_function('runTest', self.run_test)

    # pylint: disable=unused-argument,invalid-name
    def get_test_set_info(self, ev, evtctx, timeout_at_ms):
        return {
            'name' : self.test_set_info.name,
            'tests' : self.test_set_info.get_test_list()
        }

    async def run_test(self, test_name, ev, evtctx, timeout_at_ms):
        self.logger.info('Run Test %s', test_name)

        try:
            start = time.time()
            actual = await self.test_set_info.test_map[test_name].func(ev, evtctx)
            duration = round((time.time() - start) * 1000)
        except KeyError:
            self.logger.error('Test %s has not been registered', test_name)

             # Has to be a dict because of json serialization
            return TestResult(test_name, TEST_ERROR, -1).to_dict()

        # Has to be a dict because of json serialization
        return TestResult(test_name, actual, duration).to_dict()

    async def prepare(self, ev, evtctx, timeout_at_ms):
        unmet_dependencies = self.talent_dependencies.check_all()

        if unmet_dependencies:
            self.logger.error('Prepare test set failed because not connected '
                              'TestSetTalent(s): %s', unmet_dependencies)
            return False

        self.logger.debug('All talent dependencies resolved')
        return True

        # TODO we need to check the JS code for this also
