/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';
import { Terminal } from './terminal';
import {
    chooseAndUpdateIoTeaProjectDir,
    getPythonCmd,
    getPipModule,
    showProgressWithRuntimePrecheck
} from './util';

export class VssUtils {
    private static YES_OPTION = 'yes';
    private static NO_OPTION = 'no';

    constructor(private ioteaProjectRootDir: string, private progress: vscode.Progress<{ message: string, increment: number }> | null = null) {}

    public static register(context: vscode.ExtensionContext) {
        context.subscriptions.push(vscode.commands.registerCommand('iotea.vss.createIoTeaTypesFromVssJson', () => {
            return showProgressWithRuntimePrecheck('Creating IoT Event Analytics types from Vehicle signal specification', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                return await new VssUtils(await chooseAndUpdateIoTeaProjectDir(), p).startCreateIoTeaTypesFromVssJsonFlow();
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand('iotea.vss.createKuksaValConfiguration', () => {
            return showProgressWithRuntimePrecheck('Creating Kuksa.VAL configuration', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                await new VssUtils(await chooseAndUpdateIoTeaProjectDir(), p).startCreateKuksaValConfigFlow();
            })
            .then(() => {}, err => {
                return vscode.window.showErrorMessage(err.message);
            });
        }));
    }

    public async createIoTeaTypesFromVssJson(ioteaProjectRootDir: string, absTypesJsonOutputPath: string, ioteaSegment: string, absTypesJsonInputPath: string | null, forceOverwriteSegment = false): Promise<void> {
        // Create VSS Json from scratch
        const absTempDir = fs.mkdtempSync('vss-json-');

        try {
            await this.createVssJsonAt(absTempDir, '', '');
            await this.createIoTeaTypesFromExistingVssJson(ioteaProjectRootDir, path.resolve(absTempDir, 'vss.json'), absTypesJsonOutputPath, ioteaSegment, absTypesJsonInputPath, forceOverwriteSegment);
        }
        finally {
            fs.rmdirSync(absTempDir, {
                recursive: true
            });
        }
    }

    public async createIoTeaTypesFromExistingVssJson(ioteaProjectRootDir: string, absVssJsonInputPath: string, absTypesJsonOutputPath: string, ioteaSegment: string, absTypesJsonInputPath: string | null = null, forceOverwriteSegment = false): Promise<void> {
        this.reportProgress('Converting Vehicle Signal Specification');

        const absVssConversionPath = path.resolve(ioteaProjectRootDir, 'src', 'tools', 'vss');

        const t = new Terminal();

        const args = [ 'vss2types.js', '-s', ioteaSegment, '-v', `"${absVssJsonInputPath}"`, '-u', 'vss.uom.json', '-o', `"${absTypesJsonOutputPath}"` ];

        if (forceOverwriteSegment) {
            args.push('-f', 'true');
        }

        if (absTypesJsonInputPath !== null) {
            args.push('-i', absTypesJsonInputPath);
        }

        return t.executeCommand('node', args, absVssConversionPath, (message: string) => {
            this.reportProgress(message);
        }).then(() => {});
    }

    public async createKuksaValConfigurationAt(absOutPath: string, userId: string, vehicleId: string, kuksaValToken = this.createKuksaValTokenObject()): Promise<void> {
        // Create VSS Json from scratch
        await this.createVssJsonAt(absOutPath, userId, vehicleId);

        let absKuksaValRepositoryPath: string | null = null;

        try {
            // Clone Kuksa.VAL repository
            absKuksaValRepositoryPath = await this.cloneKuksaValRepository();

            // Create certs folder
            const absCertificatePath = path.resolve(absOutPath, 'certs');
            fs.mkdirSync(absCertificatePath);

            // Copy certificates
            fs.copyFileSync(path.resolve(absKuksaValRepositoryPath, 'kuksa_certificates', 'Server.key'), path.resolve(absCertificatePath, 'Server.key'));
            fs.copyFileSync(path.resolve(absKuksaValRepositoryPath, 'kuksa_certificates', 'Server.pem'), path.resolve(absCertificatePath, 'Server.pem'));
            fs.copyFileSync(path.resolve(absKuksaValRepositoryPath, 'kuksa_certificates', 'jwt', 'jwt.key.pub'), path.resolve(absCertificatePath, 'jwt.key.pub'));

            // Create a new JWT token
            const absKuksaValJwtPath = await this.createKuksaValJwt(absKuksaValRepositoryPath, kuksaValToken);

            // Copy the newly created token
            fs.copyFileSync(absKuksaValJwtPath, path.resolve(absOutPath, 'jwt.token'));
        }
        finally {
            if (absKuksaValRepositoryPath === null) {
                return;
            }

            // Remove the temporary repository clone
            fs.rmdirSync(absKuksaValRepositoryPath, {
                recursive: true
            });
        }
    }

    // Valid for one year
    public createKuksaValTokenObject(expiresInS = 31622400, pathPermissions = { '*': 'rw' }, isAdmin = true, modifyTree = true) {
        return {
            sub: 'kuksa.val',
            iss: 'Eclipse KUKSA Dev',
            admin: isAdmin,
            modifyTree: modifyTree,
            iat: 1516239022,
            exp: Math.floor(Date.now() / 1000) + expiresInS,
            'kuksa-vss':  pathPermissions
        };
    }

    private reportProgress(message: string, increment: number = 0) {
        if (this.progress === null) {
            console.log(message);
            return;
        }

        this.progress.report({ message, increment });
    }

    private async startCreateKuksaValConfigFlow(): Promise<void> {
        return vscode.window.showOpenDialog({
            canSelectFolders: true,
            canSelectFiles: false,
            canSelectMany: false,
            openLabel: 'Select output directory',
            title: 'Select the output directory for the Kuksa.VAL configuration'
        })
            .then((kuksaConfigOutputFolders: vscode.Uri[] | undefined) => {
                if (kuksaConfigOutputFolders === undefined || kuksaConfigOutputFolders.length === 0) {
                    throw new Error('Command was cancelled by user');
                }

                return kuksaConfigOutputFolders[0].fsPath;
            })
            .then(async (kuksaConfigOutputPath: string) => {
                const userId: string | undefined = await vscode.window.showInputBox({
                    prompt: 'Specify a user ID to identify the driver',
                    value: 'driver',
                    validateInput: (input: string) => input.trim() !== '' ? null : 'The user ID needs to be at least 1 character long'
                });

                if (userId === undefined) {
                    throw new Error('Command was cancelled by user');
                }

                const vehicleId: string | undefined = await vscode.window.showInputBox({
                    prompt: 'Specify a vehicle ID to identify the vehicle e.g. VIN',
                    value: 'vin',
                    validateInput: (input: string) => input.trim() !== '' ? null : 'The vehicle ID needs to be at least 1 character long'
                });

                if (vehicleId === undefined) {
                    throw new Error('Command was cancelled by user');
                }

                return this.createKuksaValConfigurationAt(kuksaConfigOutputPath, userId, vehicleId);
            })
            .then(() => {}, err => {
                return vscode.window.showErrorMessage(err.message);
            });
    }

    private async startCreateIoTeaTypesFromVssJsonFlow(): Promise<void> {
        return vscode.window.showInformationMessage('Do you want to add the Vehicle Signal Specification to an existing IoT Event Analytics types configuration file?', VssUtils.YES_OPTION, VssUtils.NO_OPTION)
            .then(async (shouldUseExistingTypesJsonAsInput: string | undefined) => {
                if (shouldUseExistingTypesJsonAsInput === undefined) {
                    // Dialog was closed
                    throw new Error('Command was cancelled by user');
                }

                let absTypesJsonInputPath: string | null = null;

                if (shouldUseExistingTypesJsonAsInput === VssUtils.YES_OPTION) {
                    absTypesJsonInputPath = await vscode.window.showOpenDialog({
                        canSelectFolders: false,
                        canSelectFiles: true,
                        canSelectMany: false,
                        openLabel: 'Select types.json',
                        filters: {
                            'IoT Event Analytics types configuration': [ 'json' ]
                        },
                        title: 'Select an IoT Event Analytics types configuration file'
                    })
                        .then((selectedFiles: vscode.Uri[] | undefined) => {
                            if (selectedFiles === undefined || selectedFiles.length === 0) {
                                throw new Error('No IoT Event Analytics types configuration file selected');
                            }

                            return selectedFiles[0].fsPath;
                        });
                }

                const absTypesJsonOutputPath = await vscode.window.showOpenDialog({
                    canSelectFolders: true,
                    canSelectFiles: false,
                    canSelectMany: false,
                    openLabel: 'Select output directory for types.json',
                    title: 'Select the output directory for the IoT Event Analytics types configuration file',
                    defaultUri: absTypesJsonInputPath !== null ? vscode.Uri.file(path.dirname(absTypesJsonInputPath)) : undefined
                })
                    .then((selectedFolders: vscode.Uri[] | undefined) => {
                        if (selectedFolders === undefined || selectedFolders.length === 0) {
                            throw new Error('No output folder for the IoT Event Analytics types configuration file selected');
                        }

                        return path.resolve(selectedFolders[0].fsPath, 'types.json');
                    });

                return [ absTypesJsonInputPath, absTypesJsonOutputPath ];
            })
            .then(async (absTypesJsonIOPaths: (string | null)[]) => {
                let ioteaSegment: string = '280101';
                let overwriteExistingSegment = false;

                while(true) {
                    let userIoteaSegment = await vscode.window.showInputBox({
                        prompt: 'Specify the IoTea segment (eCl@ss) to which the Vehicle Signal Specification belongs e.g. 280101',
                        value: ioteaSegment,
                        validateInput: (input: string) => {
                            if (/^[0-9]{6}$/.test(input)) {
                                return null;
                            }

                            return 'Value has to be 6 digits long e.g. 280101';
                        }
                    });

                    if (userIoteaSegment === undefined) {
                        throw new Error('Command was cancelled by user');
                    }

                    ioteaSegment = userIoteaSegment;

                    if (absTypesJsonIOPaths[0] === absTypesJsonIOPaths[1]) {
                        // Check if segment is already present in input types configuration
                        const typesJson = JSON.parse(fs.readFileSync(absTypesJsonIOPaths[0] as string).toString('utf-8'));

                        if (Object.prototype.hasOwnProperty.call(typesJson, ioteaSegment)) {
                            const answer: string | undefined = await vscode.window.showInformationMessage(`Do you want overwrite the existing segment ${ioteaSegment} in ${absTypesJsonIOPaths[1]}?`, VssUtils.YES_OPTION, VssUtils.NO_OPTION);

                            if (answer === undefined) {
                                throw new Error('Command was cancelled by user');
                            }

                            if (answer === VssUtils.NO_OPTION) {
                                // Restart segment input
                                continue;
                            }

                            overwriteExistingSegment = true;
                        }
                    }

                    break;
                }

                return vscode.window.showInformationMessage('Do you want to use an existing Vehicle Signal Specification?', VssUtils.YES_OPTION, VssUtils.NO_OPTION)
                    .then(async (shouldUseExistingVssDocument: string | undefined) => {
                        if (shouldUseExistingVssDocument === undefined) {
                            throw new Error('Command was cancelled by user');
                        }

                        if (shouldUseExistingVssDocument === VssUtils.NO_OPTION) {
                            return this.createIoTeaTypesFromVssJson(this.ioteaProjectRootDir, absTypesJsonIOPaths[1] as string, ioteaSegment, absTypesJsonIOPaths[0], overwriteExistingSegment);
                        }

                        const vssDocuments: vscode.Uri[] | undefined = await vscode.window.showOpenDialog({
                            canSelectFolders: false,
                            canSelectFiles: true,
                            canSelectMany: false,
                            openLabel: 'Select vss.json',
                            filters: {
                                'Vehicle Signal Specification': [ 'json' ]
                            },
                            title: 'Select a Vehicle Signal Specification file'
                        });

                        if (vssDocuments === undefined || vssDocuments.length === 0) {
                            throw new Error('Command was cancelled by user');
                        }

                        return this.createIoTeaTypesFromExistingVssJson(this.ioteaProjectRootDir, vssDocuments[0].fsPath, absTypesJsonIOPaths[1] as string, ioteaSegment, absTypesJsonIOPaths[0], overwriteExistingSegment);
                    });
            });
    }

    private async createVssJsonAt(absOutPath: string, userId: string, vehicleId: string, vssJsonFilename: string = 'vss.json'): Promise<void> {
        let absVssRepositoryPath: string | null = null;

        try {
            // Clone VSS repository
            absVssRepositoryPath = await this.cloneVssRepository();
            // Create VSS model as Json
            const absOriginalVssJsonPath: string = await this.createVssJson(absVssRepositoryPath);
            // Add userId and vehicleId as default values
            const json = this.getModifiedVssJson(absOriginalVssJsonPath, userId, vehicleId);
            // Write vss.json
            fs.writeFileSync(path.resolve(absOutPath, vssJsonFilename), JSON.stringify(json, null, 4));
        }
        finally {
            if (absVssRepositoryPath === null) {
                return;
            }

            fs.rmdirSync(absVssRepositoryPath, {
                recursive: true
            });
        }
    }

    private async createKuksaValJwt(absKuksaValRepositoryPath: string, token?: Object): Promise<string> {
        this.reportProgress('Creating Kuksa.Val JWT from source')

        const t = new Terminal();

        const absJwtPath = path.resolve(absKuksaValRepositoryPath, 'kuksa_certificates', 'jwt');

        let jwtInputFile = 'super-admin.json';

        if (token) {
            // Write the provided token
            fs.writeFileSync(path.resolve(absJwtPath, 'new.token.json'), JSON.stringify(token, null, 2));
            jwtInputFile = 'new.token.json';
        }

        // Install all python dependencies
        await t.executeCommand(getPythonCmd(), [ '-m', getPipModule(), 'install ', '-r', 'requirements.txt', '--user' ], absJwtPath, (message: string) => {
            this.reportProgress(message);
        });

        // Create JWT token
        await t.executeCommand(getPythonCmd(), [ 'createToken.py', jwtInputFile ], absJwtPath, (message: string) => {
            this.reportProgress(message);
        });

        return path.resolve(absJwtPath, `${jwtInputFile}.token`);
    }

    private async cloneKuksaValRepository(): Promise<string> {
        return this.cloneGitRepository('https://github.com/eclipse/kuksa.val.git', 'kuksa.val-');
    }

    private getModifiedVssJson(absOriginalVssJsonPath: string, userId: string, vehicleId: string): Object {
        const json = JSON.parse(fs.readFileSync(absOriginalVssJsonPath).toString('utf-8'));

        json.Vehicle.children.Driver.children.Identifier.children.Subject.value = userId;
        json.Vehicle.children.VehicleIdentification.children.VIN.value = vehicleId;

        return json;
    }

    private async createVssJson(absVssRepositoryPath: string): Promise<string> {
        this.reportProgress('Creating Vehicle Signal Specification from source');

        const t = new Terminal();

        const absVssToolsPath = path.resolve(absVssRepositoryPath, 'vss-tools');

        // Install all python dependencies
        await t.executeCommand(getPythonCmd(), [ '-m', getPipModule(), 'install ', '-r', 'requirements.txt', '--user' ], absVssToolsPath, (message: string) => {
            this.reportProgress(message);
        });

        await t.executeCommand(getPythonCmd(), [ 'vspec2json.py', '-i', `Vehicle:vehicle.uuid`, '../spec/VehicleSignalSpecification.vspec', 'vss.json' ], absVssToolsPath, (message: string) => {
            this.reportProgress(message);
        });

        return path.resolve(absVssToolsPath, 'vss.json');
    }

    private async cloneVssRepository(): Promise<string> {
        return this.cloneGitRepository('https://github.com/GENIVI/vehicle_signal_specification.git', 'vss-', [ '--recurse-submodules' ]);
    }

    private async cloneGitRepository(repositoryUrl: string, tmpDirPrefix: string, cloneArgs: string[] = []): Promise<string> {
        this.reportProgress(`Cloning ${repositoryUrl}`);

        // Create a temporary directory and clone the current vehicle_signal_specification repository
        const tmpDir = fs.mkdtempSync(tmpDirPrefix);

        const t = new Terminal();

        // Clone the repository including all submodules
        await t.executeCommand('git', [ 'clone', ...cloneArgs, repositoryUrl, '.' ], tmpDir, (message: string) => {
            this.reportProgress(message);
        });

        return tmpDir;
    }
}
