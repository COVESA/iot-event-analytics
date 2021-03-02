<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: MapReduce with local worker

## Prerequisites

- Open the _docker-compose_ folder in the project root in a terminal
- Start a Mosquitto broker by using docker-compose
  - Run `docker-compose -f docker-compose.mosquitto.yml --env-file ../src/sdk/javascript/examples/map_reduce/local/.env up --remove-orphans`

## How to start it

Start it by running `node index.js`

## How to test it

- Open the _tools/mqtt_ in a terminal
- Run `node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/javascript/examples/map_reduce/local/events.txt" --times 1 --delayMs 2000 --transform iotea.ts-now.jna`
