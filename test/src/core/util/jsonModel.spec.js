/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const JsonModel = require('../../../../src/core/util/jsonModel');

describe('core.util.jsonModel', () => {
    let json = null;
    let model = null;

    beforeEach(() => {
        json = require('../../../resources/jsonModel.input.json');
        model = new JsonModel(json);
    });

    it('should return a given value', () => {
        expect(model.get('bar.baz')).toBe('Hello World');
    });

    it('should return the default value in case the path does not exist', () => {
        expect(model.get('bar.bar', 'Hello')).toBe('Hello');
    });

    it('should throw an error on a given false path without a default value', () => {
        expect(() => model.get('bar.bar')).toThrow();
    });

    it('should validate a model, with a given validator function', () => {
        expect(() => {
            new JsonModel(json, () => {
                return false;
            });
        }).toThrow();
    });

    it('should return a submodel at a given path', () => {
        const submodel = model.getSubmodel('bar');

        expect(submodel instanceof JsonModel).toBeTruthy();
        expect(submodel.get("'bla.blubb'")).toBe(555);
    });
});