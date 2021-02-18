<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics VSCode Extension

## Prerequisites

- Docker Desktop / Docker for Windows (may require proxy configuration) >=2.4.0.0
  - Docker Engine >= v19.03.13
- docker-compose >= 1.27.4
- Git
- NodeJS >= 12.3.0 (+npm and yarn globally installed) (may require proxy configuration) to run the demo Talent
- VSCode >= 1.51.0
- Clone of IoTEA repository (develop branch) from socialcoding (Will be OSS on GitHub in near future)
  - __Install all dependencies in the IoTEA project folder using `yarn`__, since several commands are using tools, which run directly from source

## Installation

- Go to _View_ > _Extensions_ > _..._ > _Install from VSIX_
- Select the desired version from _lib/iotea-\<version\>.vsix_
- Restart Visual Studio Code

## Settings

- Go to _File_ > _Preferences_ > _Settings_ and type _iotea_ to specify machine-wide settings
- Open extension page to get an overview over all contribution points and available commands

## Command usage

- `Strg + Shift + p` > type iotea and you will see all available commands

## Talent

### Code

#### getRules()

- `iotea.rule.and` + `Strg + Space` > Create new And Rule
- `iotea.rule.or` + `Strg + Space` > Create new Or Rule
- `iotea.rule.op` + `Strg + Space` > Create a new Rule with an Operation Constraint
- `iotea.rule.change` + `Strg + Space` > Create a new Rule with a Change Constraint
