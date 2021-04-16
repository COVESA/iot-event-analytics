/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('../../src/core/util/logger');
Logger.__LOG_LEVEL.ALWAYS = 0;
module.exports = class NullLogger extends Logger {
    constructor(name) {
        super(name);
    }

    __log() {}
};