/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const { spawn } = require('child_process');

export class Terminal {
    async executeCommand(cmd: string, args: string[], cwd: string, onStdOut = (msg: string) => {}): Promise<string> {
        if (cmd.toLowerCase() === 'python') {
            try {
                // Check for Anaconda installation
                await this.executeCommand('conda', [ '--version '], cwd);
                cmd = 'conda';
                args.unshift('activate&&python');
            }
            catch(err) {
                // No conda installation found
            }
        }

        return new Promise((resolve, reject) => {
            const proc = spawn(cmd, args, {
                cwd,
                shell: true
            });

            let stdout = '';
            let stderr = '';

            proc.stdout.on('data', (buf: Buffer) => {
                const stringifiedBuffer = buf.toString('utf8');
                onStdOut(stringifiedBuffer);
                stdout += stringifiedBuffer;
            });

            proc.stderr.on('data', (buf: Buffer) => {
                stderr += buf.toString('utf8');
            });

            proc.on('close', (code: number) => {
                if (code !== 0) {
                    reject(new Error(stderr.trim()));
                    return;
                }

                resolve(stdout.trim());
            });
        });
    }
}
