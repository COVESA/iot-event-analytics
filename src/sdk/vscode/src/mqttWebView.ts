import * as fs from 'fs';
import * as path from 'path';
import * as vscode from 'vscode';

import { Terminal } from './terminal';

export class MqttWebView {
    static instance: MqttWebView | null = null;
    wwPanel: vscode.WebviewPanel;

    constructor(private extensionPath: string, private ioteaProjectRootDir: string) {
        this.wwPanel = vscode.window.createWebviewPanel(
            'iotea.www.mqtt',
            'MQTT Publisher',
            vscode.ViewColumn.One,
            {
                enableScripts: true,
                localResourceRoots: [
                    vscode.Uri.file(path.join(extensionPath, 'resources/www'))
                ]
            }
        );

        this.wwPanel.webview.onDidReceiveMessage(async (msg) => {
            if (msg.broker === '' || msg.topic === '' || msg.message === '') {
                this.wwPanel.webview.postMessage({ action: 'publish', success: false, error: 'Broker, topic and message have to be specified' });
                return;
            }

            const terminal = new Terminal();

            try {
                await terminal.executeCommand('node', ['cli.js', 'pub', '-c', msg.broker, '-t', msg.topic, '-m', msg.message], path.resolve(ioteaProjectRootDir, 'src', 'tools', 'mqtt'), (msg: string) => {
                    console.log(msg);
                });

                this.wwPanel.webview.postMessage({ action: 'publish', success: true });
            }
            catch(err) {
                this.wwPanel.webview.postMessage({ action: 'publish', success: false, error: err.message });
            }
        });

        // Remove the instance on destroying the panel
        this.wwPanel.onDidDispose(() => {
            MqttWebView.instance = null;
        });

        let html = fs.readFileSync(path.resolve(__dirname, '../resources/www/mqtt/index.html'), { encoding: 'utf8' });

        html = html.replace('{{EXT_BASE_SCRIPT}}', this.getExtResourceUriString('www/ext.js'))
                   .replace('{{EXT_BASE_CSS}}', this.getExtResourceUriString('www/ext.css'))
                   .replace('{{EXT_SCRIPT}}', this.getExtResourceUriString('www/mqtt/mqtt.js'))
                   .replace('{{EXT_CSS}}', this.getExtResourceUriString('www/mqtt/mqtt.css'));

        // Load HTML page
        this.wwPanel.webview.html = html;
    }

    getExtResourceUriString(resourcePath: string): string {
        return this.wwPanel.webview.asWebviewUri(vscode.Uri.file(path.join(this.extensionPath, 'resources', ...resourcePath.split('/')))).toString();
    }

    static loadOnce(extensionPath: string, ioteaProjectRootDir: string) {
        if (MqttWebView.instance === null) {
            MqttWebView.instance = new MqttWebView(extensionPath, ioteaProjectRootDir);
        }
    }
}