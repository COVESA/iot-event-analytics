/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const jsonata = require('jsonata');

class Logger {
    constructor(name = 'default') {
        this.name = name;
        this.logFunction = null;
        this.reset();
    }

    verbose(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.VERBOSE, ctx, null, now);
    }

    debug(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.DEBUG, ctx, null, now);
    }

    info(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.INFO, ctx, null, now);
    }

    warn(message, ctx, err, now) {
        this.__log(message, Logger.__LOG_LEVEL.WARN, ctx, err, now);
    }

    error(message, ctx, err, now) {
        this.__log(message, Logger.__LOG_LEVEL.ERROR, ctx, err, now);
    }

    always(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.ALWAYS, ctx, null, now);
    }

    reset() {
        this.logFunction = null;
        this.setLogLevel(Logger.__LOG_LEVEL.WARN);
        // Set ISO 8601 as default
        this.setDateFormat('[Y0001]-[M01]-[D01]T[H#01]:[m01]:[s01].[f001]Z');
        this.setMessageFormat('$date & " " & $pad($level, -7, " ") & " [" & $name & "] " & ": " & $message & $context');
    }

    setDateFormat(picture) {
        // Force UTC
        //this.dateFormat = jsonata(`$now('${picture}', '+0000')`);
        this.dateFormat = jsonata(`$fromMillis($now, '${picture}', '+0000')`);
    }

    setMessageFormat(jna) {
        this.messageFormat = jsonata(jna);
    }

    setLogLevel(numericLevel) {
        if (Object.values(Logger.__LOG_LEVEL).indexOf(numericLevel) === -1) {
            throw new Error(`Given numeric log level ${numericLevel} is invalid`);
        }

        this.numericLevel = numericLevel;
    }

    setLogFunction(logFunction) {
        this.logFunction = logFunction;
    }

    __formatLogLevel(numericLevel) {
        return Object.keys(Logger.__LOG_LEVEL)[Object.values(Logger.__LOG_LEVEL).indexOf(numericLevel)];
    }

    __log(message, numericLevel, ctx = null, err = null, now = Date.now(), func = this.logFunction) {
        const currentLevel = Logger.__LOG_LEVEL[process.env.LOG_LEVEL] || this.numericLevel;

        if (numericLevel < currentLevel) {
            return;
        }

        if (func === null) {
            func = console.log;

            if (numericLevel === Logger.__LOG_LEVEL.WARN || numericLevel === Logger.__LOG_LEVEL.ERROR) {
                func = console.error;
            }
        }

        if (err instanceof Error && err.stack) {
            message += ` [${this.__replaceNewlines(err.stack.replace(/^\s*at\s*(.+)$/gm, '@ $1'))}]`;
        }

        func(this.messageFormat.evaluate('', Object.assign({
            message: this.__replaceNewlines(message),
            name: this.__replaceNewlines(this.name),
            date: this.dateFormat.evaluate('', { now }),
            level: this.__formatLogLevel(numericLevel),
            context: ctx ? this.__replaceNewlines(' ' + ctx.toString()) : ''
        })));
    }

    __replaceNewlines(input) {
        return input.replace(/(?:\r\n|\r|\n)/g, ' ');
    }
}

Logger.createContext = function createContext(input, jna) {
    return new Context(input, jna);
};

Logger.createEventContext = function (ev) {
    return new Context(ev, Logger.EVENT_CTX_JNA);
};

Logger.__LOG_LEVEL = {
    VERBOSE: 1,
    DEBUG: 2,
    INFO: 3,
    WARN: 4,
    ERROR: 5,
    NONE: 6,
    ALWAYS: 7
};

Logger.ENV_LOG_LEVEL = Object.keys(Logger.__LOG_LEVEL).reduce((acc, level) => {
    acc[level] = level;
    return acc;
}, {});

// Creates a UUID4-styled random value, if correlation id cid is not available within the event
// ########-####-####-####-############
Logger.EVENT_CTX_JNA = 'cid ? cid : ( $c := "abcdef0123456789"; [[1..8], [1..4], [1..4], [1..4], [1..12]] ~> $map( function($v) { $v ~> $map( function() { $substring($c, $floor($random() * $length($c)), 1) } ) ~> $join() }) ~> $join("-"))';

class Context {
    constructor(input, jna) {
        this.jna = null;
        this.data = null;
        this.refresh(input, jna);
    }

    clear() {
        this.data = null;
    }

    refresh(input, jna = null) {
        if (jna !== null) {
            this.jna = jsonata(jna);
        }

        if (input === undefined) {
            this.data = null;
            return;
        }

        if (this.jna !== null) {
            // Transform input according to given jna
            try {
                this.data = this.jna.evaluate(input);
            }
            catch(err) {
                this.data = null;
            }
        } else {
            this.data = input;
        }
    }

    toString() {
        if (this.data === null) {
            return '';
        }

        if (typeof this.data === 'string') {
            return this.data;
        }

        return JSON.stringify(this.data, null).replace(/,/g, ', ');
    }
}

module.exports = Logger;