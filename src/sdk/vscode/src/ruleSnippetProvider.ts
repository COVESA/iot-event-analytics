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
import { TypeFeatureResolver } from './typeFeatureResolver';

export class RuleSnippetProvider implements vscode.CompletionItemProvider {
    id: Number;

    static register(context: vscode.ExtensionContext) {
        new RuleSnippetProvider(
            new vscode.SnippetString("new Rule(new OpConstraint('${CLIPBOARD/[^\\.]+\\.(.+)/${1}/}', OpConstraint.OPS.${2|ISSET,EQUALS,NEQUALS,LESS_THAN,LESS_THAN_EQUAL,GREATER_THAN,GREATER_THAN_EQUAL,REGEX|}, ${0: null}, '${CLIPBOARD/([^\\.]+).*/${1}/}', ${3|VALUE_TYPE_RAW,VALUE_TYPE_ENCODED|}))"),
            'iotea.rule.op'
        ).register(context, 'javascript');

        new RuleSnippetProvider(
            new vscode.SnippetString("new Rule(new ChangeConstraint('${CLIPBOARD/[^\\.]+\\.(.+)/${1}/}', '${CLIPBOARD/([^\\.]+).*/${1}/}', ${2|VALUE_TYPE_RAW,VALUE_TYPE_ENCODED|}))"),
            'iotea.rule.change'
        ).register(context, 'javascript');
    }

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