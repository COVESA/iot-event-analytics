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
    getDockerSock,
    getDockerComposeCmd,
    getAndUpdateDockerProxy,
    updateJsonFileAt,
    showProgressWithRuntimePrecheck
} from './util';

import { MqttWebView } from './mqttWebView';

export class IoTeaUtils {
    private static START_MOSQUITTO_BROKER_COMMAND = 'iotea.startMosquittoBroker';
    private static STOP_MOSQUITTO_BROKER_COMMAND = 'iotea.stopMosquittoBroker';
    private static PUBLISH_MQTT_MESSAGE = 'iotea.publishMqttMessage';
    private static MOSQUITTO_BROKER_TERMINAL_NAME = 'IoT Event Analytics Mosquitto Broker';

    private static START_IOTEA_PLATFORM_COMMAND = 'iotea.startIoTeaPlatform';
    private static STOP_IOTEA_PLATFORM_COMMAND = 'iotea.stopIoTeaPlatform';
    private static CREATE_IOTEA_JS_TALENT_CLASS_COMMAND = 'iotea.createJSTalentClass';
    private static CREATE_IOTEA_JS_TALENT_PROJECT_COMMAND = 'iotea.createJsTalentProject';
    private static IOTEA_PLATFORM_TERMINAL_NAME = 'IoT Event Analytics Platform';

    private static YES_OPTION = 'yes';
    private static NO_OPTION = 'no';

    constructor(private ioteaProjectRootDir: string, private progress: vscode.Progress<{ message: string, increment: number }> | null = null) {}

    public static register(context: vscode.ExtensionContext) {
        vscode.window.onDidCloseTerminal(async (terminal: vscode.Terminal) => {
            try {
                if (terminal.name === IoTeaUtils.IOTEA_PLATFORM_TERMINAL_NAME) {
                    await vscode.commands.executeCommand(IoTeaUtils.STOP_IOTEA_PLATFORM_COMMAND, terminal);
                }

                if (terminal.name === IoTeaUtils.MOSQUITTO_BROKER_TERMINAL_NAME) {
                    await vscode.commands.executeCommand(IoTeaUtils.STOP_MOSQUITTO_BROKER_COMMAND, terminal);
                }
            }
            catch(err) {
                vscode.window.showErrorMessage(err.message);
            }
        });

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.PUBLISH_MQTT_MESSAGE, () => {
            return showProgressWithRuntimePrecheck('Loading MQTT publisher', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                const ioteaUtils = new IoTeaUtils(await chooseAndUpdateIoTeaProjectDir(), p);
                MqttWebView.loadOnce(context.extensionPath, ioteaUtils.ioteaProjectRootDir);
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.START_IOTEA_PLATFORM_COMMAND, async (envFile: string | undefined) => {
            return showProgressWithRuntimePrecheck('Starting IoT Event Analytics platform', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                return new IoTeaUtils(await chooseAndUpdateIoTeaProjectDir(), p).startIoTeaPlatform(envFile);
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.STOP_IOTEA_PLATFORM_COMMAND, async (terminal: vscode.Terminal | undefined) => {
            return showProgressWithRuntimePrecheck('Starting IoT Event Analytics platform', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                return new IoTeaUtils(await chooseAndUpdateIoTeaProjectDir(), p).stopIoTeaPlatform(terminal);
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.START_MOSQUITTO_BROKER_COMMAND, async (envFile: string | undefined) => {
            return showProgressWithRuntimePrecheck('Starting Mosquitto MQTT broker', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                return new IoTeaUtils(await chooseAndUpdateIoTeaProjectDir(), p).startIoTeaMqttBroker(envFile);
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.STOP_MOSQUITTO_BROKER_COMMAND, async (terminal: vscode.Terminal | undefined) => {
            return showProgressWithRuntimePrecheck('Stopping Mosquitto MQTT broker', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                return new IoTeaUtils(await chooseAndUpdateIoTeaProjectDir(), p).stopIoTeaMqttBroker(terminal);
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.CREATE_IOTEA_JS_TALENT_PROJECT_COMMAND, async () => {
            return showProgressWithRuntimePrecheck('Creating IoT Event Analytics JavaScript Talent project', async (p: vscode.Progress<{ message: string; increment: number; }>) => {
                return new IoTeaUtils(await chooseAndUpdateIoTeaProjectDir(), p).startCreateIoTeaJsTalentProjectFlow();
            })
            .then(() => {}, err => {
                vscode.window.showErrorMessage(err.message);
            });
        }));

        context.subscriptions.push(vscode.commands.registerCommand(IoTeaUtils.CREATE_IOTEA_JS_TALENT_CLASS_COMMAND, async () => {
            try {
                const uri = vscode.Uri.parse('untitled:talent.js');

                const template = fs.readFileSync(path.resolve(__dirname, '../resources/talent.template.js'), { encoding: 'utf8' });

                const edit = new vscode.WorkspaceEdit();
                edit.insert(uri, new vscode.Position(0, 0), template);
                await vscode.workspace.applyEdit(edit);

                await vscode.window.showTextDocument(uri);
            }
            catch(err) {
                vscode.window.showErrorMessage(err.message);
            }
        }));
    }

    public startIoTeaPlatform(envFile: string | undefined): Promise<void> {
        return this.startComposeInTerminal(IoTeaUtils.IOTEA_PLATFORM_TERMINAL_NAME, 'docker-compose.platform.yml', envFile);
    }

    public stopIoTeaPlatform(terminal: vscode.Terminal | undefined): Promise<void> {
        return this.stopComposeInTerminal(IoTeaUtils.IOTEA_PLATFORM_TERMINAL_NAME, 'docker-compose.platform.yml', terminal);
    }

    public startIoTeaMqttBroker(envFile: string | undefined): Promise<void> {
        return this.startComposeInTerminal(IoTeaUtils.MOSQUITTO_BROKER_TERMINAL_NAME, 'docker-compose.mosquitto.yml', envFile);
    }

    public stopIoTeaMqttBroker(terminal: vscode.Terminal | undefined): Promise<void> {
        return this.stopComposeInTerminal(IoTeaUtils.MOSQUITTO_BROKER_TERMINAL_NAME, 'docker-compose.mosquitto.yml', terminal);
    }

    private startCreateIoTeaJsTalentProjectFlow(): Thenable<void> {
        return vscode.window.showOpenDialog({
            canSelectMany: false,
            canSelectFolders: true,
            canSelectFiles: false,
            title: 'Choose an empty folder for your new IoT Event Analytics Talent project'
        })
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
            // List all available libraries
            const availablePackages = fs.readdirSync(path.resolve(this.ioteaProjectRootDir, 'src/sdk/javascript/lib'));

            const selectedPackage: string | undefined = await vscode.window.showQuickPick(availablePackages, {
                canPickMany: false
            });

            if (selectedPackage === undefined) {
                throw new Error(`You must select an Iot Event Analytics package`);
            }

            this.reportProgress('Installing IoT Event Analytics SDK');

            const localSdkPath = path.resolve(talentProjectDir, selectedPackage);

            try {
                fs.copyFileSync(path.resolve(this.ioteaProjectRootDir, `src/sdk/javascript/lib/${selectedPackage}`), localSdkPath);

                const terminal = new Terminal();

                // Init node project
                await terminal.executeCommand('npm', ['init', '-y'], talentProjectDir);

                // Install SDK
                await terminal.executeCommand('yarn', ['add', `file:${selectedPackage}`], talentProjectDir, message => {
                    this.reportProgress(message);
                });
            }
            finally {
                // Remove SDK installation files
                fs.unlinkSync(localSdkPath);
            }

            return talentProjectDir;
        })
        .then(async (talentProjectDir: string) => {
            // Prepare the directory to have all configuration files, which are needed to start the IoTea platform
            const ioteaProjectRootDir: any = getIoTeaRootDir();
            const mqttPort = vscode.workspace.getConfiguration('iotea').get<number>('platform.mqtt.port');
            const remoteMqttPort = vscode.workspace.getConfiguration('iotea').get<number>('platform.mqtt.remote-port');
            const platformApiPort = vscode.workspace.getConfiguration('iotea').get<number>('platform.api.port');

            // Copy the configuration for the mosquitto brokers in the project directory
            const mosquittoConfigDir = path.resolve(talentProjectDir, 'config/mosquitto');
            copyDirContentsSync(path.resolve(ioteaProjectRootDir, 'docker-compose/mosquitto'), mosquittoConfigDir)

            // Update mosquitto configuration files and apply port configuration
            updateJsonFileAt(path.resolve(mosquittoConfigDir, 'config.json'), {
                'mqtt.port': mqttPort,
                'bridges[0].address': `mosquitto-remote:${remoteMqttPort}`
            });
            updateJsonFileAt(path.resolve(mosquittoConfigDir, 'remote', 'config.json'), {
                'mqtt.port': remoteMqttPort
            });

            // Copy the configurations for the platform into the project directory
            const platformConfigDir = path.resolve(talentProjectDir, 'config/platform');
            copyDirContentsSync(path.resolve(ioteaProjectRootDir, 'docker-compose/platform'), platformConfigDir);

            // Update platform configuration file and apply port configuration
            updateJsonFileAt(path.resolve(platformConfigDir, 'config.json'), {
                'mqtt.connectionString': `mqtt://mosquitto-local:${mqttPort}`,
                'api.port': platformApiPort
            });

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
            envFileContents += `PLATFORM_CONFIG_DIR=${platformConfigDir}${os.EOL}`;
            envFileContents += `MQTT_PORT=${mqttPort}${os.EOL}`;
            envFileContents += `MQTT_REMOTE_PORT=${remoteMqttPort}${os.EOL}`;
            envFileContents += `API_PORT=${platformApiPort}`;

            fs.writeFileSync(path.resolve(talentProjectDir, '.env'), envFileContents, {
                encoding: 'utf8'
            });

            return talentProjectDir;
        })
        .then(async (talentProjectDir: string) => {
            // Start the platform in a new terminal using another command
            if (await vscode.window.showInformationMessage('Do you want to start the IoTea Platform?', IoTeaUtils.YES_OPTION, IoTeaUtils.NO_OPTION) === IoTeaUtils.YES_OPTION) {
                return vscode.commands.executeCommand(IoTeaUtils.START_IOTEA_PLATFORM_COMMAND, path.resolve(talentProjectDir, '.env')).then(() => {});
            }
        });
    }

    private async startComposeInTerminal(terminalName: string, composeFile: string, envFile: string | undefined) {
        const ioteaProjectRootDir: any = getIoTeaRootDir();

        if (envFile === undefined) {
            try {
                envFile = await this.chooseComposeEnvFile();
            }
            catch(err) {
                vscode.window.showErrorMessage(err.message);
                return;
            }
        }

        const env: any = {
            _ENV_FILE: envFile as string
        };

        if (getDockerSock() !== '') {
            env.DOCKER_HOST = getDockerSock();
        }

        const terminal = vscode.window.createTerminal({
            env,
            cwd: path.resolve(ioteaProjectRootDir, 'docker-compose'),
            name: terminalName
        });

        terminal.sendText(`${getDockerComposeCmd()} -f ${composeFile} --project-name vscode-ext --env-file=${envFile} up --build --remove-orphans`);

        terminal.show(false);
    }

    private async stopComposeInTerminal(terminalName: string, composeFile: string, terminal: vscode.Terminal | undefined) {
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
            await this.stopDockerCompose(terminalName, composeFile, await this.chooseComposeEnvFile());
            return;
        }

        for (let terminal of terminals) {
            try {
                const envFile = ((terminal.creationOptions as vscode.TerminalOptions).env as any)._ENV_FILE;
                await this.stopDockerCompose(terminalName, composeFile, envFile);
            }
            finally {
                terminal.dispose();
            }
        }
    }

    private stopDockerCompose(terminalName: string, composeFile: string, envFile: string): Promise<string> {
        this.reportProgress(`Stopping ${terminalName}`);

        const ioteaProjectRootDir: any = getIoTeaRootDir();

        const terminal = new Terminal();

        const env: any = {};

        if (getDockerSock() !== '') {
            env.DOCKER_HOST = getDockerSock();
        }

        return terminal.executeCommand(
            getDockerComposeCmd(),
            ['-f', composeFile, '--project-name', 'vscode-ext', '--env-file', envFile, 'down'],
            path.resolve(ioteaProjectRootDir,'docker-compose'),
            message => { this.reportProgress(message); },
            env
        );
    }

    private async chooseComposeEnvFile(): Promise<string> {
        const selectedEnvFile: vscode.Uri[] | undefined = await vscode.window.showOpenDialog({
            canSelectFiles: true,
            canSelectMany: false,
            filters: {
                'docker-compose environment file': ['env']
            },
            title: 'Select docker-compose .env file.'
        });

        if (selectedEnvFile === undefined) {
            return Promise.reject(new Error('No docker-compose environment file selected'));
        }

        return selectedEnvFile[0].fsPath;
    }

    private reportProgress(message: string, increment: number = 0) {
        if (this.progress === null) {
            console.log(message);
            return;
        }

        this.progress.report({ message, increment });
    }
}