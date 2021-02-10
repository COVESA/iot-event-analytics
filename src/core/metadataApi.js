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
const cors = require('cors');
const debug = require('debug');
const jsonata = require('jsonata');
const path = require('path');

const SwaggerApi = require('./swaggerApi');

module.exports = class MetadataApi extends SwaggerApi {
    constructor(metadataManager) {
        super();
        this.metadataManager = metadataManager;
        this.logError = debug('iotea.core.metadataapi:err');
    }

    /**
     * @openapi
     * tags:
     *   name: MetadataApiV1
     *   description: Metadata API V1
     */
    createApiV1() {
        const router = express.Router();

        router.use(cors());

        this.__setupSwagger(
            router,
            './metadataApi.swagger.options.json',
            [
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'types.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'type.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'feature.yml'),
                path.resolve(__dirname, '..', '..', 'resources', 'swagger', 'error.yml'),
                path.resolve(__dirname, 'metadataApi.js')
            ]
        );

        /**
         * @openapi
         * /types:
         *   get:
         *     summary: Get all available types
         *     tags: [ MetadataApiV1 ]
         *     responses:
         *       "200":
         *         description: All types successfully returned
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Types"
         *       "404":
         *         description: Types not found
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Error"
         */
        router.get('/types', async (req, res) => {
            try {
                const typeMap = await this.metadataManager.getTypeMap();
                res.json(await this.__transformTypeMap(typeMap));
            }
            catch(err) {
                this.logError(err);
                res.status(500).json({ error: { message: err.message }});
            }
        });

        /**
         * @openapi
         * /types/{type}:
         *   get:
         *     summary: Get all features of a specific type
         *     tags: [ MetadataApiV1 ]
         *     parameters:
         *       - in: path
         *         name: type
         *         schema:
         *           type: string
         *           default: "default"
         *     responses:
         *       "200":
         *         description: Type successfully returned
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Type"
         *       "404":
         *         description: type not found
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Error"
         *
         */
        router.get('/types/:type', async (req, res) => {
            let featureMap = null;

            try {
                featureMap = await this.metadataManager.resolveMetaFeatures(req.params.type);
            }
            catch(err) {
                res.status(404).json({ error: { message: err.message }});
            }

            if (featureMap === null) {
                return;
            }

            try {
                const transformedFeatureMap = await this.__transformFeatureMap(featureMap);
                res.json(transformedFeatureMap)
            }
            catch(err) {
                res.status(500).json({ error: { message: err.message }});
            }
        });

        /**
         * @openapi
         * /types/{type}/features/{feature}:
         *   get:
         *     summary: Get a specific feature
         *     tags: [ MetadataApiV1 ]
         *     parameters:
         *       - in: path
         *         name: type
         *         schema:
         *           type: string
         *           default: "default"
         *       - in: path
         *         name: feature
         *         schema:
         *           type: string
         *     responses:
         *       "200":
         *         description: Feature successfully returned
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Feature"
         *       "404":
         *         description: Feature not found
         *         content:
         *           application/json:
         *             schema:
         *               $ref: "#/components/schemas/Error"
         */
        router.get('/types/:type/features/:feature', async (req, res) => {
            let feature = null;

            try {
                feature = await this.metadataManager.resolveMetaFeature(req.params.type, req.params.feature);
            }
            catch(err) {
                this.logError(err);
                res.status(404).json({ error: { message: `Could not get feature ${req.params.feature} for type ${req.params.type}` }});
            }

            if (feature === null) {
                return;
            }

            try {
                const transformedFeature = await this.__transformFeature(feature);
                res.json(transformedFeature);
            }
            catch(err) {
                res.status(500).json({ error: { message: err.message }});
            }
        });

        router.all('*', (req, res) => {
            res.redirect(`${req.baseUrl}/docs`);
        });

        return router;
    }

    __transformFeature(feature) {
        const transformer = jsonata(`
            $sift(**[0], function($v, $k) { $k != "$unit" })
        `);

        return this.__transform(feature, transformer);
    }

    __transformFeatureMap(featureMap) {
        const transformer = jsonata(`
            $merge($each(**[0], function($v, $k) {
                {
                    $k: $sift($v, function($v, $k) { $k != "$unit" })
                }
            }))
        `);

        return this.__transform(featureMap, transformer);
    }

    __transformTypeMap(typeMap) {
        const transformer = jsonata(`
            $merge($each(**[0], function($v, $k) {
                {
                    $k: {
                        "segment": $v.segment,
                        "features": $merge($each($v.features, function($v, $k) {
                            {
                                $k: $sift($v, function($v, $k) { $k != "$unit" })
                            }
                        }))
                    }
                }
            }))
        `);

        return this.__transform(typeMap, transformer);
    }
};