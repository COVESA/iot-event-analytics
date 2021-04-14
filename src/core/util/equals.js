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
 * Module Equals.
 * 
 * @module equals 
 */

/**
 * Checks if two objects are equal. Deep-check is performed with the object properties. The _options_ parameter contains
 * instructions whether to check the array elements in a strict order. This means that if strictArrayOrder = false,
 * [1, 2, 3] will be equal to [2, 3, 1].
 *
 * @param {*} a - Object to compare.
 * @param {*} b - Object to compare.
 * @param {*} [options = { strictArrayOrder: true }] - Contains instructions if arrays with disordered elements should be
 * considered equal.
 * @returns true if the objects are equal based on the passed options.
 */
 module.exports = function equals(a, b, options = { strictArrayOrder: true }) {
    const aIsArray = Array.isArray(a);
    const bIsArray = Array.isArray(b);

    if ((aIsArray && !bIsArray) || (!aIsArray && bIsArray)) {
        return false;
    }

    if (aIsArray && bIsArray) {
        if (a.length !== b.length) {
            return false;
        }

        if (!options.strictArrayOrder) {
            const abck = [...a];
            const bbck = [...b];

            for (let i = 0; i < a.length; i++) {
                let found = false;

                for (let j = 0; j < b.length; j++) {
                    if (equals(abck[i], bbck[j], options)) {
                        bbck.splice(j, 1);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    return false;
                }
            }
        } else {
            for (let i = 0; i < a.length; i++) {
                if (!equals(a[i], b[i], options)) {
                    return false;
                }
            }
        }

        return true;
    }

    if (typeof a === 'object' && typeof b === 'object') {
        if (a === null || b === null) {
            // One of them is null >> only true if both are null
            return a === b;
        }

        // None of them is null
        const akeys = Object.keys(a);
        const bkeys = Object.keys(b);

        if (akeys.length !== bkeys.length) {
            return false;
        }

        for (let i = 0; i < akeys.length; i++) {
            if (Object.prototype.hasOwnProperty.call(a, akeys[i]) && Object.prototype.hasOwnProperty.call(b, akeys[i])) {
                // Both properties are really present and not undefined
                if (!equals(a[akeys[i]], b[akeys[i]], options)) {
                    return false;
                }

                continue;
            }

            return false;
        }

        return true;
    }

    return a === b;
};

