<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: Scalable Platform

## Prerequisites

- Open the _docker-compose_ folder in the project root in a terminal
- Start a Mosquitto broker by using docker-compose
  - Run ```docker-compose -f docker-compose.mosquitto.yml --env-file ../src/sdk/javascript/examples/scalable_platform/.env up --remove-orphans --build```

## How to start it

- Check the configuration in _config.json_
- Start the platform by running ```node index.js```
