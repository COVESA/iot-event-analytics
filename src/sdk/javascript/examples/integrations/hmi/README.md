<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Example: WebApp Application integration

## Prerequisites

- Create a _lib_ folder inside _./www_ and copy _jquery.knob.min.js_ from [https://github.com/aterrien/jQuery-Knob/tree/master/dist](https://github.com/aterrien/jQuery-Knob/tree/master/dist) into this directory
- Open the _docker-compose_ folder in the project root in a terminal
- Start a Mosquitto broker by using docker-compose
  - Run ```docker-compose -f docker-compose.mosquitto.yml up --remove-orphans```

## How to start it

Start it by running ```node index.js```

## How to test it

- Your browser window will automatically open on http://localhost:8080
