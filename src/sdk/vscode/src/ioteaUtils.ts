/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';
import * as os from 'os';

import { Terminal } from './terminal';

import {
    copyDirContentsSync,
    chooseAndUpdateIoTeaProjectDir,
    getIoTeaRootDir,
    getAndUpdateDockerProxy
} from './util';

import { MqttWebView } from './mqttWebView';

const START_MOSQUITTO_BROKER_COMMAND = 'iotea.startMosquittoBroker';
const STOP_MOSQUITTO_BROKER_COMMAND = 'iotea.stopMosquittoBroker';
const PUBLISH_MQTT_MESSAGE = 'iotea.publishMqttMessage';
const MOSQUITTO_BROKER_TERMINAL_NAME = 'IoT Event Analytics Mosquitto Broker';

const START_IOTEA_PLATFORM_COMMAND = 'iotea.startIoTeaPlatform';
const STOP_IOTEA_PLATFORM_COMMAND = 'iotea.stopIoTeaPlatform';
const CREATE_IOTEA_JS_TALENT_CLASS_COMMAND = 'iotea.createJSTalentClass';
const CREATE_IOTEA_JS_TALENT_PROJECT_COMMAND = 'iotea.createJsTalentProject';
const IOTEA_PLATFORM_TERMINAL_NAME = 'IoT Event Analytics Platform';

export class IoTeaUtils {
    static register(context: vscode.ExtensionContext) {
        vscode.window.onDidCloseTerminal(async (terminal: vscode.Terminal) => {
            if (terminal.name === IOTEA_PLATFORM_TERMINAL_NAME) {
                await vscode.commands.executeCommand(STOP_IOTEA_PLATFORM_COMMAND, terminal);
            }

            if (terminal.name === MOSQUITTO_BROKER_TERMINAL_NAME) {
                await vscode.commands.executeCommand(STOP_MOSQUITTO_BROKER_COMMAND, terminal);
            }
        });

        context.subscriptions.push(vscode.commands.registerCommand(PUBLISH_MQTT_MESSAGE, async () => {
            MqttWebView.loadOnce(context.extensionPath, await chooseAndUpdateIoTeaProjectDir());
        }));

        context.subscriptions.push(vscode.commands.registerCommand(START_IOTEA_PLATFORM_COMMAND, async (envFile: string | undefined) => {
            await IoTeaUtils.startIoTeaPlatform(envFile);6
        }));

        context.subscriptions.push(vscode.commands.registerCommand(STOP_IOTEA_PLATFORM_COMMAND, async (terminal: vscode.Terminal | undefined) => {
            await IoTeaUtils.stopIoTeaPlatform(terminal);
        }));

        context.subscriptions.push(vscode.commands.registerCommand(START_MOSQUITTO_BROKER_COMMAND, async (envFile: string | undefined) => {
            await IoTeaUtils.startIoTeaMqttBroker(envFile);
        }));

        context.subscriptions.push(vscode.commands.registerCommand(STOP_MOSQUITTO_BROKER_COMMAND, async (terminal: vscode.Terminal | undefined) => {
            await IoTeaUtils.stopIoTeaMqttBroker(terminal);
        }));

        context.subscriptions.push(vscode.commands.registerCommand(CREATE_IOTEA_JS_TALENT_PROJECT_COMMAND, async () => {
            await chooseAndUpdateIoTeaProjectDir()
                .then(() => vscode.window.showOpenDialog({
                    canSelectMany: false,
                    canSelectFolders: true,
                    canSelectFiles: false,
                    title: 'Choose an empty folder for your new IoT Event Analytics Talent project'
                }))
                .then(async (uris: vscode.Uri[] | undefined) => {
                    // Get and verify the Talent Project root directory
                    if (uris === undefined) {
                        throw new Error('No talent project folder selected');
                    }

                    const talentProjectRootDir = uris[0].fsPath;

                    // Check if this folder is actually empty
                    const talentProjectRootDirContents: string[] = fs.readdirSync(talentProjectRootDir);

                    if (talentProjectRootDirContents.length > 0) {
                        // Non-empty directory detected
                        throw new Error('Talent project directory must be empty');
                    }

                    return talentProjectRootDir;
                })
                .then(async (talentProjectDir: string) => {
                    // Get, Prompt for the IoTea Event Analytics library to use
                    // Later on, get the list by retrieving the available versions in npm registry
                    const ioteaProjectRootDir: any = getIoTeaRootDir();

                    // List all available libraries
                    const availablePackages = fs.readdirSync(path.resolve(ioteaProjectRootDir, 'src/sdk/javascript/lib'));

                    const selectedPackage: string | undefined = await vscode.window.showQuickPick(availablePackages, {
                        canPickMany: false
                    });

                    if (selectedPackage === undefined) {
                        throw new Error(`You must select an Iot Event Analytics package`);
                    }

                    return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: 'Installing SDK...'}, async p => {
                        const localSdkPath = path.resolve(talentProjectDir, selectedPackage);

                        fs.copyFileSync(path.resolve(ioteaProjectRootDir, `src/sdk/javascript/lib/${selectedPackage}`), localSdkPath);

                        const terminal = new Terminal();

                        // Init node project
                        await terminal.executeCommand('npm', ['init', '-y'], talentProjectDir);

                        // Install SDK
                        await terminal.executeCommand('yarn', ['add', `file:${selectedPackage}`], talentProjectDir, msg => {
                            p.report({ message: msg.trim() });
                        });

                        // Remove SDK installation files
                        fs.unlinkSync(localSdkPath);

                        return talentProjectDir;
                    });
                })
                .then(async (talentProjectDir: string) => {
                    // Prepare the directory to have all configuration files, which are needed to start the IoTea platform

                    const ioteaProjectRootDir: any = getIoTeaRootDir();

                    // Copy the configuration for the mosquitto brokers in the project directory
                    const mosquittoConfigDir = path.resolve(talentProjectDir, 'config/mosquitto');
                    copyDirContentsSync(path.resolve(ioteaProjectRootDir, 'docker-compose/mosquitto'), mosquittoConfigDir)

                    // Copy the configurations for the platform into the project directory
                    const platformConfigDir = path.resolve(talentProjectDir, 'config/platform');
                    copyDirContentsSync(path.resolve(ioteaProjectRootDir, 'docker-compose/platform'), platformConfigDir)

                    // Copy the demo talent into this directory
                    fs.copyFileSync(path.resolve(__dirname, '../resources/talent.demo.js'), path.resolve(talentProjectDir, 'index.js'));

                    // Create the .env file used by docker-compose

                    // Get Docker Proxy
                    let dockerProxy: any = await getAndUpdateDockerProxy();

                    let envFileContents = '';

                    if (dockerProxy !== '') {
                        // Proxy was set
                        envFileContents += `DOCKER_HTTP_PROXY=${dockerProxy}${os.EOL}DOCKER_HTTPS_PROXY=${dockerProxy}${os.EOL}`;
                    }

                    envFileContents += `MOSQUITTO_CONFIG_DIR=${mosquittoConfigDir}${os.EOL}`;
                    envFileContents += `PLATFORM_CONFIG_DIR=${platformConfigDir}`;

                    fs.writeFileSync(path.resolve(talentProjectDir, '.env'), envFileContents, {
                        encoding: 'utf8'
                    });

                    return talentProjectDir;
                })
                .then(async (talentProjectDir: string) => {
                    // Start the platform in a new terminal using another command
                    if (await vscode.window.showInformationMessage('Do you want to start the IoTea Platform?', 'yes', 'no') === 'yes') {
                        return vscode.commands.executeCommand(START_IOTEA_PLATFORM_COMMAND, path.resolve(talentProjectDir, '.env'));
                    }
                })
                .then(() => {}, async err => {
                    return vscode.window.showErrorMessage(err.message);
                });
        }));

        context.subscriptions.push(
            vscode.commands.registerCommand(CREATE_IOTEA_JS_TALENT_CLASS_COMMAND, async () => {
                const uri = vscode.Uri.parse('untitled:talent.js');

                const template = fs.readFileSync(path.resolve(__dirname, '../resources/talent.template.js'), { encoding: 'utf8' });

                const edit = new vscode.WorkspaceEdit();
                edit.insert(uri, new vscode.Position(0, 0), template);
                await vscode.workspace.applyEdit(edit);

                await vscode.window.showTextDocument(uri);
            })
        );
    }

    static startIoTeaPlatform(envFile: string | undefined): Promise<void> {
        return IoTeaUtils.startComposeInTerminal(IOTEA_PLATFORM_TERMINAL_NAME, 'docker-compose.platform.yml', envFile);
    }

    static stopIoTeaPlatform(terminal: vscode.Terminal | undefined): Promise<void> {
        return IoTeaUtils.stopComposeInTerminal(IOTEA_PLATFORM_TERMINAL_NAME, 'docker-compose.platform.yml', terminal);
    }

    static startIoTeaMqttBroker(envFile: string | undefined): Promise<void> {
        return IoTeaUtils.startComposeInTerminal(MOSQUITTO_BROKER_TERMINAL_NAME, 'docker-compose.mosquitto.yml', envFile);
    }

    static stopIoTeaMqttBroker(terminal: vscode.Terminal | undefined): Promise<void> {
        return IoTeaUtils.stopComposeInTerminal(MOSQUITTO_BROKER_TERMINAL_NAME, 'docker-compose.mosquitto.yml', terminal);
    }

    private static async startComposeInTerminal(terminalName: string, composeFile: string, envFile: string | undefined) {
        const ioteaProjectRootDir: any = getIoTeaRootDir();

        if (envFile === undefined) {
            try {
                envFile = await IoTeaUtils.chooseComposeEnvFile();
            }
            catch(err) {
                vscode.window.showErrorMessage(err.message);
                return;
            }
        }

        const terminal = vscode.window.createTerminal({
            cwd: path.resolve(ioteaProjectRootDir, 'docker-compose'),
            name: terminalName,
            env: {
                _ENV_FILE: envFile as string
            }
        });

        terminal.sendText(`docker-compose -f ${composeFile} --project-name vscode-ext --env-file=${envFile} up --build --remove-orphans`);

        terminal.show(false);
    }

    private static async stopComposeInTerminal(terminalName: string, composeFile: string, terminal: vscode.Terminal | undefined) {
        const terminals = [];

        if (terminal !== undefined) {
            terminals.push(terminal);
        } else {
            for (let terminal of vscode.window.terminals) {
                if (terminal.name === terminalName) {
                    terminals.push(terminal);
                }
            }
        }

        if (terminals.length === 0) {
            // Ask for the used .env-file, if no terminal is given
            try {
                await IoTeaUtils.stopDockerCompose(terminalName, composeFile, await IoTeaUtils.chooseComposeEnvFile());
            }
            catch(err) {
                vscode.window.showErrorMessage(err.message);
            }

            return;
        }

        for (let terminal of terminals) {
            const envFile = ((terminal.creationOptions as vscode.TerminalOptions).env as any)._ENV_FILE;
            await IoTeaUtils.stopDockerCompose(terminalName, composeFile, envFile);
            terminal.dispose();
        }
    }

    private static stopDockerCompose(terminalName: string, composeFile: string, envFile: string) {
        const ioteaProjectRootDir: any = getIoTeaRootDir();

        const terminal = new Terminal();

        return vscode.window.withProgress({ location: vscode.ProgressLocation.Window, title: `Stopping ${terminalName}...`}, async p => {
            await terminal.executeCommand(
                'docker-compose',
                ['-f', composeFile, '--project-name', 'vscode-ext', '--env-file', envFile, 'down'],
                path.resolve(ioteaProjectRootDir,'docker-compose'),
                message => { p.report({ message }); }
            );
        });
    }

    private static async chooseComposeEnvFile(): Promise<string> {
        const selectedEnvFile: vscode.Uri[] | undefined = await vscode.window.showOpenDialog({
            canSelectFiles: true,
            canSelectMany: false,
            filters: {
                'docker-compose environment file': ['env']
            }
        });

        if (selectedEnvFile === undefined) {
            return Promise.reject(new Error('No docker-compose environment file selected'));
        }

        return selectedEnvFile[0].fsPath;
    }
}