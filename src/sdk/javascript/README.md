<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# npm Package

## Prepare

- Be sure to include all library relevant exports into the _../../modules.js_ file

## Build

- Go into the _./lib_ folder

### Build distribution package

- Run `npm pack ../../../..`

## Install

- Goto into your application directory and run
- `npm init`
- `npm install boschio.iotea-<version>.tgz`
