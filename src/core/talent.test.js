const Talent = require('./talent');
const FunctionTalent = require('./talent.func.js')

const {
    Rule,
    OrRules,
    OpConstraint
} = require('./rules');

const {
    DEFAULT_TYPE,
    VALUE_TYPE_RAW,
    ENCODING_TYPE_BOOLEAN,
    GET_TEST_INFO_METHOD_NAME,
    PREPARE_TEST_SUITE_METHOD_NAME,
    RUN_TEST_METHOD_NAME,
    TEST_ERROR,
    PLATFORM_EVENTS_TOPIC,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG,
    INGESTION_TOPIC
} = require('./constants');

const Logger = require('./util/logger');

process.env.MQTT_TOPIC_NS = 'iotea/';
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

class Test {
    constructor(name, expectedValue, func, timeout = 2000) {
        this.name = name;
        this.expectedValue = expectedValue;
        this.func = func;
        this.timeout = timeout;
    }
}

class TestResult {
    constructor(name, actual, duration) {
        this.name = name;
        this.actual = actual;
        this.duration = duration;
    }
}

class TalentDependencies {
    constructor() {
        this.talentDependencies = new Map();
        this.callbacks = [];
    }

    addTalent(talentId) {
        this.talentDependencies.set(talentId, false);
    }

    removeTalent(talentId) {
        this.talentDependencies.delete(talentId);

    }

    async __onPlatformEvent(ev) {
        if (ev.type == PLATFORM_EVENT_TYPE_SET_CONFIG) {
            if (this.talentDependencies.has(ev.data.talent)) {
                this.talentDependencies.set(ev.data.talent, true);
            }

        } else if (ev.type == PLATFORM_EVENT_TYPE_UNSET_CONFIG) {
            if (this.talentDependencies.has(ev.data.talent)) {
                this.talentDependencies.set(ev.data.talent, false);
            }
        }

        if (this.checkAll().length == 0) {
            // Notify those who are waiting for us
            this.callbacks.forEach( callback => callback())

            // Clear callbacks
            this.callbacks = [];
        }
    }

    check(talentId) {
        return this.talentDependencies.get(talentId);
    }

    checkAll() {
        // Return the names of all unmet dependencies
        return Array.from(this.talentDependencies).filter(e => !e[1]).map(e => e[0]);
    }

    waitForDependencies(timeoutMs) {
        const timeoutPromise = new Promise((resolve, reject) => {
            setTimeout(() => {
                reject('Timeout waiting for dependencies');
            }, timeoutMs);
        });

        const dependenciesMetPromise = new Promise((resolve, reject) => {
            if (this.checkAll().length == 0) {
                resolve();
            } else {
                this.callbacks.push(resolve);
            }
        });

        return Promise.race([timeoutPromise, dependenciesMetPromise]);
    }
}

class TestSuiteInfo {
    constructor(name) {
        this.name = name
        this.testMap = new Map();
    }

    getTestList() {
        return Array.from(this.testMap.values()).map(test => {
            const { func, ...rest } = test;
            return rest;
        });
    }
}

class TestRunnerTalent extends Talent {
    constructor(name, config) {
        super(name, config.get('protocolGateway'));

        // Define run-tests feature for this test runner automatically at runtime
        this.addOutput('run-tests', {
            description: 'Event to start the integration tests',
            encoding: {
                type: ENCODING_TYPE_BOOLEAN,
                encoder: null
            },
        });

        this.calleeArray = new Array();
        this.testSuiteArray = new Array();
        this.talentDependencies = new TalentDependencies();
        
        const testSuiteList = config.get('testSuites')
        
        testSuiteList.forEach(testSuite => {
            let testSuiteTalentId = testSuite;
            this.testSuiteArray.push(testSuiteTalentId);

            // Set Test Callees (Test API)
            this.calleeArray.push(`${testSuiteTalentId}.${GET_TEST_INFO_METHOD_NAME}`);
            this.calleeArray.push(`${testSuiteTalentId}.${PREPARE_TEST_SUITE_METHOD_NAME}`);
            this.calleeArray.push(`${testSuiteTalentId}.${RUN_TEST_METHOD_NAME}`);

            // Add talent id to dependency
            this.talentDependencies.addTalent(testSuiteTalentId);
        });

        // Wait for our own registration as well
        this.talentDependencies.addTalent(name);

        this.skipCycleCheck(true);
        
        this.timeout = config.get('discoverDependenciesTimeout', 60)*1000
    }

    start(timeoutMs=this.timeout) {
        return this.pg.subscribeJson(PLATFORM_EVENTS_TOPIC, this.talentDependencies.__onPlatformEvent.bind(this.talentDependencies))
            .then(() => super.start())
            .then(() => this.talentDependencies.waitForDependencies(timeoutMs))
            .catch(err => {
                this.logger.error(err);
                process.exit(1);
            })
            .then(() => this.triggerTestSuites())
            .catch(err => {
                this.logger.info(`${err}`);
                process.exit(1);
            });
    }

    callees() {
        return this.calleeArray || [];
    }

    getRules() {
        // Not used, but platform requires it?
        return new OrRules([
            new Rule(
                new OpConstraint(`${this.id}.run-tests`, OpConstraint.OPS.ISSET, 0, DEFAULT_TYPE, VALUE_TYPE_RAW)
            ),
        ]);
    }

    async runTestSuite(ev, testSuiteName) {
        var result = true;
        var testSuite = null;

        try {
            this.logger.info(`Get Tests for ${testSuiteName}`)
            testSuite = await this.call(testSuiteName, GET_TEST_INFO_METHOD_NAME, [], ev.subject, ev.returnTopic, 2000);

        } catch (e) {
            this.logger.error(`Could not get TestSuiteInfo from ${testSuiteName} (${e})`);
            return false;
        }

        try {
            this.logger.info(`Prepare ${testSuiteName}`);
            var prepared = await this.call(testSuiteName, PREPARE_TEST_SUITE_METHOD_NAME, [], ev.subject, ev.returnTopic, 2000);

            if (!prepared) {
                this.logger.error(`Could not prepare ${testSuiteName} (Result of prepare was false)`);
                return false;
            }

        } catch (e) {
            this.logger.error(`Could not prepare ${testSuiteName} (${e})`);
            return false;
        }

        this.logger.info(`Running ${testSuiteName}`);

        let numTests = testSuite.tests.length

        for (var i = 0; i < numTests; i++) {
            var test = testSuite.tests[i];
            this.logger.info(`[${i+1}/${numTests}] Running Test: ${test.name}`);
            this.logger.debug(` - Expected: ${test.expectedValue}`)

            try {
                let testResult = await this.call(testSuiteName, 'runTest', [ test.name ], ev.subject, ev.returnTopic, test.timeout);

                if (testResult.actual == TEST_ERROR) {
                    this.logger.info('- Result: NOT OK (TEST_ERROR returned)');
                    result = false;
                    continue;
                }

                this.logger.debug(`- Actual: ${JSON.stringify(testResult.actual)}`);

                // TODO can optimise the use of stringify by doing once
                if (JSON.stringify(test.expectedValue) == JSON.stringify(testResult.actual)) {
                    this.logger.info(`- Result: OK (${testResult.duration}ms)`);
                } else {
                    this.logger.info(` - Result: NOT OK (${JSON.stringify(test.expectedValue)}!=${JSON.stringify(testResult.actual)})`);
                    result = false;
                }
            } catch (e) {
                this.logger.info(`- Result: NOT OK (${e})`);
                result = false;
            }
        }

        return result;
    }

    async triggerTestSuites() {
        this.logger.info('Start Integration Tests');
        let initial_ev = {
            returnTopic: INGESTION_TOPIC,
            subject: "integration_test"
        };

        let result = await this.runTestSuites(initial_ev);
        this.logger.info(`Overall test result is ${result}`);

        if (result === true) {
            process.exit(0);
        } else {
            process.exit(1);
        }
    }

    async runTestSuites(ev) {
        var result = true;

        for (const testSuite of this.testSuiteArray) {
            let testSuiteResult = await this.runTestSuite(ev, testSuite);
            this.logger.info(`Result of ${testSuite} is ${testSuiteResult}`);

            if (!testSuiteResult) {
                result = false;
            }
        }

        return result;
    }

    async onEvent(ev) {
        // Not used
    }
}

class TestSuiteTalent extends FunctionTalent {
    constructor(testSuiteName, protocolAdapter) {
        super(testSuiteName, protocolAdapter)

        this.registerTestAPIFunctions();
        this.testSuiteInfo = new TestSuiteInfo(testSuiteName);
        this.talentDependencies = new TalentDependencies();
    }

    registerTest(testName, expectedValue, testFunction, timeoutMs=2000) {
        let test = new Test(testName, expectedValue, testFunction, timeoutMs);
        this.testSuiteInfo.testMap.set(testName, test);
    }

    start() {
        return this.pg.subscribeJson(PLATFORM_EVENTS_TOPIC, this.talentDependencies.__onPlatformEvent.bind(this.talentDependencies))
            .then(() => super.start());
    }


    registerTestAPIFunctions() {
        this.registerFunction(GET_TEST_INFO_METHOD_NAME, this.getTestSuiteInfo.bind(this));
        this.registerFunction(PREPARE_TEST_SUITE_METHOD_NAME, this.prepare.bind(this));
        this.registerFunction(RUN_TEST_METHOD_NAME, this.runTest.bind(this));
    }

    getTestSuiteInfo(ev, evtctx, timeoutAtMs) {
        return {
            'name' : this.testSuiteInfo.name,
            'tests' : this.testSuiteInfo.getTestList()
        };
    }

    async runTest(testName, ev, evtctx, timeoutAtMs) {
        this.logger.info(`Run Test ${testName}`);

        if (!this.testSuiteInfo.testMap.has(testName)) {
            this.logger.error(`Test ${testName} has not been registered`);
            return new TestResult(testName, TEST_ERROR, -1);
        }

        let start = Date.now();
        let actual = await this.testSuiteInfo.testMap.get(testName).func(ev, evtctx);
        let duration = Date.now() - start;

        return new TestResult(testName, actual, duration);
    }

    /*
     * Override this function to implement
     * checks to ensure that everything is prepared
     * for your test cases
     */
    async prepare(ev, evtctx, timeoutAtMs) {
        let unmetDependencies = this.talentDependencies.checkAll();

        if (unmetDependencies.length > 0) {
            this.logger.error(`Prepare test suite failed because not connected TestSuiteTalent(s): ${unmetDependencies}`);
            return false;
        }

        this.logger.debug('All talent dependencies resolved');
        return true;
    }
}

module.exports = {
    TestSuiteTalent: TestSuiteTalent,
    TestRunnerTalent
}