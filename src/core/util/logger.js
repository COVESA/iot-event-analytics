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

 /**
  * Logger module.
  * 
  * @module logger 
  */
 
 /**
  * The utility Logger class provides methods to log messages under different severity levels. If the log level is set to
  * WARNING or ERROR, the log messages are sent to stderr, otherwise - to stdout.
  */
class Logger {
     /**
      * Creates an instance of the Logger class with default log level WARNING.
      *
      * @param {string} [name = 'default'] - Descriptive name of the Logger.
      */
    constructor(name = 'default') {
        this.name = name;
        this.logFunction = null;
        this.reset();
    }

     /**
      * Logs a message with VERBOSE log level. If the message will be actually logged depends on the process environment
      * variable LOG_LEVEL.
      *
      * @param {string} message - Message to be logged.
      * @param {Object} ctx - Defines the context in which the message is created. See {@link module:logger~Context},
      * {@link module:logger~Logger.createEventContext} and {@link module:logger~Logger.createContext}. The evaluated
      * data from the context is included in the log message.
      */
    verbose(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.VERBOSE, ctx, null, now);
    }

     /**
      * Logs a message with DEBUG log level. If the message will be actually logged depends on the process environment
      * variable LOG_LEVEL.
      *
      * @param {string} message - Message to be logged.
      * @param {Object} ctx - Defines the context in which the message is created. See {@link module:logger~Context},
      * {@link module:logger~Logger.createEventContext} and {@link module:logger~Logger.createContext}. The evaluated
      * data from the context is included in the log message.
      */
    debug(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.DEBUG, ctx, null, now);
    }

     /**
      * Logs a message with INFO log level. If the message will be actually logged depends on the process environment
      * variable LOG_LEVEL.
      *
      * @param {string} message - Message to be logged.
      * @param {Object} ctx - Defines the context in which the message is created. See {@link module:logger~Context},
      * {@link module:logger~Logger.createEventContext} and {@link module:logger~ Logger.createContext}. The evaluated
      * data from the context is included in the log message.
      */
    info(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.INFO, ctx, null, now);
    }

     /**
      * Logs a message with WARNING log level. If the message will be actually logged depends on the process environment
      * variable LOG_LEVEL.
      *
      * @param {string} message - Message to be logged.
      * @param {Object} ctx - Defines the context in which the message is created. See {@link module:logger~Context},
      * {@link module:logger~Logger.createEventContext} and {@link module:logger~Logger.createContext}. The evaluated
      * data from the context is included in the log message.
      */
    warn(message, ctx, err, now) {
        this.__log(message, Logger.__LOG_LEVEL.WARN, ctx, err, now);
    }

     /**
      * Logs a message with ERROR log level. If the message will be actually logged depends on the process environment
      * variable LOG_LEVEL.
      *
      * @param {string} message - Message to be logged.
      * @param {Object} ctx - Defines the context in which the message is created. See {@link module:logger~Context},
      * {@link module:logger~Logger.createEventContext} and {@link module:logger~Logger.createContext}. The evaluated
      * data from the context is included in the log message.
      */
    error(message, ctx, err, now) {
        this.__log(message, Logger.__LOG_LEVEL.ERROR, ctx, err, now);
    }
    
     /**
      * Logs a message with ALWAYS log level. If the message will be actually logged depends on the process environment
      * variable LOG_LEVEL.
      *
      * @param {string} message - Message to be logged.
      * @param {Object} ctx - Defines the context in which the message is created. See {@link module:logger~Context},
      * {@link module:logger~Logger.createEventContext} and {@link module:logger~Logger.createContext}. The evaluated
      * data from the context is included in the log message.
      */
    always(message, ctx, now) {
        this.__log(message, Logger.__LOG_LEVEL.ALWAYS, ctx, null, now);
    }

     /**
      * Resets the log level, date format and message format to their default values.
      */
    reset() {
        this.logFunction = null;
        this.setLogLevel(Logger.__LOG_LEVEL.WARN);
        // Set ISO 8601 as default
        this.setDateFormat('[Y0001]-[M01]-[D01]T[H#01]:[m01]:[s01].[f001]Z');
        this.setMessageFormat('$date & " " & $pad($level, -7, " ") & " [" & $name & "] " & ": " & $message & $context');
    }

     /**
      * Sets the date format for the log messages.
      *
      * @param {string} picture - Format according to jsonata $now definition.
      */
    setDateFormat(picture) {
        // Force UTC
        //this.dateFormat = jsonata(`$now('${picture}', '+0000')`);
        this.dateFormat = jsonata(`$fromMillis($now, '${picture}', '+0000')`);
    }

     /**
      * Sets the format of the log messages.
      * 
      * @param {string} jna - Jsonata expression string.
      */
    setMessageFormat(jna) {
        this.messageFormat = jsonata(jna);
    }

     /**
      * Sets the log level of this Logger instance.
      * 
      * @param {number} numericLevel - A number between 1 (VERBOSE) and 7 (ALWAYS).
      */
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

    __log(message, numericLevel, ctx = '', err = null, now = Date.now(), func = this.logFunction) {
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
            message: this.__replaceNewlines(this.__serializeMessage(message, '<empty message>', '<unserializable message>')),
            name: this.__replaceNewlines(this.__serializeMessage(this.name, '<empty name>', '<unserializable name>')),
            date: this.dateFormat.evaluate('', { now }),
            level: this.__formatLogLevel(numericLevel),
            context: this.__replaceNewlines(this.__serializeMessage(ctx, '<empty context>', '<unserializable context>', ' '))
        })));
    }

    __serializeMessage(input, emptyInputPlaceholder = '<empty>', nonStringPlaceholder = '<unserializable>', prefix = '') {
        if (input === null || input === undefined) {
            return `${prefix}${emptyInputPlaceholder}`;
        }

        if (typeof input.toString === 'function') {
            input = input.toString();
        }

        if (typeof input !== 'string') {
            return `${prefix}${nonStringPlaceholder}`;
        }

        if (input === '') {
            // If it's an empty string, do not add the prefix
            return '';
        }

        return `${prefix}${input}`;
    }

    __replaceNewlines(input) {
        return input.replace(/(?:\r\n|\r|\n)/g, ' ');
    }
}

 /**
  * Creates and returns a Context object based on some json data and jsonata
  * query.
  *
  * @param {*} input - JSON object.
  * @param {string} jna - jsonata query string.
  * @returns a newly created {@link module:logger~Context} object.
  */
Logger.createContext = function createContext(input, jna) {
    return new Context(input, jna);
};

 /**
  * Creates and returns a Context object based on an event.
  *
  * @param {*} ev - JSON event.
  * @returns a newly created {@link module:logger~Context} object.
  */
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

 /**
  * An object containing the string values of the log levels: VERBOSE, DEBUG, INFO, WARN, ERROR, NONE, ALWAYS. Can be
  * accessed as Logger.ENV_LOG_LEVEL.INFO for example.
  */
Logger.ENV_LOG_LEVEL = Object.keys(Logger.__LOG_LEVEL).reduce((acc, level) => {
    acc[level] = level;
    return acc;
}, {});

 /**
  * Creates a UUID4-styled random value, if correlation id cid is not available within the event.
  * Pattern: ########-####-####-####-############.
  */
  Logger.EVENT_CTX_JNA = 'cid ? cid : ( $c := "abcdef0123456789"; [[1..8], [1..4], [1..4], [1..4], [1..12]] ~> $map( function($v) { $v ~> $map( function() { $substring($c, $floor($random() * $length($c)), 1) } ) ~> $join() }) ~> $join("-"))';

 /**
  * This class represents a context in which logging is done.
  */
  class Context {
    /**
      * Creates a Context instance.
      * 
      * @param {*} input - A json object.
      * @param {string} jna - A jsonata query string.
      */      
    constructor(input, jna) {
        this.jna = null;
        this.data = null;
        this.refresh(input, jna);
    }

    /**
     * Clears the evaluated jsonata data.
     */
    clear() {
        this.data = null;
    }

     /**
      * Applies the jsonata query string on the input json object. Loads the
      * result into data field.
      *
      * @param {*} input - A json object.
      * @param {string} jna - A jsonata query string.
      * @returns {@void}
      */    
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

     /**
      * Get a string representation of this Context object.
      * 
      * @returns {string}
      */
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