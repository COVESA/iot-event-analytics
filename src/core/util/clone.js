/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

module.exports = function clone(value, createKey = (key => key)) {
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
            object[createKey(inKey)] = clone(value[inKey], createKey);
        }

        return object;
    }

    return value;
};
