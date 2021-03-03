/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const swaggerJsdoc = require('swagger-jsdoc');
const swaggerUi = require('swagger-ui-express');

const Api = require('./api');

module.exports = class SwaggerApi extends Api {
    constructor() {
        super();
    }

    __setupSwagger(router, swaggerSpecOptionsFile, apis) {
        // Swagger set up
        const options = require(swaggerSpecOptionsFile);

        // Add schemas and route description
        options.apis.push(...apis);

        const swaggerSpec = swaggerJsdoc(options);

        router.use('/docs', swaggerUi.serve, swaggerUi.setup(swaggerSpec, {
            swaggerOptions: {
                showExtensions: true
            }
        }));

        router.get('/swagger.json', (req, res) => {
            res.json(swaggerSpec);
        });
    }
};