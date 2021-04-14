/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

/**
 * Clone Module.
 * 
 * @module clone 
 */


/**
 * Performs a deep-clone of the passed object. Clones also the property values of the object including the inherited
 * ones.
 *
 * @param {object} value - Object to be cloned.
 * @param {*} [createKey = (key => key)] - A function defining how to treat the object keys (properties) during the
 * clone.
 * @param {*} [exclude = (key => false)] - A function defining if an object key (property) should be excluded from the
 * cloning.
 * @returns a clone of the _value_ object.
 */
module.exports = function clone(value, createKey = (key => key), exclude = (key => false)) {
    if (Array.isArray(value)) {
        const arr = [];

        for (var i = 0; i < value.length; i++) {
            arr.push(clone(value[i], createKey));
        }

        return arr;
    }

    if (value === Object(value)) {
        const object = {};

        for (let inKey in value) {
            // Enumerate through inherited properties as well
            // i.e. do not use Object.keys(value) here
            if (exclude(inKey)) {
                continue;
            }

            object[createKey(inKey)] = clone(value[inKey], createKey);
        }

        return object;
    }

    return value;
};
