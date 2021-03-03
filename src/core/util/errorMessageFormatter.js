const { ValidationError } = require('ajv')

class ErrorMessageFormatter {}

ErrorMessageFormatter.formatAjvValidationError = function formatAjvValidationError(validationError) {
    if (validationError instanceof ValidationError) {
        return validationError.errors.map(err => `${err.dataPath} ${err.message}`).join(', ')
    }

    return validationError.message;
};

module.exports = ErrorMessageFormatter;