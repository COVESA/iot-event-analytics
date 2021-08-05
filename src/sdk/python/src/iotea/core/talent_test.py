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
import sys
import time
from asyncio import Event

from junit_xml import TestSuite, TestCase, to_xml_report_string, to_xml_report_file

# pylint: disable=wrong-import-position
from .talent import Talent
from .talent_func import FunctionTalent
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
    DEFAULT_TYPE,
    INGESTION_TOPIC
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
    def __init__(self, talent_ids=None, platform_event=None):
        self.dependencies = {}
        self.platform_event = platform_event
        if talent_ids:
            for tid in talent_ids:
                self.add_talent(tid)

    def add_talent(self, talent_id):
        self.dependencies[talent_id] = False

    def remove_talent(self, talent_id):
        self.dependencies.pop(talent_id)

    # pylint: disable=unused-argument,invalid-name
    async def on_platform_event(self, ev, topic, adapter_id=None):
        talent_id = ev['data']['talent']

        if ev['type'] == PLATFORM_EVENT_TYPE_SET_CONFIG:
            if talent_id in self.dependencies:
                self.dependencies[talent_id] = True
        elif ev['type'] == PLATFORM_EVENT_TYPE_UNSET_CONFIG:
            if talent_id in self.dependencies:
                self.dependencies[talent_id] = False

        for dep_met in self.dependencies.values():
            if not dep_met:
                return
        if self.platform_event is not None: self.platform_event.set()

    def check(self, talent_id):
        return self.dependencies[talent_id]  # TODO Check needed or return None?

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


# Default Timeout for the TestRunner to wait for all dependencies get discovered
TIMEOUT = 60


class TestRunnerTalent(Talent):
    def __init__(self, name, config):

        test_sets = config['testSets']
        methods = [GET_TEST_INFO_METHOD_NAME, PREPARE_TEST_SET_METHOD_NAME, RUN_TEST_METHOD_NAME]

        self.test_callees = [f'{test_set}.{m}' for m in methods for test_set in test_sets]

        super().__init__(name, config['protocolGateway'])

        self.output_file = config['outputFile']
        self.discover_dependencies_timeout = config.get('discoverDependenciesTimeout', TIMEOUT)

        # TODO run-tests should be made into constant
        self.add_output('run-tests', {
            'description': 'Event to start the integration tests',
            'encoding': {
                'type': ENCODING_TYPE_BOOLEAN,
                'encoder': None
            }
        })
        self.platform_event = Event()
        self.test_sets = test_sets[:]

        dep_names = test_sets[:]
        dep_names.append(name)

        self.dependencies = TalentDependencies(dep_names, platform_event=self.platform_event)

        self.skip_cycle_check(True)

    async def start(self):
        await self.pg.subscribe_json(PLATFORM_EVENTS_TOPIC, self.dependencies.on_platform_event)
        loop = asyncio.get_event_loop()
        loop.create_task(super().start())
        try:
            await asyncio.wait_for(self.platform_event.wait(), self.discover_dependencies_timeout)
        except asyncio.exceptions.TimeoutError as e:
            self.logger.error('Timeout to gather dependencies expired. There are unmet dependencies.')

        await self.trigger_test_run()

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
            return False, TestSuite(test_set_name, [], None, f'Could not get TestSetInfo from {test_set_name} ({e})')

        try:
            self.logger.info('Prepare %s', test_set_name)
            prepared = await self.call(test_set_name, PREPARE_TEST_SET_METHOD_NAME,
                                       [], ev['subject'], ev['returnTopic'], 2000)

            if not prepared:
                self.logger.error('Could not prepare, %s.%s returned False',
                                  test_set_name, PREPARE_TEST_SET_METHOD_NAME)
                return False, TestSuite(test_set_name, [], None,
                                        f'Could not prepare, {test_set_name}.{PREPARE_TEST_SET_METHOD_NAME} returned False')

        # Pylint complains here but since call() raises a plain Exception we can't
        # be narrower in our except.
        # pylint: disable=broad-except
        except Exception as e:
            self.logger.error('Could not prepare %s (%s)', test_set_name, e)
            return False, TestSuite(test_set_name, [], None, f'Could not prepare, {test_set_name} ({e})')

        self.logger.info('Running %s', test_set_name)

        num_tests = len(test_set['tests'])
        test_cases = []
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
                    test_case = TestCase(test['name'], elapsed_sec=duration / 1000)
                    test_case.add_error_info(
                        f'Result: NOT OK ({test_set_name}.{RUN_TEST_METHOD_NAME}) returned TEST_ERROR')
                    test_cases.append(test_case)
                    return (False, TestSuite(test_set_name, test_cases))

                actual = test_result['actual']
                duration = test_result['duration']

                self.logger.debug('- Actual: %s', actual)

                test_case = TestCase(test['name'], elapsed_sec=duration / 1000)
                test_cases.append(test_case)

                if expected == actual:
                    self.logger.info('- Result: OK (%dms)', duration)
                else:
                    self.logger.info(' - Result: NOT OK (%s != %s)', expected, actual)
                    result = False
                    test_case.add_failure_info(f'Result: NOT OK ({expected} != {actual})')

            # Pylint complains here but since call() raises a plain Exception we can't
            # be narrower in our except.
            # pylint: disable=broad-except
            except Exception as e:
                self.logger.info(' - Result: NOT OK Exception(%s)', e)
                test_case = TestCase(test['name'])
                test_case.add_error_info(f'Result: NOT OK Exception({e})')
                test_cases.append(test_case)

                result = False

        return result, TestSuite(test_set_name, test_cases)

    def create_test_output(self, test_suites):
        self.logger.info(to_xml_report_string(test_suites))

        with open(self.output_file, 'w', encoding='utf-8') as f:
            to_xml_report_file(f, test_suites)
            self.logger.info(f'Junit xml output stored to {f.name}')

    async def run_test_sets(self, ev):
        result = True

        test_suites = []
        for test_set in self.test_sets:
            test_set_result = await self.run_test_set(ev, test_set)
            self.logger.info('Result of %s is %s', test_set, test_set_result[0])
            test_suites.append(test_set_result[1])

            if not test_set_result:
                result = False

        self.create_test_output(test_suites)
        return result

    async def on_event(self, ev, evtctx):
        # not used
        pass

    async def trigger_test_run(self):
        unmet_dependencies = self.dependencies.check_all()

        if unmet_dependencies:
            self.logger.error("Can't start tests because of not connected "
                              'TestSetTalent(s): %s', unmet_dependencies)
            test_suites = []
            for unmet_dep in unmet_dependencies:
                test_suites.append(TestSuite(unmet_dep, [], stderr=f'Can not start tests because of not connected'
                                                                   f' TestSetTalent(s): {unmet_dep}'))
            self.create_test_output(test_suites)
            result = False
        else:
            self.logger.info('Start Integration Tests')
            initial_event = {
                'returnTopic': INGESTION_TOPIC,
                'subject': "integration_test"
            }

            result = await self.run_test_sets(initial_event)
            self.logger.info('Overall test result is %s', result)

        if result:
            sys.exit(0)
        else:
            #signal that tests have failed
            sys.exit(1)


class TestSetTalent(FunctionTalent):
    def __init__(self, testset_name, protocol_gateway_config):
        super().__init__(testset_name, protocol_gateway_config)

        self.register_test_api_functions()
        self.test_set_info = TestSetInfo(testset_name)
        self.talent_dependencies = TalentDependencies()

    def register_test(self, test_name, expected_value, test_function, timeout=2000):
        test = Test(test_name, expected_value, test_function, timeout)
        self.test_set_info.test_map[test_name] = test

    async def start(self):
        await self.pg.subscribe_json(PLATFORM_EVENTS_TOPIC,
                                     self.talent_dependencies.on_platform_event)
        await super().start()

    def register_test_api_functions(self):
        self.register_function('getTestSetInfo', self.get_test_set_info)
        self.register_function('prepare', self.prepare)
        self.register_function('runTest', self.run_test)

    # pylint: disable=unused-argument,invalid-name
    def get_test_set_info(self, ev, evtctx, timeout_at_ms):
        return {
            'name': self.test_set_info.name,
            'tests': self.test_set_info.get_test_list()
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

