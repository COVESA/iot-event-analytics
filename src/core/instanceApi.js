/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const express = require('express');
const debug = require('debug');
const jsonata = require('jsonata');
const path = require('path');

const SwaggerApi = require('./swaggerApi');
const Logger = require('./util/logger');
const clone = require('./util/clone');

module.exports = class InstanceApi extends SwaggerApi {
    constructor(instanceManager) {
        super();
        this.logger = new Logger('InstanceApi');
        this.instanceManager = instanceManager;
        this.logError = debug('iotea.core.instanceapi:err');
    }
    /**
     * @openapi
     * tags:
     *   name: InstanceApiV1
     *   description: Instance API V1
     */
    createApiV1() {
        const router = express.Router();

        this.__setupSwagger(
            router,
            './instanceApi.swagger.options.json',
            [
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'instances.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'instance.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'instanceFeature.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'feature.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'error.yml'),
                path.resolve(__dirname, 'instanceApi.js')
            ]
        );

        /**
         * @openapi
         * /subjects:
         *   get:
         *     summary: Gets all available subjects
         *     tags: [ InstanceApiV1 ]
         *     responses:
         *       "200":
         *         description: All subjects could be retrieved successfully
         *         content:
         *           application/json:
         *             schema:
         *               type: array
         *               items:
         *                 type: string
         */
        router.get('/subjects', async (req, res) => {
            res.json(this.instanceManager.getSubjects());
        });

        /**
         * @openapi
         * /subjects/{subject}/instances:
         *   get:
         *     summary: Gets all instances of a given subject
         *     tags: [ InstanceApiV1 ]
         *     parameters:
         *       - in: path
         *         name: subject
         *         schema:
         *           type: string
         *           default: "default"
         *     responses:
         *       "200":
         *         description: All instances of a given subject could be retrieved successfully
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Instances"
         *       "404":
         *         description: The subject could not be found
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Error"
         */
        router.get('/subjects/:subject/instances', async (req, res) => {
            let instances = null;

            try {
                instances = this.instanceManager.getInstances(req.params.subject);
            }
            catch(err) {
                // Subject could not be found
                res.status(404).json({ error: { message: err.message }});
                return;
            }

            try {
                const instanceMap = await this.__createInstanceMap(instances);

                for (let mappedInstanceId of Object.keys(instanceMap)) {
                    instanceMap[mappedInstanceId] = await this.__createInstanceWithMetaFeatures(instanceMap[mappedInstanceId]);
                }

                res.json(instanceMap);
            }
            catch(err) {
                this.logger.warn(err.message, null, err);
                res.status(500).json({ error: { message: err.message }});
            }
        });

        /**
         * @openapi
         * /subjects/{subject}/instances/{instanceId}:
         *   get:
         *     summary: Gets all instances of a given subject
         *     tags: [ InstanceApiV1 ]
         *     parameters:
         *       - in: path
         *         name: subject
         *         schema:
         *           type: string
         *           default: "default"
         *       - in: path
         *         name: instanceId
         *         schema:
         *           type: string
         *     responses:
         *       "200":
         *         description: A single instance by the given instanceId from the given subject could be retrieved successfully
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Instance"
         *       "404":
         *         description: The subject or instance could not be found
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Error"
         */
        router.get('/subjects/:subject/instances/:instanceId', async (req, res) => {
            let instances = null;

            try {
                instances = this.instanceManager.getInstances(req.params.subject, new RegExp(`^${req.params.instanceId}$`, 'm'));

                if (instances.length === 0) {
                    throw new Error(`The given instance could not be found for the subject ${req.params.subject}`);
                }
            }
            catch(err) {
                // Subject could not be found
                res.status(404).json({ error: { message: err.message }});
                return;
            }

            try {
                const instanceMap = await this.__createInstanceMap(instances);
                res.json(await this.__createInstanceWithMetaFeatures(instanceMap[req.params.instanceId]));
            }
            catch(err) {
                this.logger.warn(err.message, null, err);
                res.status(500).json({ error: { message: err.message }});
            }
        });

        /**
         * @swagger
         * /subjects/{subject}/instances/{instanceId}/features/{feature}:
         *   get:
         *     summary: Gets all instances of a given subject
         *     tags: [ InstanceApiV1 ]
         *     parameters:
         *       - in: path
         *         name: subject
         *         schema:
         *           type: string
         *           default: "default"
         *       - in: path
         *         name: instanceId
         *         schema:
         *           type: string
         *       - in: path
         *         name: feature
         *         schema:
         *           type: string
         *     responses:
         *       "200":
         *         description: A single feature of a given instanceId of a given subject could be retrieved successfully
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/InstanceFeature"
         *       "404":
         *         description: The subject, instance or feature could not be found
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Error"
         */
        router.get('/subjects/:subject/instances/:instanceId/features/:feature', async (req, res) => {
            // Returns a specific feature for a given instance and a feature
            let instance = null;
            let metaFeature = null;

            try {
                instance = this.instanceManager.getInstance(req.params.subject, req.params.instanceId);
                metaFeature = await this.instanceManager.getMetadataManager().resolveMetaFeature(instance.type, req.params.feature);
            }
            catch(err) {
                // Subject could not be found
                res.status(404).json({ error: { message: err.message }});
                return;
            }

            let $feature = null;

            try {
                $feature = instance.getFeatureAt(metaFeature.idx)
            }
            catch(err) {
                // Is not set yet, just ignore
            }

            res.json({
                $feature,
                $metadata: await this.__stripMetaFeature(metaFeature)
            });
        });

        router.all('*', (req, res) => {
            res.redirect(`${req.baseUrl}/docs`);
        });

        return router;
    }

    async __createInstanceWithMetaFeatures(mappedInstance) {
        mappedInstance = clone(mappedInstance);

        const metaFeatures = await this.instanceManager.getMetadataManager().resolveMetaFeatures(mappedInstance.type);

        for (let feature of Object.keys(metaFeatures)) {
            const metaFeature = metaFeatures[feature];

            mappedInstance.features[metaFeature.idx] = {
                id: feature,
                $metadata: metaFeature,
                $feature: mappedInstance.features[metaFeature.idx]
            };
        }

        // Create a featureMap
        const transformer = jsonata(`
            $merge(
                $map(
                    $filter(*, function ($v) {
                        $v.id != null
                    }), function($v, $i, $a) {
                        {
                            $a[$i].id: $sift($v, function($v, $k) { $k != "$unit" and $k != "id" })
                        }
                    }
                )
            )
        `);

        mappedInstance.features = await this.__transform(mappedInstance.features, transformer);

        return mappedInstance;
    }

    __createInstanceMap(instances) {
        const transformer = jsonata(`
            $merge(
                $map(
                    $filter(*, function ($v) {
                        $v.id != null
                    }), function($v, $i, $a) {
                        {
                            $a[$i].id: { "type": $v.type, "features": $v.features }
                        }
                    }
                )
            )
        `);

        return this.__transform(instances, transformer);
    }

    __stripMetaFeature(metaFeature) {
        const transformer = jsonata(`$sift(**[0], function($v, $k) { $k != "$unit" })`);
        return this.__transform(metaFeature, transformer);
    }
};