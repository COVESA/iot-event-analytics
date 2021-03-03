/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Ajv = require('ajv');
const jsonata = require('jsonata');
const fs = require('fs');
const path = require('path');

const Logger = require('./util/logger');

const {
    MSG_TYPE_EVENT,
    DEFAULT_INSTANCE,
    DEFAULT_TYPE,
    CHANNEL_STEP_VALIDATE,
    CHANNEL_STEP_TRANSFORM
} = require('./constants');

class ValidationStep {
    constructor(absSchemaPath) {
        this.absSchemaPath = absSchemaPath;
        const schema = JSON.parse(fs.readFileSync(this.absSchemaPath, { encoding: 'utf8' }));
        schema.$async = true;
        this.validate = ValidationStep.ajv.compile(schema);
    }

    handle(ev) {
        return this.validate(ev);
    }
}

ValidationStep.ajv = new Ajv();

class TransformationStep {
    constructor(absJnaPath) {
        this.absJnaPath = absJnaPath;
        this.jna = jsonata(fs.readFileSync(absJnaPath, { encoding: 'utf8' }));
    }

    handle(ev) {
        return new Promise((resolve, reject) => {
            this.jna.evaluate(ev, null, (err, res) => {
                if (err) {
                    reject(err);
                    return;
                }

                resolve(res);
            });
        });
    }
}

class Channel {
    constructor(channelsDir, configFile) {
        this.logger = new Logger('Channel');

        this.configPath = path.join(channelsDir, configFile);

        this.config = JSON.parse(fs.readFileSync(this.configPath, { encoding: 'utf8' }));

        this.pipeline = this.config.reduce((steps, step) => {
            if (step.type === CHANNEL_STEP_VALIDATE) {
                steps.push(new ValidationStep(path.resolve(channelsDir, step.path)));
            }

            if (step.type === CHANNEL_STEP_TRANSFORM) {
                steps.push(new TransformationStep(path.resolve(channelsDir, step.path)));
            }

            return steps;
        }, []);

        if (this.pipeline.length !== this.config.length) {
            throw new Error(`Invalid pipeline configuration found in channel ${configFile}`);
        }

        this.logger.info(`Channel loaded from ${this.configPath}`);
    }

    handle(ev) {
        return this.pipeline.reduce((ev, step) => ev.then(ev => step.handle(ev)), Promise.resolve(ev))
            .then(ev => {
                ev.type = ev.type || DEFAULT_TYPE;
                ev.instance = (ev.instance && ev.type !== DEFAULT_TYPE) ? ev.instance : DEFAULT_INSTANCE;
                ev.now = Date.now();
                ev.msgType = ev.msgType || MSG_TYPE_EVENT;
                return ev;
            });
    }
}

class Channels {}

Channels.load = function load(dir) {
    return new Promise((resolve, reject) => {
        fs.readdir(dir, { encoding: 'utf8'}, (err, files) => {
            if (err) {
                reject(err);
                return;
            }

            resolve(files.filter(file => file.indexOf('.channel.json') !== -1).map(channelConfigFile => new Channel(dir, channelConfigFile)));
        });
    });
};

module.exports = Channels;