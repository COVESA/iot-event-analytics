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

## Build the Node.js SDK

- Run `yarn build.sdk` from somewhere within the project directory
- The artifact will be saved at _src/sdk/javascript/lib/boschio.iotea-\<version\>.tgz_

## Install

- Copy the artifact (_src/sdk/javascript/lib/boschio.iotea-\<version\>.tgz_) into your application directory
- Goto into your application directory and run
- `npm init`
- `npm install boschio.iotea-<version>.tgz`
