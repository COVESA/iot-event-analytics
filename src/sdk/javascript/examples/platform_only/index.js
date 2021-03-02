/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const spawn = require('child_process').spawn;

process.env.MQTT_TOPIC_NS = 'iotea/';

const path = require('path');
const express = require('express');

const Logger = require('../../../../core/util/logger');
const JsonModel = require('../../../../core/util/jsonModel');
const config = new JsonModel(require('./config.json'));

process.env.LOG_LEVEL = config.get('loglevel', Logger.ENV_LOG_LEVEL.INFO);

const ConfigManager = require('../../../../core/configManager');
const MetadataApi = require('../../../../core/metadataApi');

const cf = new ConfigManager(config.get('mqtt.connectionString'), config.get('platformId'));

const metadataApi = new MetadataApi(cf.getMetadataManager());

const app = express();
app.use('/metadata/api/v1', metadataApi.createApiV1());
app.listen(config.get('api.port'));

const platformLogger = new Logger('Platform');

cf.start(config.get('talentDiscoveryIntervalMs'), path.resolve(config.get('platformConfigDir'), 'types.json'), path.resolve(config.get('platformConfigDir'), 'uom.json'))
    .then(() => {
        const procs = [];

        procs.push(...spawnIngestionChildProcess(config.get('ingestion.processes', 1)));
        procs.push(...spawnEncodingChildProcess(config.get('encoding.processes', 1)));
        procs.push(...spawnRoutingChildProcess(config.get('routing.processes', 1)));

        return procs;
    })
    .then(procs => {
        platformLogger.info(`${procs.length} child processes started`);
    })
    .catch(err => {
        platformLogger.error('Failed to start IoTea Event Analytics Platform', null, err);
    });

function spawnIngestionChildProcess(count = 1) {
    return spawnChildProcesses('node', [ 'startIngestion.js' ], count);
}

function spawnEncodingChildProcess(count = 1) {
    return spawnChildProcesses('node', [ 'startEncoding.js' ], count);
}

function spawnRoutingChildProcess(count = 1) {
    return spawnChildProcesses('node', [ 'startRouting.js' ], count);
}

function spawnChildProcesses(command, args = [], count = 1) {
    const procs = [];

    for (let i = 0; i < count; i++) {
        const proc = spawn(
            command,
            args, {
                cwd: __dirname,
                shell: true
            }
        );

        proc.stdout.on('data', buf => {
            console.log(buf.toString('utf8').trim());
        });

        proc.stderr.on('data', buf => {
            console.log(buf.toString('utf8').trim());
        });

        procs.push(proc);
    }

    return procs;
}