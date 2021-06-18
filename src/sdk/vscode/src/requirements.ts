/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

import * as fs from 'fs';
import * as path from 'path';
import * as semver from 'semver';
import { Terminal } from './terminal';
import {
    getDockerComposeCmd,
    executeDockerCmd,
    getDockerCmdArgs,
    getPipModule,
    getPythonCmd
} from './util';

async function ensureCliVersion(range: semver.Range, cmd: string, args: string[], env: any = {}, cwd: string = __dirname, extractCliVersion: (cliVersion: string) => string = cliVersion => cliVersion): Promise<semver.SemVer> {
    const t = new Terminal();

    const versionOutput: string = await t.executeCommand(cmd, args, cwd, (msg: string) => {}, env);

    const extractedCliVersion: string = extractCliVersion(versionOutput.trim());

    const parsedSemVer: semver.SemVer | null = semver.parse(extractedCliVersion);

    if (parsedSemVer === null) {
        throw new Error(`Cannot parse version "${extractedCliVersion}" from command "${cmd} ${args.join(' ')}"`);
    }

    if (!semver.satisfies(parsedSemVer, range)) {
        throw new Error(`Given version "${versionOutput}" does not satisfy ${range}`);
    }

    return parsedSemVer;
}

export async function ensureDockerVersion(range: semver.Range): Promise<semver.SemVer> {
    // Check Docker Engine version
    const dockerCmdArgs = getDockerCmdArgs(['version', '--format', '{{.Client.Version}}']);
    return ensureCliVersion(range, dockerCmdArgs.cmd, dockerCmdArgs.args, dockerCmdArgs.env, __dirname);
}

export async function ensureDockerConfiguration(): Promise<void> {
    // See if Docker run hello-world runs
    return executeDockerCmd([ 'run', '--rm', 'hello-world' ], __dirname).then(() => {});
}

export async function ensureComposeVersion(range: semver.Range): Promise<semver.SemVer> {
    return ensureCliVersion(range, getDockerComposeCmd(), [ '--version' ], {}, __dirname, (cliVersion: string) => {
        // cliVersion >> docker-compose version 1.27.4, build <commit hash>
        const composeVersionMatch = (/^.*([0-9]+\.[0-9]+\.[0-9]+).*(?:\s((?:[0-9a-f]+)|unknown))$/mgi).exec(cliVersion);

        if (composeVersionMatch === null) {
            throw new Error(`Cannot parse docker-compose version string "${cliVersion}"`);
        }

        if (composeVersionMatch[2] === null || composeVersionMatch[2] === 'unknown') {
            return composeVersionMatch[1];
        }

        return `${composeVersionMatch[1]}+${composeVersionMatch[2].toLowerCase()}`;
    });
}

export async function ensureNodeVersion(range: semver.Range): Promise<semver.SemVer> {
    // cliVersion >> v12.13.0
    return ensureCliVersion(range, 'node', [ '--version' ]);
}

export async function ensureGitVersion(range: semver.Range): Promise<semver.SemVer> {
    return ensureCliVersion(range, 'git', [ '--version' ], {}, __dirname, (cliVersion: string) => {
        // cliVersion >> git version 2.28.0.windows.1
        const gitVersionMatch = (/^.*([0-9]+\.[0-9]+\.[0-9]+).*$/g).exec(cliVersion);

        if (gitVersionMatch === null) {
            throw new Error(`Cannot parse git version string "${cliVersion}"`);
        }

        return gitVersionMatch[1];
    });
}

export async function ensurePythonVersion(range: semver.Range): Promise<semver.SemVer> {
    return ensureCliVersion(range, getPythonCmd(), [ '--version' ], {}, __dirname, (cliVersion: string) => {
        // cliVersion >> Python 3.7.4
        const pythonVersionMatch = (/^.*([0-9]+\.[0-9]+\.[0-9]+).*$/g).exec(cliVersion);

        if (pythonVersionMatch === null) {
            throw new Error(`Cannot parse python version string "${cliVersion}"`);
        }

        return pythonVersionMatch[1];
    });
}

export async function ensurePipVersion(range: semver.Range): Promise<semver.SemVer> {
    return ensureCliVersion(range, getPythonCmd(), [ '-m', getPipModule(), '--version' ], {}, __dirname, (cliVersion: string) => {
        // cliVersion >> pip 19.1.1 from <some directory> (python 3.7)
        const pipVersionMatch = (/^.*([0-9]+\.[0-9]+\.[0-9]+).*$/g).exec(cliVersion);

        if (pipVersionMatch === null) {
            throw new Error(`Cannot parse pip version string "${cliVersion}"`);
        }

        return pipVersionMatch[1];
    });
}

export async function ensureIoTeaProjectRootDirAt(ioteaProjectRootDir: string): Promise<void> {
    if (!fs.existsSync(path.resolve(ioteaProjectRootDir, 'node_modules'))) {
        throw new Error(`You have to install all dependencies for the IoT Event Analytics project using yarn`);
    }
}

export async function ensureRuntimeRequirements(requirements: any, onRequirement: (component: string, requirement: any) => void, onRequirementSuccess: (component: string, requirement: string, expected: any, actual: any) => void): Promise<void> {
    for (let component in requirements) {
        for (let requirement in requirements[component]) {
            onRequirement(component, requirement);

            switch(requirement) {
                case 'version': {
                    let version: semver.SemVer | null = null;

                    switch(component) {
                        case 'docker': {
                            version = await ensureDockerVersion(requirements[component].version);
                            break;
                        }
                        case 'compose': {
                            version = await ensureComposeVersion(requirements[component].version);
                            break;
                        }
                        case 'node': {
                            version = await ensureNodeVersion(requirements[component].version);
                            break;
                        }
                        case 'git': {
                            version = await ensureGitVersion(requirements[component].version);
                            break;
                        }
                        case 'python': {
                            version = await ensurePythonVersion(requirements[component].version);
                            break;
                        }
                        case 'pip': {
                            version = await ensurePipVersion(requirements[component].version);
                            break;
                        }
                        default: {
                            throw new Error(`Version check for ${component} is not supported`);
                        }
                    }

                    onRequirementSuccess(component, requirement, requirements[component].version, version);
                    break;
                }
                case 'config': {
                    switch(component) {
                        case 'docker': {
                            await ensureDockerConfiguration();
                            break;
                        }
                    }

                    onRequirementSuccess(component, requirement, true, true);
                }
            }
        }
    }
}