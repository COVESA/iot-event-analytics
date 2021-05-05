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

export class VssPathTranslator {
    constructor(private vssPathSeparator: string, private vssPathReplacer: { [s: string]: string } ) {}

    public ioteaTypeAndFeature2KuksaVssPath(type: string, feature: string) {
        const reversePathSeparator = this.vssPathReplacer[this.vssPathSeparator] === undefined ? this.vssPathSeparator : this.vssPathReplacer[this.vssPathSeparator];

        let kuksaVssPath = `${type}${reversePathSeparator}${feature}`;

        for (let replacement in this.vssPathReplacer) {
            kuksaVssPath = this.replaceAllChars(kuksaVssPath, this.vssPathReplacer[replacement], replacement);
        }

        return kuksaVssPath;
    }

    private replaceAllChars(input: string, searchChar: string, replaceChar: string) {
        const reservedRegexpChars = ['[', ']', '(', ')', '\\', '^', '$', '.', '|', '?', '*', '+', '{', '}' ];

        if (reservedRegexpChars.indexOf(searchChar) === -1) {
            return input.replace(new RegExp(`${searchChar}`, 'g'), replaceChar);
        }

        return input.replace(new RegExp(`\\${searchChar}`, 'g'), replaceChar);
    }
}
export class TypeFeatureResolver {
    private ttl = -1;
    private typeFeatures: any[] = [];

    async getTypeFeatures() {
        const apiEndpoint = vscode.workspace.getConfiguration('iotea').get('platform.api.endpoint');

        const req = bent('json');

        if (this.typeFeatures.length > 0 && this.ttl > Date.now()) {
            // Has valid data
            return this.typeFeatures;
        }

        this.typeFeatures = [];

        const vssPathTranslator = new VssPathTranslator(
            vscode.workspace.getConfiguration('iotea').get('vss.path.separator') as string,
            vscode.workspace.getConfiguration('iotea').get('vss.path.replacer') as { [s: string]: string }
        );

        try {
            const types: any = await req(`${apiEndpoint}/types`);

            for (let type in types) {
                if (types[type].features === undefined) {
                    continue;
                }

                for (let feature in types[type].features) {
                    this.typeFeatures.push({
                        ioteaType: type,
                        description: types[type].features[feature].description,
                        ioteaFeature: feature,
                        vssPath: vssPathTranslator.ioteaTypeAndFeature2KuksaVssPath(type, feature)
                    });
                }
            }

            const refreshIntervalMs = vscode.workspace.getConfiguration('iotea').get('autocomplete.typeFeatures.refresh-interval-ms') as number;
            this.ttl = Date.now() + refreshIntervalMs;

            return this.typeFeatures;
        }
        catch(err) {
            return [
                '<type>.<feature>'
            ];
        }
    }
}