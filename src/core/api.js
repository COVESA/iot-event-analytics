/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('./util/logger');

module.exports = class Api {
    constructor() {
        this.logger = new Logger('Api');
    }
    __transform(input, transformer) {
        return new Promise((resolve, reject) => {
            transformer.evaluate(input, null, (err, output) => {
                if (err) {
                    this.logger.error(`Transformation failed`, null, err);
                    reject(new Error('Transformation failed'));
                    return;
                }

                resolve(output);
            });
        });
    }
};