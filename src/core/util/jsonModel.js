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

class JsonModel {
    constructor(model, validator = null) {
        if (validator !== null && !validator(model)) {
            throw new Error('Invalid model found');
        }

        Object.assign(this, model);
    }

    getSubmodel(path, defaultValue) {
        return new JsonModel(this.get(path, defaultValue));
    }

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