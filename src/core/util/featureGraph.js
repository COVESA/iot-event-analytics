/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const {
    ANY_FEATURE,
    ALL_TYPES
} = require('../rules');

const Logger = require('./logger');

class FeatureGraphNode {
    constructor(id) {
        this.id = id;
        this.visited = false;
        this.children = [];
    }
}

module.exports = class FeatureGraph {
    constructor() {
        this.ids = [];
        this.nodes = {};
        this.state = null;
        this.logger = new Logger('FeatureGraph');
    }

    freeze() {
        const state = {
            nodes: this.ids.reduce((acc, id) => {
                acc[id] = new FeatureGraphNode(id);
                return acc;
            }, {}),
            ids: [...this.ids]
        };

        // Update referenced children
        for (const id of this.ids) {
            for (const child of this.nodes[id].children) {
                state.nodes[id].children.push(state.nodes[child.id]);
            }
        }

        this.state = state;
    }

    melt() {
        // Go back to the last frozen state if available
        if (this.state === null) {
            return;
        }

        this.ids = this.state.ids;
        this.nodes = this.state.nodes;

        this.state = null;
    }

    addDependency(from, to) {
        let fromNode = this.nodes[from];

        if (fromNode === undefined) {
            fromNode = new FeatureGraphNode(from);
            this.ids.push(from);
            this.nodes[from] = fromNode;
        }

        let toNode = this.nodes[to];

        if (toNode === undefined) {
            toNode = new FeatureGraphNode(to);
            this.ids.push(to);
            this.nodes[to] = toNode;
        }

        if (fromNode.children.indexOf(toNode) === -1) {
            fromNode.children.push(toNode);
        }
    }

    containsCycles() {
        const paths = this.getAllPaths();

        // eslint-disable-next-line no-useless-escape
        const anyFeatureRegex = /^[^\.]+\.\*$/m;

        const serializePath = path => `"${path.join(' -> ')}"`;

        for (let path of paths) {
            if (path.indexOf(`${ALL_TYPES}.${ANY_FEATURE}`) !== -1 && path.length > 1) {
                // *.* leads to a cycle, if more than one element is in the path
                this.logger.warn(`General wildcard found in a graph path ${serializePath(path)}`);
                return true;
            }

            if (paths.reduce((acc, path) => acc || (new Set(path)).size !== path.length, false)) {
                // If path contains the same entry twice
                this.logger.warn(`Feature cycle found in path ${serializePath(path)}`);
                return true;
            }

            for (let i = 0; i < path.length; i++) {
                const pathPart = path[i];

                // Handle <type>.* entries
                if (anyFeatureRegex.test(pathPart)) {
                    const typePrefix = pathPart.slice(0, -1);

                    const pathPrefixMatches = path.reduce((acc, otherPathPart, idx) => {
                        if (idx === i) {
                            return acc;
                        }

                        return acc = acc || otherPathPart.indexOf(typePrefix) === 0;
                    }, false);

                    if (pathPrefixMatches) {
                        this.logger.warn(`Feature wildcard for type prefix "${typePrefix}" matches other entry in path ${serializePath(path)}`);
                        return true;
                    }
                }

                anyFeatureRegex.lastIndex = 0;
            }

            // *.feature entries cannot be checked, since it's unclear, which type they actually belong to
            // Simply return false here
            return false;
        }


        return this.getAllPaths().reduce((acc, path) => acc || (new Set(path)).size !== path.length, false);
    }

    toString() {
        return this.getAllPaths().map(path => path.join('->')).join('\n');
    }

    getAllPaths() {
        let paths = [];

        this.ids.sort().forEach(id => {
            paths = [...paths, ...this.__getPathsFromSource(id)];
        });

        this.ids.forEach(id => this.nodes[id].visited = false);

        return paths;
    }

    __getPathsFromSource(id, path = [], paths = []) {
        if ((new Set(path)).size !== path.length) {
            paths.push([...path]);
            return paths;
        }

        if (path.length === 0) {
            // If node was already covered by an other traversal
            if (this.nodes[id].visited) {
                return paths;
            }

            path = [ id ];
        }

        const node = this.nodes[path[path.length - 1]];

        node.visited = true;

        if (node.children.length === 0) {
            paths.push([...path]);
            return paths;
        }

        for (let child of node.children) {
            this.__getPathsFromSource(id, [...path, child.id], paths);
        }

        return paths;
    }
};