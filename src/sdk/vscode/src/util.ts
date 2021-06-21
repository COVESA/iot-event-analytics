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
import * as crypto from 'crypto';
import * as vscode from 'vscode';
import { ensureIoTeaProjectRootDirAt, ensureRuntimeRequirements } from './requirements';
import { Terminal } from './terminal';

export function copyDirContentsSync(inDir: string, outDir: string) {
    if (!fs.existsSync(outDir)) {
        fs.mkdirSync(outDir, { recursive: true});
    }

    const fileEntries: fs.Dirent[] = fs.readdirSync(inDir, { withFileTypes: true });

    for (let fileEntry of fileEntries) {
        const inPath = path.resolve(inDir, fileEntry.name);
        const outPath = path.resolve(outDir, fileEntry.name);

        if (fileEntry.isDirectory()) {
            fs.mkdirSync(outPath);
            copyDirContentsSync(inPath, outPath);
        } else if (fileEntry.isFile()) {
            fs.copyFileSync(inPath, outPath);
        }
    }
}

export async function getAndUpdateDockerProxy(): Promise<string> {
    // Ask whether to use a proxy for docker or not
    let dockerProxy: any = vscode.workspace.getConfiguration('iotea').get('project.docker.proxy');

    dockerProxy = await vscode.window.showInputBox({
        value: dockerProxy || '',
        prompt: 'Specify the http(s) proxy, to connect to the internet (e.g. http://host.docker.internal:3128). Leave it blank or just cancel this dialog for no proxy'
    });

    dockerProxy = (dockerProxy || '').trim();

    await vscode.workspace.getConfiguration('iotea').update('project.docker.proxy', dockerProxy, true);

    return dockerProxy;
}

export async function chooseAndUpdateIoTeaProjectDir(): Promise<string> {
    // Retrieve, Prompt and store the iotea project root folder
    let ioteaProjectRootDir = getIoTeaRootDir();

    if (ioteaProjectRootDir === '') {
        const ioteaProjectRootDirUris = await vscode.window.showOpenDialog({
            canSelectMany: false,
            canSelectFolders: true,
            canSelectFiles: false,
            openLabel: 'Select IoT Event Analytics project folder',
            title: 'Choose the IoT Event Analytics project folder'
        });

        if (ioteaProjectRootDirUris === undefined) {
            throw new Error('No IoT Event Analytics project folder selected');
        }

        ioteaProjectRootDir = ioteaProjectRootDirUris[0].fsPath;

        await ensureIoTeaProjectRootDirAt(ioteaProjectRootDir);

        // Update the workspace configuration according to the selected folder
        await setIoTeaRootDir(ioteaProjectRootDir);
    }

    return ioteaProjectRootDir;
}

export function getIoTeaRootDir(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('project.root.dir') as string).trim();
}

export function getDockerComposeCmd(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('terminal.docker-compose') as string).trim();
}

export function getDockerCmdArgs(args: string[]): any {
    // To take care about sudo docker....
    const dockerCmdParts = getDockerCmd().split(' ');

    const env: any = {};
    const dockerSock = getDockerSock();

    if (dockerSock !== '') {
        env.DOCKER_HOST = dockerSock;
    }

    return {
        cmd: dockerCmdParts.shift(),
        args: [ ...dockerCmdParts, ...args ],
        env
    };
}

export function executeDockerCmd(args: string[], cwd: string): Promise<string> {
    const t = new Terminal();
    const dockerCmdArgs = getDockerCmdArgs(args);
    return t.executeCommand(dockerCmdArgs.cmd, dockerCmdArgs.args, cwd, () => {}, dockerCmdArgs.env);
}

export function getDockerSock(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('terminal.docker-sock') as string).trim();
}

export function getPythonCmd(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('terminal.python') as string).trim();
}

export function getPipModule(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('terminal.pip') as string).trim();
}

export function updateJsonFileAt(absJsonPath: string, updatePaths: any): void {
    const json = JSON.parse(fs.readFileSync(absJsonPath, { encoding: 'utf8' }));
    updateJsonAt(json, updatePaths);
    fs.writeFileSync(absJsonPath, JSON.stringify(json, null, 4), { encoding: 'utf-8'});
}

export function showProgressWithRuntimePrecheck(title: string, onPrecheckSuccess: (p: vscode.Progress<{ message: string, increment?: number }>) => Promise<void>): Thenable<any> {
    return vscode.window.withProgress({ title, location: vscode.ProgressLocation.Notification }, async p => {
        await checkRuntimeRequirementsIfNeeded(
            (component: string, requirement: string) => {
                p.report({ message: `Checking ${requirement} requirement for component ${component}...`});
            },
            (component: string, requirement: string, expected: any, actual: any) => {
                p.report({ message: `Check done. Expected ${requirement} of ${component} to be ${expected}. Found ${actual}`});
            }
        )
            .then(() => onPrecheckSuccess(p));
    });
}

export function checkRuntimeRequirementsIfNeeded(onRequirement: (component: string, requirement: string) => void, onRequirementSuccess: (component: string, requirement: string, expected: any, actual: any) => void): Promise<void> {
    const requirements = vscode.workspace.getConfiguration('iotea').get<object>('platform.requirements');
    const storedHash = vscode.workspace.getConfiguration('iotea').get<string>('platform.requirements-check');

    const md5 = crypto.createHash('md5');
    md5.update(JSON.stringify(requirements));
    const hash = md5.digest('hex');

    if (storedHash === hash) {
        return Promise.resolve();
    }

    return ensureRuntimeRequirements(
        requirements,
        onRequirement,
        onRequirementSuccess
    )
        .then(() => {
            // Update check with the hash
            vscode.workspace.getConfiguration('iotea').update('platform.requirements-check', hash, true);
        });
}

function updateJsonAt(json: any, updatePaths: any) {
    for (let path in updatePaths) {
        let objLayer = json;

        path.split('.').forEach((pathPart: string, idx: number, pathParts: string[]) => {
            // Check if pathParts have a number attached to them to specify an array accessor
            const match: string[] | null = pathPart.match(/([^\[]+)(?:\[([0-9])+\])?/);

            if (match === null) {
                throw new Error(`Invalid path specified ${path}`);
            }

            pathPart = match[1];
            const arrayIndex = match[2];

            if (!Object.prototype.hasOwnProperty.call(objLayer, pathPart)) {
                if (arrayIndex === undefined) {
                    // Create new object
                    objLayer[pathPart] = {};
                } else {
                    // Create new array
                    objLayer[pathPart] = new Array().fill(undefined, 0, parseInt(arrayIndex, 10));
                }
            }

            if (arrayIndex !== undefined) {
                const parsedArrayIndex = parseInt(arrayIndex, 10);

                if (!Array.isArray(objLayer[pathPart])) {
                    throw new Error('An array should be accessed on a non-array value');
                }

                if (idx === pathParts.length - 1) {
                    objLayer[pathPart][parsedArrayIndex] = updatePaths[path];
                    return;
                }

                if (objLayer[pathPart][parsedArrayIndex] === undefined) {
                    objLayer[pathPart][parsedArrayIndex] = {};
                }

                objLayer = objLayer[pathPart][parsedArrayIndex];

                return;
            }

            if (idx === pathParts.length - 1) {
                objLayer[pathPart] = updatePaths[path];
                return;
            }

            objLayer = objLayer[pathPart];
        });
    }

    return json;
}

function setIoTeaRootDir(ioteaProjectRootDir: string): Thenable<void> {
    return vscode.workspace.getConfiguration('iotea').update('project.root.dir', ioteaProjectRootDir, true);
}

function getDockerCmd(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('terminal.docker') as string).trim();
}