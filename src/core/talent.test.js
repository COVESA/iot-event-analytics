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
    PREPARE_TEST_SET_METHOD_NAME,
    RUN_TEST_METHOD_NAME,
    TEST_ERROR,
    PLATFORM_EVENTS_TOPIC,
    PLATFORM_EVENT_TYPE_SET_RULES,
    PLATFORM_EVENT_TYPE_UNSET_RULES
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
    }
    addTalent(talentId) {
        this.talentDependencies.set(talentId, false);
    }

    removeTalent(talentId) {
        this.talentDependencies.delete(talentId);

    }

    async __onPlatformEvent(ev) {
        if (ev.type == PLATFORM_EVENT_TYPE_SET_RULES) {
            if (this.talentDependencies.has(ev.data.talent)) {
                this.talentDependencies.set(ev.data.talent, true);
            }

        } else if (ev.type == PLATFORM_EVENT_TYPE_UNSET_RULES) {
            if (this.talentDependencies.has(ev.data.talent)) {
                this.talentDependencies.set(ev.data.talent, false);
            }
        }
    }

    check(talentId) {
        return this.talendDependencies.get(talentId);
    }

    checkAll() {
        let notConnectedArray = new Array();
        let result = true;
        for (const [key, value] of this.talentDependencies.entries()) {
            if (value == false) {
                result = false;
                notConnectedArray.push(key);
            }
        }

        return { 'result': result , 'notConnected': notConnectedArray };
    }
}

class TestSetInfo {
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
    constructor(name, testSetList, connectionString) {
        super(name, connectionString);

        // Define run-tests feature for this test runner automatically at runtime
        this.addOutput('run-tests', {
            description: 'Event to start the integration tests',
            encoding: {
                type: ENCODING_TYPE_BOOLEAN,
                encoder: null
            },
        });

        this.calleeArray = new Array();
        this.testSetArray = new Array();
        this.talentDependencies = new TalentDependencies();

        testSetList.forEach(testSet => {
            let testSetTalentId = testSet;
            this.testSetArray.push(testSetTalentId);

            // Set Test Callees (Test API)
            this.calleeArray.push(`${testSetTalentId}.${GET_TEST_INFO_METHOD_NAME}`);
            this.calleeArray.push(`${testSetTalentId}.${PREPARE_TEST_SET_METHOD_NAME}`);
            this.calleeArray.push(`${testSetTalentId}.${RUN_TEST_METHOD_NAME}`);

            // Add talent id to dependency
            this.talentDependencies.addTalent(testSetTalentId);
            }
        );

        this.skipCycleCheck(true);
    }

    start() {
        this.broker.subscribeJson(PLATFORM_EVENTS_TOPIC, this.talentDependencies.__onPlatformEvent.bind(this.talentDependencies));
        super.start();
    }

    callees() {
        return this.calleeArray || [];
    }

    getRules() {
        return new OrRules([
            new Rule(
                new OpConstraint(`${this.id}.run-tests`, OpConstraint.OPS.ISSET, 0, DEFAULT_TYPE, VALUE_TYPE_RAW)
            ),
        ]);
    }

    async runTestSet(ev, testSetName) {
        var result = true;
        var testSet = null;

        try {
            this.logger.info(`Get Tests for ${testSetName}`)
            testSet = await this.call(testSetName, GET_TEST_INFO_METHOD_NAME, [], ev.subject, ev.returnTopic, 2000);

        } catch (e) {
            this.logger.error(`Could not get TestSetInfo from ${testSetName} (${e})`);
            return false;
        }


        try {
            this.logger.info(`Prepare ${testSetName}`);
            var prepared = await this.call(testSetName, PREPARE_TEST_SET_METHOD_NAME, [], ev.subject, ev.returnTopic, 2000);

            if (!prepared) {
                throw 'Result of prepare was false';
            }

        } catch (e) {
            this.logger.error(`Could not prepare ${testSetName} (${e})`);
            return false;
        }

        this.logger.info(`Running ${testSetName}`);

        let numTests = testSet.tests.length

        for (var i = 0; i < numTests; i++) {
            var test = testSet.tests[i];
            this.logger.info(`[${i+1}/${numTests}] Running Test: ${test.name}`);
            this.logger.debug(` - Expected: ${test.expectedValue}`)

            try {
                let testResult = await this.call(testSetName, 'runTest', [ test.name ], ev.subject, ev.returnTopic, test.timeout);

                if (testResult.actual == TEST_ERROR) {
                    throw 'TEST_ERROR returned'
                }

                this.logger.debug(`- Actual: ${JSON.stringify(testResult.actual)}`);
                
                // TODO can optimise the use of stringify by doing once
                if (JSON.stringify(test.expectedValue) == JSON.stringify(testResult.actual)) {
                    this.logger.info(`- Result: OK (${testResult.duration}ms)`);
                    continue;
                } else {
                    this.logger.info(` - Result: NOT OK (${JSON.stringify(test.expectedValue)}!=${JSON.stringify(testResult.actual)})`);
                    result = false;
                    continue;
                }
            } catch (e) {
                this.logger.info(`- Result: NOT OK (${e})`);
                result = false;
                continue;
            }
        }

        return result;
    }

    async runTestSets(ev) {
        var result = true;

        for (const testSet of this.testSetArray) {
            let testSetResult = await this.runTestSet(ev, testSet);
            this.logger.info(`Result of ${testSet} is ${testSetResult}`);

            if (!testSetResult) {
                result = false;
            }
        }

        return result;
    }

    async onEvent(ev, evtctx) {
        let dependenciesState = this.talentDependencies.checkAll();

        if (dependenciesState.result == true) {
            this.logger.info('Start Integration Tests');
            let result = await this.runTestSets(ev);
            this.logger.info(`Overall test result is ${result}`);
        } else {
            this.logger.error(`Can't start tests because of not connected TestSetTalent(s): ${dependenciesState.notConnected}`);
        }
    }
}

class TestSetTalent extends FunctionTalent {
    constructor(testSetName, connectionString) {
        super(testSetName, connectionString)

        this.registerTestAPIFunctions();
        this.testSetInfo = new TestSetInfo(testSetName);
        this.talentDependencies = new TalentDependencies();
    }

    registerTest(testName, expectedValue, testFunction, timeout=2000) {
        let test = new Test(testName, expectedValue, testFunction, timeout);
        this.testSetInfo.testMap.set(testName, test);
    }

    start() {
        this.broker.subscribeJson(PLATFORM_EVENTS_TOPIC, this.talentDependencies.__onPlatformEvent.bind(this.talentDependencies));
        super.start();
    }


    registerTestAPIFunctions(self) {
        this.registerFunction(GET_TEST_INFO_METHOD_NAME, this.getTestSetInfo.bind(this));
        this.registerFunction(PREPARE_TEST_SET_METHOD_NAME, this.prepare.bind(this));
        this.registerFunction(RUN_TEST_METHOD_NAME, this.runTest.bind(this));
    }

    getTestSetInfo(ev, evtctx) {
        return {
            "name" : this.testSetInfo.name,
            "tests" : this.testSetInfo.getTestList()
        }
    }

    async runTest(testName, ev, evtctx) {
        this.logger.info(`Run Test ${testName}`);
        if (this.testSetInfo.testMap.has(testName)) {

            let start = Date.now();
            let actual = await this.testSetInfo.testMap.get(testName).func(ev, evtctx);
            let duration = Date.now() - start;

            return new TestResult(testName, actual, duration);

        } else {
            this.logger.error(`Test ${testName} has not been registered`);
            return new TestResult(testName, TEST_ERROR, -1);
        }
    }

    /*
     * Override this function to implement
     * checks to ensure that everything is prepared
     * for your test cases
     */
    async prepare(ev, evtctx) {
        let dependenciesState = this.talentDependencies.checkAll();

        if (dependenciesState.result == true) {
            this.logger.debug('All talent dependencies resolved');
            return true;

        } else {
            this.logger.error(`Prepare test set failed because not connected TestSetTalent(s): ${dependenciesState.notConnected}`);
            return false;
        }
    }
}

module.exports = {
    TestSetTalent,
    TestRunnerTalent
}