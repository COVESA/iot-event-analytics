<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics VSCode Extension

## Installation

- Go to _View_ > _Extensions_ > _..._ > _Install from VSIX_
- Select the desired version from _lib/iotea-\<version\>.vsix_
- Restart Visual Studio Code

## Settings

- Go to _File_ > _Preferences_ > _Settings_ and type _IoTea_
  - Modify API endpoint by clicking on _Edit in settings.json_ (default: http://localhost:8080/metadata/api/v1)

## Talent

### Create

- `Strg + Shift + p` > _Bosch IoTea Event Analytics: Create new JavaScript Talent_

### Code

#### getRules()

- `iotea.rule.and` + `Strg + Space` > Create new And Rule
- `iotea.rule.or` + `Strg + Space` > Create new Or Rule
- `iotea.rule.op` + `Strg + Space` > Create a new Rule with an Operation Constraint
- `iotea.rule.change` + `Strg + Space` > Create a new Rule with a Change Constraint
