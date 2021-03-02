<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: Agent Network CarAPI

## Prerequisites

- Open the _docker-compose_ folder in the project root in a terminal and start the platform and the local broker by using docker-compose `docker-compose -f docker-compose.platform.yml up --remove-orphans`
- Follow the guide in this [README.md](../../../../python/examples/integrations/agent-network/README.md) for running the full example

## How to start it

- Run `node index.js`

## How to test it

- Run `node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/javascript/examples/integrations/agent-network/events.txt" --times 1 --delayMs 2000 --transform iotea.ts-now.jna`
