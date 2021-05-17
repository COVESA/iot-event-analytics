/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('../../../../src/core/util/logger');

describe('core.util.jsonModel', () => {
    let logger = null;
    let logFunctionSpy = null;

    beforeEach(() => {
        logger = new Logger('test-logger');

        // Hide output in test results
        logFunctionSpy = jasmine.createSpy('logFunction');

        logger.setLogFunction(logFunctionSpy);

        spyOn(logger, '__log').and.callThrough();
    });

    it('should reject setting a wrong log level', () => {
        expect(() => logger.setLogLevel(9999999)).toThrow();
    });

    it('should respect the given log levels', () => {
        const log2FuncMapping = {};

        log2FuncMapping[Logger.__LOG_LEVEL.VERBOSE] = logger.verbose.bind(logger);
        log2FuncMapping[Logger.__LOG_LEVEL.DEBUG] = logger.debug.bind(logger);
        log2FuncMapping[Logger.__LOG_LEVEL.INFO] = logger.info.bind(logger);
        log2FuncMapping[Logger.__LOG_LEVEL.WARN] = logger.warn.bind(logger);
        log2FuncMapping[Logger.__LOG_LEVEL.ERROR] = logger.error.bind(logger);
        log2FuncMapping[Logger.__LOG_LEVEL.NONE] = null;
        log2FuncMapping[Logger.__LOG_LEVEL.ALWAYS] = logger.always.bind(logger);

        const numericLoglevels = Object.keys(log2FuncMapping).map(s => parseInt(s, 10));

        for (let i = 0; i < numericLoglevels.length; i++) {
            const numericLoglevel = numericLoglevels[i];

            if (log2FuncMapping[numericLoglevel] === null) {
                continue;
            }

            logger.setLogLevel(numericLoglevel);

            // Call logging function
            log2FuncMapping[numericLoglevel]('I should be logged');

            expect(logger.__log.calls.mostRecent().args[1]).toBe(numericLoglevel);

            logFunctionSpy.calls.reset();

            if (i < numericLoglevels.length - 1) {
                // Set to higher loglevel
                logger.setLogLevel(numericLoglevels[i + 1]);
                // Call logging function
                log2FuncMapping[numericLoglevel]('I should not be logged');

                expect(logFunctionSpy).not.toHaveBeenCalled();
            }
        }
    });

    it('should have the correct message format', () => {
        const ctx = Logger.createContext('HELLO');

        logger.setLogLevel(Logger.__LOG_LEVEL.INFO);
        const now = 1617712424808;
        logger.info('An info', ctx, now);

        expect(logFunctionSpy.calls.mostRecent().args[0]).toBe('2021-04-06T12:33:44.808Z    INFO [test-logger] : An info HELLO');
    });

    it('should take a given correlation id', () => {
        logger.setLogLevel(Logger.__LOG_LEVEL.INFO);

        const now = 1617712424808;

        let evtctx = Logger.createEventContext({
            cid: '338e8dc0-16ca-398b-194c-950ab6d15bd3'
        });

        logger.info('An info', evtctx, now);

        expect(logFunctionSpy.calls.mostRecent().args[0]).toBe('2021-04-06T12:33:44.808Z    INFO [test-logger] : An info 338e8dc0-16ca-398b-194c-950ab6d15bd3');
    });

    it('should generate a valid correlation id if not given', () => {
        const evtctx = Logger.createEventContext({});

        // Check UUIDv4 format
        expect(evtctx.data).toMatch(/^[a-z0-9]{8}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12}$/);
    });

    it('should render an error to console.error', () => {
        spyOn(console, 'error').and.callFake(() => {});

        logger.setLogFunction(null);
        const err = new Error('An error');

        logger.error(err.message, null, err);

        expect(console.error.calls.mostRecent().args[0]).toMatch(/An error \[Error: .+\] <empty context>$/g);
    });

    it('should render an log message console.log', () => {
        logger.setLogLevel(Logger.__LOG_LEVEL.INFO);

        spyOn(console, 'log').and.callFake(() => {});

        logger.setLogFunction(null);

        const nowMs = 1618576287255;
        logger.info('Hello world', '', nowMs);

        expect(console.log).toHaveBeenCalledTimes(1);
        expect(console.log.calls.mostRecent().args[0]).toBe('2021-04-16T12:31:27.255Z    INFO [test-logger] : Hello world');
    });

    it('should not crash, if invalid or missing input parameters are given', () => {
        logger.setLogLevel(Logger.__LOG_LEVEL.INFO);

        const now = 1617712424808;

        logger.info('Hello world', '', now);

        expect(logFunctionSpy.calls.mostRecent().args[0]).toBe('2021-04-06T12:33:44.808Z    INFO [test-logger] : Hello world');

        logger.info(undefined, null, now);

        expect(logFunctionSpy.calls.mostRecent().args[0]).toBe('2021-04-06T12:33:44.808Z    INFO [test-logger] : <empty message> <empty context>');

        logger.warn('A warning', 123, null, now);

        expect(logFunctionSpy.calls.mostRecent().args[0]).toBe('2021-04-06T12:33:44.808Z    WARN [test-logger] : A warning 123');

    });
});