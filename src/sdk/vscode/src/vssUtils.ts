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
import { chooseAndUpdateIoTeaProjectDir } from './util';

const CREATE_IOTEA_TYPES_FROM_VSS_JSON_COMMAND = 'iotea.createIoTeaTypesFromVssJson';
const CREATE_KUKSA_VAL_CONFIG_COMMAND = 'iotea.createKuksaValConfiguration';

export class VssUtils {
    static register(context: vscode.ExtensionContext) {
        context.subscriptions.push(vscode.commands.registerCommand(CREATE_IOTEA_TYPES_FROM_VSS_JSON_COMMAND, async () => {
            const ioteaProjectRootDir = await chooseAndUpdateIoTeaProjectDir();

            await vscode.window.showInformationMessage('Do you want to add the Vehicle Signal Specification to an existing IoT Event Analytics types configuration file?', 'yes', 'no')
                .then(async (shouldUseExistingTypesJson: string | undefined) => {
                    if (shouldUseExistingTypesJson === undefined) {
                        // Dialog was closed
                        throw new Error('Command was cancelled by user');
                    }

                    if (shouldUseExistingTypesJson === 'no') {
                        return vscode.window.showOpenDialog({
                            canSelectFolders: true,
                            canSelectFiles: false,
                            canSelectMany: false,
                            title: 'Select the output directory for the IoT Event Analytics types configuration file'
                        })
                            .then((selectedFolders: vscode.Uri[] | undefined) => {
                                if (selectedFolders === undefined || selectedFolders.length === 0) {
                                    throw new Error('No output folder for the IoT Event Analytics types configuration file selected');
                                }

                                return path.resolve(selectedFolders[0].fsPath, 'types.json');
                            });
                    } else {
                        return vscode.window.showOpenDialog({
                            canSelectFolders: false,
                            canSelectFiles: true,
                            canSelectMany: false,
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
                })
                .then(async (absTypesJsonOutputPath: string) => {
                    const ioteaSegment: string | undefined = await vscode.window.showInputBox({
                        prompt: 'Specify the IoTea segment (eCl@ss) to which the Vehicle Signal Specification belongs e.g. 280101',
                        value: '280101',
                        validateInput: (input: string) => {
                            if (/^[0-9]{6}$/.test(input)) {
                                return null;
                            }

                            return 'Value has to be 6 digits long e.g. 280101';
                        }
                    });

                    if (ioteaSegment === undefined) {
                        throw new Error('Command was cancelled by user');
                    }

                    return vscode.window.showInformationMessage('Do you want to use an existing Vehicle Signal Specification?', 'yes', 'no')
                        .then(async (shouldUseExistingVssDocument: string | undefined) => {
                            if (shouldUseExistingVssDocument === undefined) {
                                throw new Error('Command was cancelled by user');
                            }

                            if (shouldUseExistingVssDocument === 'no') {
                                return VssUtils.createIoTeaTypesFromVssJson(ioteaProjectRootDir, absTypesJsonOutputPath, ioteaSegment, true);
                            }

                            const vssDocuments: vscode.Uri[] | undefined = await vscode.window.showOpenDialog({
                                canSelectFolders: false,
                                canSelectFiles: true,
                                canSelectMany: false,
                                filters: {
                                    'Vehicle Signal Specification': [ 'json' ]
                                },
                                title: 'Select a Vehicle Signal Specification file'
                            });

                            if (vssDocuments === undefined || vssDocuments.length === 0) {
                                throw new Error('Command was cancelled by user');
                            }

                            return VssUtils.createIoTeaTypesFromExistingVssJson(ioteaProjectRootDir, vssDocuments[0].fsPath, absTypesJsonOutputPath, ioteaSegment, true);
                        });
                })
                .then(() => {}, err => {
                    return vscode.window.showErrorMessage(err.message);
                });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(CREATE_KUKSA_VAL_CONFIG_COMMAND, async () => {
            return vscode.window.showOpenDialog({
                canSelectFolders: true,
                canSelectFiles: false,
                canSelectMany: false,
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

                    return VssUtils.createKuksaValConfigurationAt(kuksaConfigOutputPath, userId, vehicleId);
                })
                .then(() => {}, err => {
                    return vscode.window.showErrorMessage(err.message);
                });
        }));
    }

    static async createIoTeaTypesFromVssJson(ioteaProjectRootDir: string, absTypesJsonOutputPath: string, ioteaSegment: string, forceOverwriteSegment = false): Promise<void> {
        // Create VSS Json from scratch
        const absTempDir = fs.mkdtempSync('vss-json-');

        await VssUtils.createVssJsonAt(absTempDir, '', '');
        await VssUtils.createIoTeaTypesFromExistingVssJson(ioteaProjectRootDir, path.resolve(absTempDir, 'vss.json'), absTypesJsonOutputPath, ioteaSegment, forceOverwriteSegment);

        fs.rmdirSync(absTempDir, {
            recursive: true
        });
    }

    static async createIoTeaTypesFromExistingVssJson(ioteaProjectRootDir: string, absVssJsonInputPath: string, absTypesJsonOutputPath: string, ioteaSegment: string, forceOverwriteSegment = false): Promise<void> {
        await vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: 'Converting Vehicle Signal Specification...'}, async p => {
            const absVssConversionPath = path.resolve(ioteaProjectRootDir, 'src', 'tools', 'vss');

            const t = new Terminal();

            const args = [ 'vss2types.js', '-s', ioteaSegment, '-v', absVssJsonInputPath, '-u', 'vss.uom.json', '-o', absTypesJsonOutputPath];

            if (forceOverwriteSegment) {
                args.push('-f', 'true');
            }

            await t.executeCommand('node', args, absVssConversionPath);
        });
    }

    static async createKuksaValConfigurationAt(absOutPath: string, userId: string, vehicleId: string, kuksaValToken = VssUtils.createKuksaValTokenObject()) {
        // Create VSS Json from scratch
        await VssUtils.createVssJsonAt(absOutPath, userId, vehicleId);
        // Clone Kuksa.VAL repository
        const absKuksaValRepositoryPath: string = await VssUtils.cloneKuksaValRepository();
        // Create certs folder
        const absCertificatePath = path.resolve(absOutPath, 'certs');
        fs.mkdirSync(absCertificatePath);
        // Copy certificates
        fs.copyFileSync(path.resolve(absKuksaValRepositoryPath, 'certificates', 'Server.key'), path.resolve(absCertificatePath, 'Server.key'));
        fs.copyFileSync(path.resolve(absKuksaValRepositoryPath, 'certificates', 'Server.pem'), path.resolve(absCertificatePath, 'Server.pem'));
        fs.copyFileSync(path.resolve(absKuksaValRepositoryPath, 'certificates', 'jwt', 'jwt.key.pub'), path.resolve(absCertificatePath, 'jwt.key.pub'));
        // Create a new JWT token
        const absKuksaValJwtPath = await VssUtils.createKuksaValJwt(absKuksaValRepositoryPath, kuksaValToken);
        // Copy the newly created token
        fs.copyFileSync(absKuksaValJwtPath, path.resolve(absOutPath, 'jwt.token'));
        // Remove the temporary repository clone
        return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: `Cleaning up...`}, async p => {
            fs.rmdirSync(absKuksaValRepositoryPath, {
                recursive: true
            });
        });
    }

    // Valid for one year
    static createKuksaValTokenObject(expiresInS = 31622400, pathPermissions = { '*': 'rw' }, isAdmin = true, modifyTree = true) {
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

    private static async createVssJsonAt(absOutPath: string, userId: string, vehicleId: string, vssJsonFilename: string = 'vss.json'): Promise<void> {
        // Clone VSS repository
        console.log('1');
        const absVssRepositoryPath: string = await VssUtils.cloneVssRepository();
        console.log('2');
        // Create VSS model as Json
        const absOriginalVssJsonPath: string = await VssUtils.createVssJson(absVssRepositoryPath);
        console.log('3');
        // Add userId and vehicleId as default values
        const json = VssUtils.getModifiedVssJson(absOriginalVssJsonPath, userId, vehicleId);
        console.log('4');
        // Write vss.json
        fs.writeFileSync(path.resolve(absOutPath, vssJsonFilename), JSON.stringify(json, null, 4));
        console.log('5');
        // Remove the temporary repository clone
        return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: `Cleaning up...`}, async p => {
            fs.rmdirSync(absVssRepositoryPath, {
                recursive: true
            });
        });
    }

    private static async createKuksaValJwt(absKuksaValRepositoryPath: string, token?: Object): Promise<string> {
        return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: `Creating Kuksa.Val JWT from source...`}, async p => {
            const t = new Terminal();

            const absJwtPath = path.resolve(absKuksaValRepositoryPath, 'certificates', 'jwt');

            let jwtInputFile = 'super-admin.json';

            if (token) {
                // Write the provided token
                fs.writeFileSync(path.resolve(absJwtPath, 'new.token.json'), JSON.stringify(token, null, 2));
                jwtInputFile = 'new.token.json';
            }

            // Install all python dependencies
            await t.executeCommand('python', [ '-m', 'pip', 'install ', '-r', 'requirements.txt', '--user' ], absJwtPath, (message: string) => {
                p.report({ message });
            });

            // Create JWT token
            await t.executeCommand('python', [ 'createToken.py', jwtInputFile ], absJwtPath, (message: string) => {
                p.report({ message });
            });

            return path.resolve(absJwtPath, `${jwtInputFile}.token`);
        });
    }

    private static async cloneKuksaValRepository(): Promise<string> {
        return VssUtils.cloneGitRepository('https://github.com/eclipse/kuksa.val.git', 'kuksa.val-');
    }

    private static getModifiedVssJson(absOriginalVssJsonPath: string, userId: string, vehicleId: string): Object {
        const json = JSON.parse(fs.readFileSync(absOriginalVssJsonPath).toString('utf-8'));

        json.Vehicle.children.Driver.children.Identifier.children.Subject.value = userId;
        json.Vehicle.children.VehicleIdentification.children.VIN.value = vehicleId;

        return json;
    }

    private static async createVssJson(absVssRepositoryPath: string): Promise<string> {
        return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: `Creating Vehicle Signal Specification from source...`}, async p => {
            const t = new Terminal();

            const absVssToolsPath = path.resolve(absVssRepositoryPath, 'vss-tools');

            // Install all python dependencies
            await t.executeCommand('python', [ '-m', 'pip', 'install ', '-r', 'requirements.txt', '--user' ], absVssToolsPath, (message: string) => {
                p.report({ message });
            });

            await t.executeCommand('python', [ 'vspec2json.py', '-i', `Vehicle:vehicle.uuid`, '../spec/VehicleSignalSpecification.vspec', 'vss.json' ], absVssToolsPath, (message: string) => {
                p.report({ message });
            });

            return path.resolve(absVssToolsPath, 'vss.json');
        });
    }

    private static async cloneVssRepository(): Promise<string> {
        return VssUtils.cloneGitRepository('https://github.com/GENIVI/vehicle_signal_specification.git', 'vss-', [ '--recurse-submodules' ]);
    }

    private static async cloneGitRepository(repositoryUrl: string, tmpDirPrefix: string, cloneArgs: string[] = []): Promise<string> {
        return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: `Cloning ${repositoryUrl}...`}, async p => {
            // Create a temporary directory and clone the current vehicle_signal_specification repository
            const tmpDir = fs.mkdtempSync(tmpDirPrefix);

            const t = new Terminal();

            // Clone the repository including all submodules
            await t.executeCommand('git', [ 'clone', ...cloneArgs, repositoryUrl, '.' ], tmpDir, (message: string) => {
                p.report({ message });
            });

            return tmpDir;
        });
    }
}
