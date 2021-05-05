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
const { spawn } = require('child_process');

class Terminal {
    async runCommand(cmd, args, cwd, onStdOut = () => {}, env = {}) {
        return new Promise((resolve, reject) => {
            const proc = spawn(cmd, args, {
                cwd,
                shell: true,
                env: { ...process.env, ...env }
            });

            let stdout = '';
            let stderr = '';

            proc.stdout.on('data', buf => {
                const stringifiedBuffer = buf.toString('utf8');
                onStdOut(stringifiedBuffer);
                stdout += stringifiedBuffer;
            });

            proc.stderr.on('data', buf => {
                stderr += buf.toString('utf8');
            });

            proc.on('close', code => {
                if (code !== 0) {
                    reject(new Error(stderr.trim()));
                    return;
                }

                resolve(stdout.trim());
            });
        });
    }
}

(async () => {
    // Creates a new installable Node.js package in the lib folder
    const projectRoot = path.resolve(__dirname, '..', '..', '..');
    const packageInfo = require(path.resolve(projectRoot, 'package.json'));

    const libraryDir = path.resolve(projectRoot, 'src/sdk/javascript/lib');
    if (!fs.existsSync(libraryDir)) {
        fs.mkdirSync(libraryDir);
    }

    const terminal = new Terminal();
    await terminal.runCommand('yarn', [
        'pack',
        '--filename',
        `src/sdk/javascript/lib/${packageInfo.name}-${packageInfo.version}.tgz`
    ], projectRoot, msg => {
        console.log(msg);
    });
})();