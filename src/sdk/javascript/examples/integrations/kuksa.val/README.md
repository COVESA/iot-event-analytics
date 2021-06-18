<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: Bidirectional integration of Kuksa.val

This example shows, how to subscribe and update datapoints in Kuksa.VAL

It contains the following steps:

- `Vehicle.Acceleration.Longitudinal` is published to Kuksa.VAL periodically in _index.js_ in an infinite loop
- The Talent defined in _index.js_ subscribes on this VSS data point
- The Kuksa.val2iotea adapter subscribes at Kuksa.VAL for this data point and receives updates from Kuksa.VAL and ingests it to the IoT Event Analytics platform
- The Talent reacts on the incoming event and updates `Vehicle.Acceleration.Lateral` as output in the IoT Event Analytics Platform
- The Kuksa.val2iotea adapter forwards this updated value to Kuksa.VAL, where it is finally updated
- The examples subscribes at Kuksa.VAL for the data point `Vehicle.Acceleration.Lateral` and outputs a message, once an update was recieved<br>
  `2021-06-17T08:07:16.127Z    INFO [KuksaValDemo] : Received <some value> from Vehicle.Acceleration.Lateral`

## Prerequisites

- Download kuksa.val Docker image from [here](https://ci.eclipse.org/kuksa/job/kuksa.val/job/master) and load it using<br>
`docker image load --input kuksa-val-*-amd64.tar.xz`
  - Look at the tag, which is displayed, if the image was successfully loaded
  - Update the value _KUKSA_VAL_IMG_ in the .env file with the corresponding version
- Follow this [manual](./config/kuksa.val/README.md) to create your kuksa.val configuration in the _./config/kuksa.val_ folder
- Update the JWT for authentication against Kuksa.val in these configuration files: _./config.json_, _./config/kuksa.val2iotea/config.json_

## How to start it

- Open the _docker-compose_ folder in the project root in a terminal and start the whole environment using docker-compose<br>
  `docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml -f ../src/sdk/javascript/examples/integrations/kuksa.val/docker-compose.kuksa.val2iotea.yml --env-file ../src/sdk/javascript/examples/integrations/kuksa.val/.env --project-name kuksa-val-stack up --remove-orphans --build`
- Start the actual example it by running ```node index.js```<br />
  You have to wait until the talent is successfully registered at the platform until it receives the first values
