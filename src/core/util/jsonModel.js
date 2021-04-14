/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const jsonQuery = require('./jsonQuery');

/**
 * Module JSON Model.
 * 
 * @module jsonModel
 */

/**
 * The JsonModel class provides utility methods to handle json objects. It allows validation, easier access to elements
 * in the json tree and supplies default values.
 */
class JsonModel {
    /**
     * Constructs an instance of the JsonModel class based on json object source and optionally validates the input. The
     * json properties are copied into the instance.
     *
     * @param {*} model - Json object.
     * @param {*} [validator = null] - The model is considered valid unless a validator function is supplied and
     * validator(model) returns false.
     */
    constructor(model, validator = null) {
        if (validator !== null && !validator(model)) {
            throw new Error('Invalid model found');
        }

        Object.assign(this, model);
    }

    /**
     * Gets a newly created submodel of the current JsonModel at a specified path.
     *
     * @param {string} path - Path in the json tree.
     * @param {*} defaultValue - Default value to be used when the path is not found.
     * @returns a newly created JsonModel wrapping the json at the specified path or the default value.
     */
    getSubmodel(path, defaultValue) {
        return new JsonModel(this.get(path, defaultValue));
    }

    /**
     * Gets the json value located at the specified path in the JsonModel.
     *
     * @param {string} path - Path in the json tree.
     * @param {*} defaultValue - Default value to be returned in case the path is not found.
     * @returns json object located at the json path or if the path is not found - the default value.
     */
    get(path, defaultValue) {
        try {
            return jsonQuery.first(this, path).value;
        }
        catch(err) {
            if (defaultValue !== undefined) {
                return defaultValue;
            }

            throw err;
        }
    }
}

module.exports = JsonModel;
