const Logger = require('../../src/core/util/logger');
Logger.__LOG_LEVEL.ALWAYS = 0;
module.exports = class NullLogger extends Logger {
    constructor(name) {
        super(name);
    }

    __log() {}
};