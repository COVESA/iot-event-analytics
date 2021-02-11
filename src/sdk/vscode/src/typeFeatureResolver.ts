/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

import * as bent from 'bent';
import * as vscode from 'vscode';

export class TypeFeatureResolver {
    private ttl = -1;
    private typeFeatures: string[] = [];

    async getTypeFeatures() {
        const apiEndpoint = vscode.workspace.getConfiguration('iotea').get('platform.api.endpoint');

        const req = bent('json');

        if (this.typeFeatures.length > 0 && this.ttl > Date.now()) {
            // Has valid data
            return this.typeFeatures;
        }

        this.typeFeatures = [];

        try {
            const types: any = await req(`${apiEndpoint}/types`);

            for (let type in types) {
                if (types[type].features === undefined) {
                    continue;
                }

                for (let feature in types[type].features) {
                    this.typeFeatures.push(`${type}.${feature}`);
                }
            }

            // Fetch again in 10 seconds
            this.ttl = Date.now() + 10000;

            return this.typeFeatures;
        }
        catch(err) {
            return [
                '<type>.<feature>'
            ];
        }
    }
}