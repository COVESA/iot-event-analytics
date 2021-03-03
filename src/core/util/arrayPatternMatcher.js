/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Logger = require('./logger');

const JsonModel = require('./jsonModel');
class Wildcard {
    constructor () {
        this.nmin = 0;
        this.nmax = Number.MAX_SAFE_INTEGER;
        this.rejects = [];
        this.accepts = null;
    }

    minValues(nmin) {
        this.nmin = nmin;
        return this;
    }

    maxValues(nmax) {
        this.nmax = nmax;
        return this;
    }

    reject() {
        this.rejects = Array.from(new Set([...this.rejects, ...arguments]));
        return this;
    }

    accept() {
        if (this.accepts === null) {
            this.accepts = [];
        }

        this.accepts = Array.from(new Set([...this.accepts, ...arguments]));
        return this;
    }
}

Wildcard.fromJson = function(json) {
    try {
        const model = new JsonModel(json);
        return new Wildcard().minValues(model.get('nmin')).maxValues(model.get('nmax')).accept(...model.get('accepts')).reject(...model.get('rejects'));
    }
    catch(err) {
        return json;
    }
};
class TreeNode {
    constructor(pi, vi, length) {
        this.pi = pi;
        this.vi = vi;
        this.length = length;
        this.parent = null;
        this.children = [];
    }

    addChild(child) {
        this.children.push(child);
        child.parent = this;
        return child;
    }

    toString() {
        let node = this;
        let o = '';

        // eslint-disable-next-line no-constant-condition
        while(true) {
            let item = `p${node.pi}->{0}`;

            if (node.length > 0) {
                item = `p${node.pi}->[${node.vi}:${node.vi + node.length - 1}]`;
            }

            if (o === '') {
                o = item;
            } else {
                o = `${item},${o}`;
            }

            if (node.parent === null) {
                break;
            }

            node = node.parent;
        }

        return `[${node.vi}]->${o}`;
    }
}

class ArrayPatternMatcher {
    constructor(pattern) {
        this.logger = new Logger('ArrayPatternMatcher');

        // Compile pattern
        this.pattern = pattern.map(pe => {
            if (pe instanceof Wildcard) {
                return pe;
            }

            return new Wildcard().minValues(1).maxValues(1).accept(pe);
        });

        this.totalMinLength = this.pattern.reduce((totalMinLength, wildcard) => totalMinLength + wildcard.nmin, 0);
    }

    booleanMatch(input) {
        try {
            this.treeMatch(input);
            return true;
        }
        catch(err) {
            return false;
        }
    }

    treeMatch(input) {
        if (this.pattern.length === 0) {
            throw new Error(`Empty pattern given`);
        }

        const start = Date.now();

        const fullGrownTree = this.__findFullGrownTree(input);

        if (fullGrownTree === null) {
            throw new Error(`No match found`);
        }

        this.logger.verbose(`Match found: ${fullGrownTree.toString()} in ${Date.now() - start}ms`);

        return fullGrownTree;
    }

    __findFullGrownTree(input) {
        const wildcard = this.pattern[0];

        const treeRoots = [];

        for (let i = 0; i <= input.length; i++) {
            if (i > input.length - this.totalMinLength) {
                // Does not fit by evaluating the overall minimum length of the pattern
                break;
            }

            if (wildcard.nmin === 0) {
                // This is always a potential starting point
                treeRoots.push(new TreeNode(0, i, 0));
                continue;
            }

            if (i + wildcard.nmin > input.length) {
                // No more space in input to squeeze in another match with given minimum length
                continue;
            }

            const accepts = wildcard.accepts;
            const rejects = wildcard.rejects || [];

            if (!Array.isArray(accepts) && rejects.length === 0) {
                // All accept, no reject
                treeRoots.push(new TreeNode(0, i, wildcard.nmin));
                continue;
            }

            for (let j = i; j < input.length; j++) {
                if (Array.isArray(accepts) && accepts.indexOf(input[j]) === -1 || rejects.indexOf(input[j]) !== -1) {
                    // Non-matching value found >> Break
                    break;
                }

                if (j - i + 1 > wildcard.nmax) {
                    // +1 since we are comparing 0-based indices with 1-based counts
                    // Maximum successive values reached
                    break;
                }

                if (j - i + 1 >= wildcard.nmin) {
                    // +1 since we are comparing 0-based indices with 1-based counts
                    // Minimum length is met
                    treeRoots.push(new TreeNode(0, i, j - i + 1));
                }
            }
        }

        for (let treeRoot of treeRoots) {
            const grownTree = this.__grow(treeRoot, input);

            if (grownTree !== null) {
                return grownTree;
            }
        }

        return null;
    }

    __grow(node, input) {
        // Pattern to check
        const cpi = node.pi + 1;

        if (cpi === this.pattern.length) {
            return node;
        }

        const childWildcard = this.pattern[cpi];

        if (childWildcard.nmin === 0) {
            // If required minimum length is 0, squeeze it in
            node.addChild(new TreeNode(cpi, node.vi + node.length, 0));
        } else {
            if (node.vi + node.length + childWildcard.nmin > input.length) {
                // No more space in input to squeeze in the current pattern
                return null;
            }
        }

        // Undefined for accepts means everything is accepted
        // Therefore no initialization if undefined
        const accepts = childWildcard.accepts;
        // undefined for rejects is the same as an empty array
        const rejects = childWildcard.rejects || [];

        for (let i = node.vi + node.length, j = 0; i < input.length; i++, j++) {
            if (j + 1 > childWildcard.nmax) {
                // +1 since we are comparing 0-based indices with 1-based counts
                // Maximum successive values reached
                break;
            }

            if (!Array.isArray(accepts) && rejects.length === 0) {
                // All accept, no reject
                if (j + 1 >= childWildcard.nmin) {
                    // +1 since we are comparing 0-based indices with 1-based counts
                    // Minimum length is met
                    node.addChild(new TreeNode(cpi, node.vi + node.length, j + 1));
                }

                continue;
            }

            if (Array.isArray(accepts) && accepts.indexOf(input[i]) === -1 || rejects.indexOf(input[i]) !== -1) {
                // Non-matching value found >> Break
                break;
            }

            if (j + 1 >= childWildcard.nmin) {
                node.addChild(new TreeNode(cpi, node.vi + node.length, j + 1));
            }
        }

        for (let child of node.children) {
            const grownTree = this.__grow(child, input);

            if (grownTree !== null) {
                return grownTree;
            }
        }

        return null;
    }
}

ArrayPatternMatcher.fromJson = function(jsonPattern) {
    return new ArrayPatternMatcher(ArrayPatternMatcher.patternFromJson(jsonPattern));
};

ArrayPatternMatcher.patternFromJson = function(jsonPattern) {
    if (!Array.isArray(jsonPattern)) {
        throw new Error(`Input needs to be a pattern in form of an array`);
    }

    const pattern = [];

    for (let wildcard of jsonPattern) {
        pattern.push(Wildcard.fromJson(wildcard));
    }

    return pattern;
};

/*
    let matchers = [
        new ArrayPatternMatcher([ new Wildcard(), new Wildcard().accept(2), new Wildcard().minValues(2), new Wildcard(), new Wildcard() ]),
        new ArrayPatternMatcher([ new Wildcard().minValues(3) ]),
        new ArrayPatternMatcher([ new Wildcard(), 1, new Wildcard(), 2 ]),
        new ArrayPatternMatcher([ 3, new Wildcard() ]),
        new ArrayPatternMatcher([ 3, new Wildcard().reject(5), 19 ]),
        new ArrayPatternMatcher([ 3, 5, new Wildcard().minValues(3).maxValues(5), 1]),
        new ArrayPatternMatcher([ new Wildcard().minValues(4) ]),
        new ArrayPatternMatcher([ 10, new Wildcard(), 20, new Wildcard().reject(3), 19 ]),
        new ArrayPatternMatcher([ 20, new Wildcard(), 19 ]),
        new ArrayPatternMatcher([ 3, 5, new Wildcard().minValues(3).maxValues(6).reject(26), 3]),
        new ArrayPatternMatcher([]),
        new ArrayPatternMatcher([ 3, 5, new Wildcard().minValues(3).maxValues(6).reject(25), 3]),
        new ArrayPatternMatcher([ 3, 5, new Wildcard().minValues(3).maxValues(5), 3])
    ];

    let input = [ 1, 2, 3, 5, 9, 10, 20, 25, 33, 1, 3, 20, 19, 3, 7, 9, 130, 99, 2, 2 ];

    const times = [];
    for (let i = 0; i < 1000; i++) {
        const startMatchAtMs = Date.now();

        for (let matcher of matchers) {
            matcher.booleanMatch(input);
        }

        times.push(Date.now() - startMatchAtMs);
    }

    const avgTimeMs = times.reduce((sum, timeMs) => sum + timeMs, 0) / times.length;
    const avgSingleMatchTimeMs = avgTimeMs / matchers.length;

    console.log(`Average time for ${matchers.length} matchings = ${avgTimeMs}ms`);
    console.log(`Average time for a single match = ${avgSingleMatchTimeMs}ms`);

    // TODO:
    // - Integrate jsonQuery to be able to process "complex" values
    // - Integrate timstamp into Pattern to be able to express "Has to have happened in 5000ms"
    //   - Have timerange constraints for wildcards with multiple values
    // - Return segmented pattern instead of boolean containing the total time of the segments (if there is more than one value in a segment. If only one value is available in a segment, one can take the time difference to the next segment)
*/

module.exports = {
    ArrayPatternMatcher,
    Wildcard
};