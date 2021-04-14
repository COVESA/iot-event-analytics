const { ValidationError } = require('ajv')
/**
 * Module Error Message Formatter.
 * 
 * @module errorMessageFormatter
 */

/**
 * ErrorMessageFormatter class.
 */
class ErrorMessageFormatter {}

/**
 * Formats an AJV ValidationError object into a string ready to be put in a log message.
 *
 * @param {object} validationError AJV ValidationError object.
 * @returns a formatted string message if the passed argument is an AJV ValidationError object, validationError.message otherwise.
 */
ErrorMessageFormatter.formatAjvValidationError = function formatAjvValidationError(validationError) {
    if (validationError instanceof ValidationError) {
        return validationError.errors.map(err => `${err.dataPath} ${err.message}`).join(', ')
    }

    return validationError.message;
};

module.exports = ErrorMessageFormatter;