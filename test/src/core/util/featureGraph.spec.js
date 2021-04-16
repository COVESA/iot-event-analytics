/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const FeatureGraph = require('../../../../src/core/util/featureGraph');
const Logger = require('../../../../src/core/util/logger');

describe('core.util.featureGraph', () => {
    let fg = null;

    beforeEach(() => {
        fg = new FeatureGraph();
        fg.logger.setLogLevel(Logger.__LOG_LEVEL.NONE);
    });

    it('should detect simple dependency cycle', () => {
        fg.addDependency('1', '2');
        fg.addDependency('2', '1');

        expect(fg.containsCycles()).toBeTruthy();
    });

    it('should detect complex dependency cycle', () => {
        fg.addDependency('1', '2');
        fg.addDependency('1', '4');
        fg.addDependency('4', '5');
        fg.addDependency('5', '8');
        fg.addDependency('4', '9');
        fg.addDependency('9', '5');
        fg.addDependency('8', '4');

        expect(fg.containsCycles()).toBeTruthy();
    });

    it('should be able to freeze a specific dependency state', () => {
        fg.addDependency('1', '2');
        fg.addDependency('2', '3');
        fg.freeze();
        fg.addDependency('3', '4');
        fg.addDependency('4', '1');

        expect(fg.containsCycles()).toBeTruthy();
        expect(fg.ids).toEqual(['1', '2', '3', '4']);
        fg.melt();

        expect(fg.containsCycles()).toBeFalsy();
        expect(fg.ids).toEqual(['1', '2', '3']);

        fg.melt();

        expect(fg.ids).toEqual(['1', '2', '3']);
    });

    it('should being able to handle wildcard dependency as from dependency', () => {
        fg.addDependency('a.*', 'b');
        fg.addDependency('b', 'a.b');

        expect(fg.containsCycles()).toBeTruthy();
    });

    it('should being able to handle wildcard dependency as to dependency', () => {
        fg.addDependency('a', 'b.*');
        fg.addDependency('b.test', 'a');

        expect(fg.containsCycles()).toBeTruthy();
    });

    it('should ignore wildcards for types', () => {
        fg.addDependency('a.b', 'b.c');
        fg.addDependency('*.c', 'a.b');

        expect(fg.containsCycles()).toBeFalsy();
    });

    it('should always detect a cycle, if any-wildcard is used', () => {
        fg.addDependency('*.*', 'b');
        fg.addDependency('b', 'a.b');

        expect(fg.containsCycles()).toBeTruthy();
    });
});