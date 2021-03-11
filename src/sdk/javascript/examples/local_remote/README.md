<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Local "RemoteTalent" example

## How to start it

- Open the _docker-compose_ folder in the project root in a terminal and start the platform and the local broker by using docker-compose `docker-compose -f docker-compose.platform.yml --env-file ../src/sdk/javascript/examples/local_remote/.env up --remove-orphans`
- Start the local cloud output talent by running `node index.js`

## How to test it

- Open _tools/mqtt_ in a terminal
- If you specified a namespace, make sure you prefix it accordingly
- Run `node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/javascript/examples/local_remote/events.txt" --times 1 --delayMs 2000 --transform iotea.ts-now.jna`
