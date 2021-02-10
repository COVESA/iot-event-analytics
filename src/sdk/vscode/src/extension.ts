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
import * as bent from 'bent';

import { Terminal } from './terminal';
import {
    copyDirContentsSync,
    chooseAndUpdateIoTeaProjectDir,
    getIoTeaRootDir,
    getAndUpdateDockerProxy
} from './util';

import { MqttWebView } from './mqttWebView';

const IOTEA_PLATFORM_TERMINAL_NAME = 'IoT Event Analytics Platform';
const START_IOTEA_PLATFORM_COMMAND = 'iotea.startIoTeaPlatform';
const STOP_IOTEA_PLATFORM_COMMAND = 'iotea.stopIoTeaPlatform';
const MOSQUITTO_BROKER_TERMINAL_NAME = 'IoT Event Analytics Mosquitto Broker';
const START_MOSQUITTO_BROKER_COMMAND = 'iotea.startMosquittoBroker';
const STOP_MOSQUITTO_BROKER_COMMAND = 'iotea.stopMosquittoBroker';
class TypeFeatureResolver {
    private ttl = -1;
    private typeFeatures: string[] = [];

    async getTypeFeatures() {
        const apiEndpoint = vscode.workspace.getConfiguration('iotea').get('platform.api.endpoint');

        const req = bent('json');

        if (this.typeFeatures.length > 0 && this.ttl > Date.now()) {
            // Has valid data
            return this.typeFeatures;
        }

        this.typeFeatures = [];

        try {
            const types: any = await req(`${apiEndpoint}/types`);

            for (let type in types) {
                if (types[type].features === undefined) {
                    continue;
                }

                for (let feature in types[type].features) {
                    this.typeFeatures.push(`${type}.${feature}`);
                }
            }

            // Fetch again in 10 seconds
            this.ttl = Date.now() + 10000;

            return this.typeFeatures;
        }
        catch(err) {
            return [
                '<type>.<feature>'
            ];
        }
    }
}

class RuleSnippetProvider implements vscode.CompletionItemProvider {
    id: Number;

    constructor(private snippetString: vscode.SnippetString, private triggerString: string, private typeFeatureResolver = new TypeFeatureResolver()) {
        this.id = Math.random();
    }

    async provideCompletionItems(document: vscode.TextDocument, position: vscode.Position, token: vscode.CancellationToken, context: vscode.CompletionContext) {
        const linePrefix = document.lineAt(position).text.substr(0, position.character);

        // Check if line ends with given triggerString
        if (linePrefix.indexOf(this.triggerString) !== linePrefix.length - this.triggerString.length) {
            return undefined;
        }

        const typeFeatures = await this.typeFeatureResolver.getTypeFeatures();

        return typeFeatures.map(typeFeature => {
            const item = new vscode.CompletionItem(typeFeature);

            // Insert nothing
            item.insertText = '';

            // Make it appear in the filter
            item.filterText = `${typeFeature}-${this.triggerString}`;

            // Invoke the command
            item.command = {
                title: '',
                command: `insertRuleOnTypeFeatureSelect-${this.id}`,
                arguments: [ typeFeature, position ]
            };

            return item;
        });
    }

    async onTextEditorCommand(textEditor: vscode.TextEditor, edit: vscode.TextEditorEdit, typeFeature: string, position: vscode.Position) {
        // Save it to the clipboard
        const clipboardBackup = await vscode.env.clipboard.readText();

        await vscode.env.clipboard.writeText(typeFeature);

        // Insert the snippet
        await textEditor.insertSnippet(
            this.snippetString,
            // length of snippet trigger string
            new vscode.Range(new vscode.Position(position.line, position.character - this.triggerString.length), new vscode.Position(position.line, position.character - 1))
        );

        await vscode.env.clipboard.writeText(clipboardBackup);
    }

    register(context: vscode.ExtensionContext, language: string) {
        context.subscriptions.push(
            vscode.languages.registerCompletionItemProvider(language, this, this.triggerString.substring(this.triggerString.length - 1)),
            vscode.commands.registerTextEditorCommand(`insertRuleOnTypeFeatureSelect-${this.id}`, this.onTextEditorCommand.bind(this))
        );
    }
}

export function activate(context: vscode.ExtensionContext) {
    new RuleSnippetProvider(
        new vscode.SnippetString("new Rule(new OpConstraint('${CLIPBOARD/[^\\.]+\\.(.+)/${1}/}', OpConstraint.OPS.${2|ISSET,EQUALS,NEQUALS,LESS_THAN,LESS_THAN_EQUAL,GREATER_THAN,GREATER_THAN_EQUAL,REGEX|}, ${0: null}, '${CLIPBOARD/([^\\.]+).*/${1}/}', ${3|VALUE_TYPE_RAW,VALUE_TYPE_ENCODED|}))"),
        'iotea.rule.op'
    ).register(context, 'javascript');

    new RuleSnippetProvider(
        new vscode.SnippetString("new Rule(new ChangeConstraint('${CLIPBOARD/[^\\.]+\\.(.+)/${1}/}', '${CLIPBOARD/([^\\.]+).*/${1}/}', ${2|VALUE_TYPE_RAW,VALUE_TYPE_ENCODED|}))"),
        'iotea.rule.change'
    ).register(context, 'javascript');

    vscode.window.onDidCloseTerminal(async (terminal: vscode.Terminal) => {
        if (terminal.name === IOTEA_PLATFORM_TERMINAL_NAME) {
            await vscode.commands.executeCommand(STOP_IOTEA_PLATFORM_COMMAND, terminal);
        }

        if (terminal.name === MOSQUITTO_BROKER_TERMINAL_NAME) {
            await vscode.commands.executeCommand(STOP_MOSQUITTO_BROKER_COMMAND, terminal);
        }
    });

    context.subscriptions.push(vscode.commands.registerCommand('iotea.publishMqttMessage', async () => {
        MqttWebView.loadOnce(context.extensionPath, await chooseAndUpdateIoTeaProjectDir());
    }));

    context.subscriptions.push(vscode.commands.registerCommand(START_IOTEA_PLATFORM_COMMAND, async (envFile: string | undefined) => {
        await startIoTeaPlatform(envFile);6
    }));

    context.subscriptions.push(vscode.commands.registerCommand(STOP_IOTEA_PLATFORM_COMMAND, async (terminal: vscode.Terminal | undefined) => {
        await stopIoTeaPlatform(terminal);
    }));

    context.subscriptions.push(vscode.commands.registerCommand(START_MOSQUITTO_BROKER_COMMAND, async (envFile: string | undefined) => {
        await startIoTeaMqttBroker(envFile);
    }));

    context.subscriptions.push(vscode.commands.registerCommand(STOP_MOSQUITTO_BROKER_COMMAND, async (terminal: vscode.Terminal | undefined) => {
        await stopIoTeaMqttBroker(terminal);
    }));

    context.subscriptions.push(vscode.commands.registerCommand('iotea.createJsTalentProject', async () => {
        await chooseAndUpdateIoTeaProjectDir()
            .then(() => vscode.window.showOpenDialog({
                canSelectMany: false,
                canSelectFolders: true,
                canSelectFiles: false,
                title: 'Choose an empty folder for your talent project'
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
                    return vscode.commands.executeCommand('iotea.startIoTeaPlatform', path.resolve(talentProjectDir, '.env'));
                }
            })
            .then(() => {}, async err => {
                return vscode.window.showErrorMessage(err.message);
            });
    }));

    context.subscriptions.push(
        vscode.commands.registerCommand('iotea.createJSTalentClass', async () => {
            const uri = vscode.Uri.parse('untitled:talent.js');

            const template = fs.readFileSync(path.resolve(__dirname, '../resources/talent.template.js'), { encoding: 'utf8' });

            const edit = new vscode.WorkspaceEdit();
            edit.insert(uri, new vscode.Position(0, 0), template);
            await vscode.workspace.applyEdit(edit);

            await vscode.window.showTextDocument(uri);
        })
    );
}

export function deactivate() { }

function startIoTeaPlatform(envFile: string | undefined): Promise<void> {
    return startComposeInTerminal(IOTEA_PLATFORM_TERMINAL_NAME, 'docker-compose.platform.yml', envFile);
}

function stopIoTeaPlatform(terminal: vscode.Terminal | undefined): Promise<void> {
    return stopComposeInTerminal(IOTEA_PLATFORM_TERMINAL_NAME, 'docker-compose.platform.yml', terminal);
}

function startIoTeaMqttBroker(envFile: string | undefined): Promise<void> {
    return startComposeInTerminal(MOSQUITTO_BROKER_TERMINAL_NAME, 'docker-compose.mosquitto.yml', envFile);
}

function stopIoTeaMqttBroker(terminal: vscode.Terminal | undefined): Promise<void> {
    return stopComposeInTerminal(MOSQUITTO_BROKER_TERMINAL_NAME, 'docker-compose.mosquitto.yml', terminal);
}

async function startComposeInTerminal(terminalName: string, composeFile: string, envFile: string | undefined) {
    const ioteaProjectRootDir: any = getIoTeaRootDir();

    if (envFile === undefined) {
        try {
            envFile = await chooseComposeEnvFile();
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

async function stopComposeInTerminal(terminalName: string, composeFile: string, terminal: vscode.Terminal | undefined) {
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
            await stopDockerCompose(terminalName, composeFile, await chooseComposeEnvFile());
        }
        catch(err) {
            vscode.window.showErrorMessage(err.message);
        }

        return;
    }

    for (let terminal of terminals) {
        const envFile = ((terminal.creationOptions as vscode.TerminalOptions).env as any)._ENV_FILE;
        await stopDockerCompose(terminalName, composeFile, envFile);
        terminal.dispose();
    }
}

async function stopDockerCompose(terminalName: string, composeFile: string, envFile: string) {
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

async function chooseComposeEnvFile(): Promise<string> {
    const selectedEnvFile: vscode.Uri[] | undefined = await vscode.window.showOpenDialog({
        canSelectFiles: true,
        canSelectMany: false,
        filters: {
            'Docker-compose env file': ['env']
        }
    });

    if (selectedEnvFile === undefined) {
        return Promise.reject(new Error('No .env file selected'));
    }

    return selectedEnvFile[0].fsPath;
}
