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
import * as vscode from 'vscode';

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
        prompt: 'Specify the http(s) proxy, which is used within your Docker environment to connect to the internet. Leave it blank or just cancel this dialog for no proxy'
    });

    dockerProxy = (dockerProxy || '').trim();

    await vscode.workspace.getConfiguration('iotea').update('project.docker.proxy', dockerProxy);

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
            title: 'Choose the IoT Event Analytics project folder'
        });

        if (ioteaProjectRootDirUris === undefined) {
            throw new Error('No IoT Event Analytics project folder selected');
        }

        ioteaProjectRootDir = ioteaProjectRootDirUris[0].fsPath;

        // Update the workspace configuration according to the selected folder
        await setIoTeaRootDir(ioteaProjectRootDir);
    }

    return ioteaProjectRootDir;
}

export function getIoTeaRootDir(): string {
    return (vscode.workspace.getConfiguration('iotea').get<string>('project.root.dir') as string).trim();
}

function setIoTeaRootDir(ioteaProjectRootDir: string): Thenable<void> {
    return vscode.workspace.getConfiguration('iotea').update('project.root.dir', ioteaProjectRootDir);
}