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

import { Terminal } from './terminal';

export class MqttWebView {
    static instance: MqttWebView | null = null;
    wvPanel: vscode.WebviewPanel;

    constructor(private extensionPath: string, private ioteaProjectRootDir: string) {
        this.wvPanel = vscode.window.createWebviewPanel(
            'iotea.www.mqtt',
            'MQTT Publisher',
            vscode.ViewColumn.One,
            {
                enableScripts: true,
                retainContextWhenHidden: true,
                localResourceRoots: [
                    vscode.Uri.file(path.join(extensionPath, 'resources/www'))
                ]
            }
        );

        this.wvPanel.webview.onDidReceiveMessage(async (msg) => {
            if (msg.broker === '' || msg.topic === '' || msg.message === '') {
                await this.postMessageToUi({ action: 'publish', success: false, error: 'Broker, topic and message have to be specified' });
                return;
            }

            const terminal = new Terminal();

            try {
                // Put message in quotation marks
                await terminal.executeCommand('node', ['cli.js', 'pub', '-c', msg.broker, '-t', msg.topic, '-m', `"${msg.message}"`], path.resolve(this.ioteaProjectRootDir, 'src', 'tools', 'mqtt'), (msg: string) => {
                    console.log(msg);
                });

                await this.postMessageToUi({ action: 'publish', success: true });
            }
            catch(err) {
                await this.postMessageToUi({ action: 'publish', success: false, error: err.message });
            }
        });

        // Remove the instance on destroying the panel
        this.wvPanel.onDidDispose(() => {
            MqttWebView.instance = null;
        });

        let html = fs.readFileSync(path.resolve(__dirname, '../resources/www/mqtt/index.html'), { encoding: 'utf8' });

        html = html.replace('{{EXT_BASE_SCRIPT}}', this.getExtResourceUriString('www/ext.js'))
                   .replace('{{EXT_BASE_CSS}}', this.getExtResourceUriString('www/ext.css'))
                   .replace('{{EXT_SCRIPT}}', this.getExtResourceUriString('www/mqtt/mqtt.js'))
                   .replace('{{EXT_CSS}}', this.getExtResourceUriString('www/mqtt/mqtt.css'));

        // Inject configurations from platform
        html = html.replace('{{CONSTANTS}}', `
            const VSS_PATH_SEPARATOR = '${vscode.workspace.getConfiguration('iotea').get('vss.path.separator')}';
            const VSS_PATH_REPLACER = ${JSON.stringify(vscode.workspace.getConfiguration('iotea').get('vss.path.replacer'))};`);

        // Load HTML page
        this.wvPanel.webview.html = html;
    }

    async postMessageToUi(msg: any) {
        await this.wvPanel.webview.postMessage(msg);
    }

    getExtResourceUriString(resourcePath: string): string {
        return this.wvPanel.webview.asWebviewUri(vscode.Uri.file(path.join(this.extensionPath, 'resources', ...resourcePath.split('/')))).toString();
    }

    static loadOnce(extensionPath: string, ioteaProjectRootDir: string) {
        if (MqttWebView.instance === null) {
            MqttWebView.instance = new MqttWebView(extensionPath, ioteaProjectRootDir);
        }
    }
}