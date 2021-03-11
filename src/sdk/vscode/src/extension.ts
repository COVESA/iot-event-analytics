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
import { VssUtils } from './vssUtils';
import { IoTeaUtils } from './ioteaUtils';
import { RuleSnippetProvider } from './ruleSnippetProvider';

export function activate(context: vscode.ExtensionContext) {
    IoTeaUtils.register(context);
    VssUtils.register(context);
    RuleSnippetProvider.register(context);
}

export function deactivate() { }