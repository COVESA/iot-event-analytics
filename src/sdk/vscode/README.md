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
