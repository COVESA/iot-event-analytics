/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const fs = require('fs');
const path = require('path');
const yargs = require('yargs');
const Terminal = require('../../tools/terminal');

const args = yargs
    .option('directory', {
        alias: 'd',
        description: 'Package output directory',
        required: false,
        default: null
    })
    .help()
    .alias('help', 'h');

(async () => {
    // Creates a new installable Node.js package in the lib folder
    const projectRoot = path.resolve(__dirname, '..', '..', '..');
    let absPackageDir = path.resolve(projectRoot, 'src/sdk/javascript/lib');

    const argv = args.argv;

    if (argv.directory) {
        if (!path.isAbsolute(argv.directory)) {
            console.error(new Error(`Given path "${argv.directory}" must be absolute`));
            return;
        }

        absPackageDir = argv.directory;
    }

    const packageInfo = require(path.resolve(projectRoot, 'package.json'));

    if (!fs.existsSync(absPackageDir)) {
        fs.mkdirSync(absPackageDir);
    }

    const terminal = new Terminal();
    const packageName = `${packageInfo.name}-${packageInfo.version}.tgz`;
    const absPackagePath = path.join(absPackageDir, packageName);

    await terminal.runCommand('yarn', [
        'pack',
        '--filename',
        `"${absPackagePath}"`
    ], projectRoot, msg => {
        console.log(msg);
    });

    console.log(absPackagePath);
})();