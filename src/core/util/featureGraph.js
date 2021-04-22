/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

/**
 * Module Feature Graph.
 * 
 * @module featureGraph
 */
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

/**
 * The class represents a graph of nodes. It provides methods to add dependencies between nodes, to record the state of
 * the graph (freeze), to restore the previously saved state (melt), to detect cycles within the graph.
 */
class FeatureGraph {
    /**
     * Constructs an instance of FeatureGraph with no nodes.
     */
    constructor() {
        this.ids = [];
        this.nodes = {};
        this.state = null;
        this.logger = new Logger('FeatureGraph');
    }

    /**
     * Stores the current state of the graph. Calling the {@link module:featureGraph~FeatureGraph#melt} method restores
     * the graph to the last frozen state. Calling freeze() again overwrites the previously frozen state.
     */
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

    /**
     * Restores the state of the graph to the previously frozen state. If
     * {@link module:featureGraph~FeatureGraph#freeze} had not been called before, the method does nothing. 
     */
    melt() {
        // Go back to the last frozen state if available
        if (this.state === null) {
            return;
        }

        this.ids = this.state.ids;
        this.nodes = this.state.nodes;

        this.state = null;
    }

    /**
     * Adds a dependency between two nodes in the graph. If the nodes are not existing in the graph yet, they will be
     * added.
     *
     * @param {*} from - Id of the source node.
     * @param {*} to - Id of the destination node.
     */
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

    /**
     * Checks if the dependencies of the graph form cycles. For example:
     * ```
     *   featureGraph.addDependency('1', '2');
     *   featureGraph.addDependency('2', '3');
     *   featureGraph.addDependency('3', '1');
     *   console.log(featureGraph.containsCycles());
     * ```
     * will result in _true_.
     * @returns {boolean} True if there is a cyclic dependency in the graph.
     */
    containsCycles() {
        const paths = this.getAllPaths();

        // eslint-disable-next-line no-useless-escape
        const anyFeatureRegex = /^[^\.]+\.\*$/m;
        // eslint-disable-next-line no-useless-escape
        const anyTypeRegex = /^\*\.[^\.]+$/m;

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

                // Reset regexes
                anyTypeRegex.lastIndex = 0;
                anyFeatureRegex.lastIndex = 0;

                // Handle *.<feature> entries
                if (anyTypeRegex.test(pathPart)) {
                    // Skip *.feature entries since they cannot be checked. It's unclear, which type they actually belong to
                    // i.e. *.a -> b -> foo.a
                    continue;
                }

                // Handle <type>.* entries
                if (anyFeatureRegex.test(pathPart)) {
                    // i.e. b.foo -> c -> b.*
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
            }
        }

        return this.getAllPaths().reduce((acc, path) => acc || (new Set(path)).size !== path.length, false);
    }

    toString() {
        return this.getAllPaths().map(path => path.join('->')).join('\n');
    }

    /**
     * Gets the longest paths in the graph. For example:
     * ```
     *    featureGraph.addDependency('1', '2');
     *    featureGraph.addDependency('2', '3');
     *    featureGraph.addDependency('3', '4');
     *    console.log(featureGraph.getAllPaths());
     * ```
     * will result in [ [ '1', '2', '3', '4' ] ].
     * <p>
     * In cycling paths, the nodes are traversed only once.
     * 
     * @returns {Array<Array>} - An array of paths.
     */
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

            path = [id];
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
}

module.exports = FeatureGraph;