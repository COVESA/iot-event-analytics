/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

 const yargs = require('yargs');
const path = require('path');
const fs = require('fs').promises;
const Mustache = require('mustache');

const Talent = require('../../core/talent');
const {
    Worker
} = require('../../core/talent.mr');

const {
    MSG_TYPE_DISCOVERY,
    MSG_TYPE_EVENT,
    MSG_TYPE_ERROR
} = require('../../core/constants');

const args = yargs
    .option('file', {
        alias: 'f',
        description: 'File, which contains the talent class',
        required: true
    })
    .option('out', {
        alias: 'o',
        description: 'output filename',
        default: null
    })
    .option('name', {
        alias: 'n',
        description: 'Named export',
        default: null
    })
    .option('provider', {
        alias: 'p',
        description: 'The cloud provider',
        choices: [ 'aws' ],
        default: 'aws'
    })
    .option('arguments', {
        alias: 'a',
        description: 'Space separated params of given types, which are passed to the constructor of the given class',
        example: '"1:number" "true:boolean" "Das ist ein Test:string" "{\'foo\': 123, \'bar\': { \'test\': 1234}}:object"',
        default: [],
        type: 'array'
    })
    .help()
    .alias('help', 'h');

const argv = args.argv;

let TalentClass = require(path.resolve(process.cwd(), argv.file));

if (argv.name !== null) {
    TalentClass = TalentClass[argv.name];
}

// Get params from argv and parse them accordingly to initialize the Talent properly
const params = [];

if (argv.arguments.length > 0) {
    const regex = /^(.+):([^:]+)$/gm;

    for (let param of argv.arguments) {
        const match = regex.exec(param);

        regex.lastIndex = 0;

        if (match === null) {
            throw new Error(`Parameter ${param} has to be in form "<value>:<number|string|boolean|object>"`);
        }

        switch(match[2]) {
            case 'number': params.push(parseFloat(match[1])); break;
            case 'string': params.push(match[1]); break;
            case 'boolean': params.push(match[1] === 'true'); break;
            case 'object': params.push(JSON.parse(match[1].replace(/\'/g, '"'))); break;
            default: throw new Error('Parameter type needs to be one of number, string, boolean, or object');
        }
    }
}

const talentInstance = new TalentClass(...params);

if (!(talentInstance instanceof Talent)) {
    throw new Error('Class needs to subclass Talent');
}

const isWorker = talentInstance instanceof Worker;

// Make sure, isRemote is overridden and returns true
talentInstance.isRemote = ()  => true;

fs.readdir(path.resolve(__dirname, 'templates'))
    .then(files => files.filter(fileName => fileName.indexOf('.mustache') > -1))
    .then(async files => {
        const partials = {};

        for (let file of files) {
            const template = (await fs.readFile(path.resolve(__dirname, 'templates', file))).toString('utf8');
            Mustache.parse(template);
            partials[path.basename(file, 'mustache').slice(0, -1)] = template;
        }

        return partials;
    })
    .then(partials => {
        const view = {
            MSG_TYPE_ERROR,
            MSG_TYPE_DISCOVERY,
            MSG_TYPE_EVENT,
            TALENT_ID: talentInstance.id,
            IS_CLOUD_WORKER: isWorker,
            DISCOVERY_MESAGE: JSON.stringify(talentInstance.__createDiscoveryResponse(), null, 4),
        };

        if (isWorker) {
            view['MAPPER_TALENT_ID'] = talentInstance.mapperId;
        }

        return Mustache.render('{{> ' + argv.provider + '}}', view, partials);
    })
    .then(renderedTemplate => {
        return require('js-beautify').js(renderedTemplate, {
            indent_size: 4,
            space_in_empty_paren: true,
            space_after_anon_function: true
        });
    })
    .then(template => {
        if (argv.out === null) {
            console.log(template);
            return;
        }

        const absOutPath = path.resolve(process.cwd(), argv.out);

        return fs.writeFile(absOutPath, template, 'utf-8')
            .then(() => {
                console.log(`Successfully written output to ${absOutPath}`);
            });
    })
    .catch(err => {
        console.error(err);
    });