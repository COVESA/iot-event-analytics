<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Examples

## Prerequisites

- Open the _docker-compose_ folder in the project root in a terminal and start the platform and the local broker by using docker-compose
  - If there is __no__ _.env_ file within the example folder
    `docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml up --remove-orphans --build`
  - If there is _.env_ in the folder, run
    `docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml --env-file ../src/sdk/python/examples/<example>/.env up --remove-orphans`

## How to run the examples

- Execute _run.py_ in the respective folder

## How to test it

- If there is an _events.txt_ within the example folder
  - Open the _tools/mqtt_ in a terminal
  - Run `node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/python/examples/<example>/events.txt" --times 1 --delayMs 2000 --transform iotea.ts-now.jna`
