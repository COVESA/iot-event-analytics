<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics VSCode Extension

## Prerequisites

All requirements will be checked on each command invocation

## Installation

- Go to _View_ > _Extensions_ > _..._ > _Install from VSIX_
- Select the desired version from _lib/iotea-\<version\>.vsix_
- Restart Visual Studio Code

## Settings

- Go to _File_ > _Preferences_ > _Settings_ and type _iotea_ to specify machine-wide settings
- Open extension page to get an overview over all contribution points and available commands

## Command usage

- `Strg + Shift + P` > type _iotea_ or _vss_ and you will see all available commands

## Talent

### Code

#### getRules()

- `iotea.rule.and` + `Strg + Space` > Create new And Rule
- `iotea.rule.or` + `Strg + Space` > Create new Or Rule
- `iotea.rule.op` + `Strg + Space` > Create a new Rule with an Operation Constraint
- `iotea.rule.change` + `Strg + Space` > Create a new Rule with a Change Constraint

## Troubleshooting

- _"Docker version check fails"_<br>
  Make sure you have the latest Docker version installed, since `docker version --format=json` is used to retrieve the version. In older versions the string "json" was returned
- _"Python version check fails"_<br>
  You can specify the Python interpreter and the pip module in the settings. It defaults to `python3` and `pip3` for standalone installations. If you are running a virtual environment setup based on Anaconda, you have to use `python` and `pip`. On Linux-Systems it can be different. Please check in a terminal, which command to use and update the settings accordingly.
- _"Cannot validate installed modules"_
