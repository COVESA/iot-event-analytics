<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: Bridging between different Protocol Adapters

## How to start it

- Open the _docker-compose_ folder in the project root in a terminal
  - Start the platform and the MQTT broker by using docker-compose `docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml --project-name=platform --env-file ../src/sdk/javascript/examples/protocols/.env.platform up --remove-orphans --build`<br>
  - Start the platform and the MQTT broker by using docker-compose `docker-compose -f docker-compose.mosquitto.yml --project-name=broker --env-file ../src/sdk/javascript/examples/protocols/.env.broker up --remove-orphans --build`
- Start the talent by running `node index.js`

## How to test it

- Open _tools/mqtt_ in a terminal
- Run `node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/javascript/examples/console/cloud/events.txt" --times 1 --delayMs 2000 --transform iotea.ts-now.jna`
