const Logger = require('../../src/core/util/logger');

module.exports = class NullLogger extends Logger {
    constructor(name) {
        super(name)
    }

    __log() {}
};