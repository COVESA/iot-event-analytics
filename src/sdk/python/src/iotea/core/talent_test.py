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
import time
import os
from asyncio import Event

from junit_xml import TestSuite, TestCase, to_xml_report_string, to_xml_report_file

# pylint: disable=wrong-import-position
from .talent import Talent
from .talent_func import FunctionTalent
from .rules import Rule, OrRules, OpConstraint
from .util.talent_io import TalentInput

from .constants import (
    PLATFORM_EVENTS_TOPIC,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG,
    GET_TEST_INFO_METHOD_NAME,
    PREPARE_TEST_SUITE_METHOD_NAME,
    RUN_TEST_METHOD_NAME,
    ENCODING_TYPE_OBJECT,
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
        if self.platform_event is not None:
            self.platform_event.set()

    def check(self, talent_id):
        # TODO Check needed or return None?
        return self.dependencies[talent_id]

    def check_all(self):
        # Collect the names of all unmet dependencies
        return [k for k, v in self.dependencies.items() if not v]

class TestSuiteInfo:
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
TEST_SUITE_METHODS = [GET_TEST_INFO_METHOD_NAME, PREPARE_TEST_SUITE_METHOD_NAME, RUN_TEST_METHOD_NAME]
INTEGRATION_TEST_SUBJECT = 'integration_test_subject'
INTEGRATION_TEST_INSTANCE = 'integration_test_instance'
class TestRunnerTalent(FunctionTalent):
    def __init__(self, name, config):
        self.config_json = config

        test_suites = config.get('testSuites', [])
        self.test_callees = [
            f'{test_suite}.{m}' for m in TEST_SUITE_METHODS for test_suite in test_suites]

        super().__init__(name, config['protocolGateway'])

        self.register_function('registerTestSuite', self.register_test_suite)

        self.output_file = config['outputFile']
        self.discover_dependencies_timeout = config.get(
            'discoverDependenciesTimeout', TIMEOUT)

        self.add_output('run-tests', {
            'description': 'Event to start the integration tests',
            'encoding': {
                'type': ENCODING_TYPE_OBJECT,
                'encoder': None
            }
        })
        self.platform_event = Event()
        self.test_suites = test_suites[:]

        dep_names = test_suites[:]
        dep_names.append(name)

        self.dependencies = TalentDependencies(
            dep_names, platform_event=self.platform_event)

        self.skip_cycle_check(True)


    async def register_test_suite(self, test_suite_name, ev, evtctx, timeout_at_ms):
        if self.config_json.get('testSuites', []):
            raise Exception('There are preconfigured testSuites for execution! Dynamic and static registration should not be mixed!')
        
        if test_suite_name not in self.test_suites:
            self.test_suites.append(test_suite_name)
            self.test_callees.extend([f'{test_suite_name}.{m}' for m in TEST_SUITE_METHODS])
            self.logger.info('A new test suite: %s was registered!', test_suite_name)
            return True
        return False

    async def start(self):
        await self.pg.subscribe_json(PLATFORM_EVENTS_TOPIC, self.dependencies.on_platform_event)
        loop = asyncio.get_event_loop()
        loop.create_task(super().start())
        self.logger.info('Test Runner started ....')
        
        # For backward compatibility and to easily handle multiple test suites run: if there's a 
        # list of predefined testSuites in config.json, run the tests at startup
        if self.config_json.get('testSuites', []):
            await self.trigger_test_run()
            
        while True:
            await asyncio.sleep(1)

    def callees(self):
        return self.test_callees

    def get_rules(self):
        return OrRules([
            Rule(OpConstraint(f"{self.id}.run-tests", OpConstraint.OPS['ISSET'],
                              None, DEFAULT_TYPE, VALUE_TYPE_RAW))
        ])

    # pylint: disable=invalid-name
    async def run_test_suite(self, ev, test_suite_name):
        result = True
        test_suite = None

        try:
            self.logger.info('Get Tests for %s', test_suite_name)
            test_suite = await self.call(test_suite_name, GET_TEST_INFO_METHOD_NAME,
                                       [], ev['subject'], ev['returnTopic'], 2000)

        # Pylint complains here but since call() raises a plain Exception we can't
        # be narrower in our except.
        # pylint: disable=broad-except
        except Exception as e:
            self.logger.error('Could not get TestSuiteInfo from %s (%s)', test_suite_name, e)
            return False, TestSuite(test_suite_name, [], None, f'Could not get TestSuiteInfo from {test_suite_name} ({e})')

        try:
            self.logger.info('Prepare %s', test_suite_name)
            prepared = await self.call(test_suite_name, PREPARE_TEST_SUITE_METHOD_NAME,
                                       [], ev['subject'], ev['returnTopic'], 2000)

            if not prepared:
                self.logger.error('Could not prepare, %s.%s returned False',
                                  test_suite_name, PREPARE_TEST_SUITE_METHOD_NAME)
                return False, TestSuite(test_suite_name, [], None,
                                        f'Could not prepare, {test_suite_name}.{PREPARE_TEST_SUITE_METHOD_NAME} returned False')

        # Pylint complains here but since call() raises a plain Exception we can't
        # be narrower in our except.
        # pylint: disable=broad-except
        except Exception as e:
            self.logger.error('Could not prepare %s (%s)', test_suite_name, e)
            return False, TestSuite(test_suite_name, [], None, f'Could not prepare, {test_suite_name} ({e})')

        self.logger.info('Running %s', test_suite_name)

        num_tests = len(test_suite['tests'])
        test_cases = []
        for idx in range(num_tests):
            test = test_suite['tests'][idx]
            expected = test['expectedValue']

            test_name = test['name']
            self.logger.info('[%d/%d] Running Test: %s', idx + 1, num_tests, test_name)
            self.logger.debug(' - Expected: %s', expected)

            try:
                test_result = await self.call(test_suite_name, 'runTest', [test_name],
                                              ev['subject'], ev['returnTopic'], test['timeout'])

                if test_result['actual'] == TEST_ERROR:
                    self.logger.info(' - Result: NOT OK ( % s.%s) returned TEST_ERROR',
                                     test_suite_name, RUN_TEST_METHOD_NAME)
                    test_case = TestCase(test['name'], elapsed_sec=duration / 1000)
                    test_case.add_error_info(
                        f'Result: NOT OK ({test_suite_name}.{RUN_TEST_METHOD_NAME}) returned TEST_ERROR')
                    test_cases.append(test_case)
                    return False, TestSuite(test_suite_name, test_cases)

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

        return result, TestSuite(test_suite_name, test_cases)

    def create_test_output(self, test_suites):
        self.logger.info(to_xml_report_string(test_suites))

        with open(self.output_file, 'w', encoding='utf-8') as f:
            to_xml_report_file(f, test_suites)
            self.logger.info('Junit xml output stored to %s', f.name)

    async def run_test_suites(self, ev):
        result = True

        test_suites = []
        for test_suite in self.test_suites:
            test_suite_result = await self.run_test_suite(ev, test_suite)
            self.logger.info('Result of %s is %s', test_suite, test_suite_result[0])
            test_suites.append(test_suite_result[1])

            if not test_suite_result[0]:
                result = False

        self.create_test_output(test_suites)
        return result


    async def on_event(self, ev, evtctx):
        self.logger.debug('Test Runner Received an event %s', ev)
        if ev['feature'] == f'{self.id}.run-tests':
            await self.trigger_test_run(ev)


    async def trigger_test_run(self, ev=None):
        try:
            await asyncio.wait_for(self.platform_event.wait(), self.discover_dependencies_timeout)
        except asyncio.exceptions.TimeoutError as e:
            self.logger.error('Timeout to gather dependencies expired. There are unmet dependencies.')
        
        unmet_dependencies = self.dependencies.check_all()

        if unmet_dependencies:
            self.logger.error("Can't start tests because of not connected "
                              'TestSuiteTalent(s): %s', unmet_dependencies)
            test_suites = []
            for unmet_dep in unmet_dependencies:
                test_suites.append(TestSuite(unmet_dep, [], stderr=f'Can not start tests because of not connected'
                                                                   f' TestSuiteTalent(s): {unmet_dep}'))
            self.create_test_output(test_suites)
            result = False
        else:
            self.logger.info('Start Integration Tests')
            initial_event = {
                'returnTopic': INGESTION_TOPIC,
                'subject': INTEGRATION_TEST_SUBJECT
            }
            #TODO Even though we're waiting for the platoform events of TestSuites to be discovered before sending method
            #invocations, there's a timeout calling the function getTestSuiteInfo
            #the problem happens only if TestRunner.registerTestSuite is invoked ???. If the suite is in config.json -
            #there's no such issue.
            if not self.config_json.get('testSuites', []):
                await asyncio.sleep(15)
            result = await self.run_test_suites(initial_event)
            self.logger.info('Overall test result is %s', result)
        
        result_event = {
            'subject': ev.get('subject') if ev is not None else INTEGRATION_TEST_SUBJECT,
            'type': ev.get('type') if ev is not None else 'default',
            'instance': ev.get('instance') if ev is not None else INTEGRATION_TEST_INSTANCE,
            'feature': 'testResultsHandler.test-result',
            'value': {'id': ev['value']['id'] if ev is not None else 1, 'result': result},
            'whenMs': round(time.time()*1000)
        }
        await self.pg.publish_json(INGESTION_TOPIC, result_event)

        if ev is None or ev['value']['exit']:
            # give time to publish the result before exiting
            await asyncio.sleep(5)
            if result:
                os._exit(0)
            else:
                # signal that tests have failed
                os._exit(1)
        
class TestSuiteTalent(FunctionTalent):
    def __init__(self, testsuite_name, protocol_gateway_config):
        super().__init__(testsuite_name, protocol_gateway_config)

        self.register_test_api_functions()
        self.test_suite_info = TestSuiteInfo(testsuite_name)
        self.platform_event = Event()
        self.talent_dependencies = TalentDependencies(['testRunner', testsuite_name], platform_event=self.platform_event)
     
    def callees(self):
        return ['testRunner.registerTestSuite']

    def register_test(self, test_name, expected_value, test_function, timeout=2000):
        test = Test(test_name, expected_value, test_function, timeout)
        self.test_suite_info.test_map[test_name] = test
    
    # A helper method triggering the execution of the registered test suites in the TestRunner.
    async def trigger_test_run(self, test_run_id=1, exit_flag=True):
        run_event = {
            "subject": INTEGRATION_TEST_SUBJECT,
            "type": "default",
            "instance": INTEGRATION_TEST_INSTANCE,
            "feature": "testRunner.run-tests",
            "value": {
                "id": test_run_id,
                "exit": exit_flag
            },
            'whenMs': round(time.time()*1000)
        }
        await self.pg.publish_json(INGESTION_TOPIC, run_event)

    # Registers the test suite with the TestRunner.        
    async def register(self):
        try:
            # wait for the runner and dependencies to be discovered
            await asyncio.wait_for(self.talent_dependencies.platform_event.wait(), TIMEOUT)
        except asyncio.exceptions.TimeoutError as e:
            self.logger.error('Timeout to gather dependencies expired. There are unmet dependencies.')
        result = await self.call('testRunner', 'registerTestSuite',
                        [self.id],
                        INTEGRATION_TEST_SUBJECT,
                        INGESTION_TOPIC,
                        2000)
        return result


    async def start(self):
        await self.pg.subscribe_json(PLATFORM_EVENTS_TOPIC, self.talent_dependencies.on_platform_event)
        loop = asyncio.get_event_loop()
        loop.create_task(super().start())
        self.logger.info('Test Suite started ....')
        try:
            # wait for the runner and dependencies to be discovered
            await asyncio.wait_for(self.talent_dependencies.platform_event.wait(), TIMEOUT)
        except asyncio.exceptions.TimeoutError as e:
            self.logger.error('Timeout to gather dependencies expired. There are unmet dependencies.')
        while True:
            await asyncio.sleep(1)


    def register_test_api_functions(self):
        self.register_function('getTestSuiteInfo', self.get_test_suite_info)
        self.register_function('prepare', self.prepare)
        self.register_function('runTest', self.run_test)

    # pylint: disable=unused-argument,invalid-name
    def get_test_suite_info(self, ev, evtctx, timeout_at_ms):
        return {
            'name': self.test_suite_info.name,
            'tests': self.test_suite_info.get_test_list()
        }

    async def run_test(self, test_name, ev, evtctx, timeout_at_ms):
        self.logger.info('Run Test %s', test_name)

        try:
            start = time.time()
            actual = await self.test_suite_info.test_map[test_name].func(ev, evtctx)
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
            self.logger.error('Prepare test suite failed because not connected '
                              'TestSuiteTalent(s): %s', unmet_dependencies)
            return False

        self.logger.debug('All talent dependencies resolved')
        return True


class TestResultsHandler(Talent):
    def __init__(self, config):
        super().__init__('testResultsHandler', config['protocolGateway'])
        self.add_output('test-result', {
            'description': 'Event which carries the results from integration test execution',
            'encoding': {
                'type': ENCODING_TYPE_OBJECT,
                'encoder': None
            }
        })
        self.skip_cycle_check(True)

    def get_rules(self):
        return OrRules([
            Rule(OpConstraint(f"{self.id}.test-result", OpConstraint.OPS['ISSET'],
                              None, DEFAULT_TYPE, VALUE_TYPE_RAW))])

    async def on_event(self, ev, evtctx):
        self.logger.info('TestResultsHandler received test result %s', TalentInput.get_raw_value(ev))


    async def start(self):
        loop = asyncio.get_event_loop()
        loop.create_task(super().start())
        self.logger.info('Test Results Handler started ....')
        while True:
            await asyncio.sleep(1)
