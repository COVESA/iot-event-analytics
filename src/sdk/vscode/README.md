<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics VSCode Extension

## Requirements

Following requirements have to be matched to use this Extension

- Docker >=19.3.13
- docker-compose >= 1.27.4
- Node.js >=12.3.0
- git (CLI) _any version_
- python >= 3.6.8
- pip _any version_

Additionally, all requirements will be checked on the first command invocation automatically.

## Installation

- Go to _View_ > _Extensions_ > _..._ > _Install from VSIX_
- Select the desired version from _lib/iotea-\<version\>.vsix_
- Restart Visual Studio Code

## Settings

- Go to _File_ > _Preferences_ > _Settings_ and type _iotea_ to specify machine-wide settings
- Open extension page to get an overview over all contribution points and available commands

## Command usage

- `Strg + ⇧ + P (Windows, Linux)`, `⇧ + ⌘ + P (macOS)` > type _iotea_ or _vss_ and you will see all available commands

## Talent

### Code

_"Trigger suggest"_, _"Toggle Autocompletion"_ can be triggered using

- Windows, Linux: `Strg + Space`
- macOS:
  - `⌥ + Esc` on Apple Keyboards
  - `Alt + Esc` on Windows Keyboards

#### getRules()

- `iotea.rule.and` + _"Trigger suggest"_ > Create new And Rule
- `iotea.rule.or` + _"Trigger suggest"_ > Create new Or Rule
- `iotea.rule.op` + _"Trigger suggest"_ > Create a new Rule with an Operation Constraint
- `iotea.rule.change` + _"Trigger suggest"_ > Create a new Rule with a Change Constraint

## Troubleshooting

- _"I cannot select an .env file on macOS file open dialog"_<br>
  These files might be hidden from view. Just hit `⌘ + ⇧ + .` to show hidden files in the dialog.
- _"The Docker version check fails"_<br>
  Make sure you have the latest Docker version installed. The docker version is retrieved using: `docker version --format={{.Client.Version}}`. Test in your console, if this command returns the version number for your current docker installation.
- _"The Python version check fails"_<br>
  You can specify the Python interpreter and the pip module in the settings. It defaults to `python3` and `pip3` for standalone installations. If you are running a virtual environment setup based on Anaconda, you have to use `python` and `pip`. On Linux-Systems it can be different. Please check in a terminal, which command to use and update the settings accordingly.
- _"I cannot publish MQTT messages"_<br>
  Make sure you executed `yarn` in the IoT Event Analytics project folder to install all needed dependencies
