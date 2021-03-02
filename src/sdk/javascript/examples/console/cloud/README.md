<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: Cloud Console Output

## Prerequisites

- Convert the cloudTalent.js using the t2c CLI
  - Open _sdk/javascript/t2c_ in a terminal
  - Run `node t2c.js -f "../examples/console/cloud/cloudTalent.js" -a "mqtt://localhost:1884:string" -p aws -o "../examples/console/cloud/cloudTalentAWS.js"`
  - Deploy the function to AWS as described in the t2c README.md
    - __The TalentID is "my-new-cloud-talent"__

- Open _config/mosquitto/config.json_ and update the authentication settings and the endpoint URL to match your AWS configuration

## How to start it

- Open the _docker-compose_ folder in the project root in a terminal and start the platform and the local broker by using docker-compose `docker-compose -f docker-compose.platform.yml --env-file ../src/sdk/javascript/examples/console/cloud/.env up --remove-orphans`
- Start the local cloud output talent by running `node index.js`

## How to test it

- Open _tools/mqtt_ in a terminal
- Run `node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/javascript/examples/console/cloud/events.txt" --times 1 --delayMs 2000 --transform iotea.ts-now.jna`
