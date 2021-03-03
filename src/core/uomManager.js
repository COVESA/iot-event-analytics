/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const fs = require('fs');

class UomManager {
    constructor() {
        this.uom = {};
    }

    start(uomConfigPath) {
        return this.__loadConfiguration(uomConfigPath)
            .then(uomConfig => {
                this.uom = uomConfig;

                for (const key in this.uom) {
                    this.processBaseReference(this.uom[key]);
                }
            });
    }

    resolve(key) {
        if (!this.__hasUom(key)) {
            throw new Error(`Unit of measurement with key ${key} could not be resolved`);
        }

        return this.uom[key];
    }

    processBaseReference(uom) {
        if (uom.ref === undefined) {
            return uom;
        }

        if (!this.__hasUom(uom.ref)) {
            throw new Error(`Base Unit of measurement ${uom.ref} could not be resolved`);
        }

        uom.base = this.uom[uom.ref];

        // Delete the reference key, since it has been resolved
        delete uom.ref;

        return uom;
    }

    __hasUom(key) {
        return this.uom[key] !== undefined;
    }

    __loadConfiguration(uomConfigPath) {
        return new Promise((resolve, reject) => {
            fs.readFile(uomConfigPath, {
                encoding: 'utf8'
            }, (err, content) => {
                if (err) {
                    reject(err);
                    return;
                }

                resolve(JSON.parse(content));
            });
        });
    }
}

module.exports = UomManager;