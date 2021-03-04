<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Usage of Docker Compose

## Overview

This folder contains docker-compose configurations (*.yml) for mqtt and platform which builds all needed containers (build) and to run it (up).

## Prerequisites

- Tested with
  - Windows
    - Docker desktop v2.4.0.0
    - Docker Engine v19.03.13
    - Compose: 1.27.4
  - Ubuntu 18.04.5
    - Docker Engine v19.03.6
    - Compose 1.27.4

## Setup custom configuration

If you want to create a custom configuration, create a config folder at _\<YOUR CONFIG PATH\>_, and copy the following files/folders into it

- ./_mosquitto_
- ./_platform_ (Only needs to be copied, if you want to use _docker-compose.platform.yml_)
- ./.env

Adapt the configuration files as needed and update the `PLATFORM_CONFIG_DIR` and the `MOSQUITTO_CONFIG_DIR` variable within the copied _.env_ file

- _PLATFORM_CONFIG_DIR_ needs to point to _\<YOUR CONFIG PATH\>/platform_
- _MQTT_CONFIG_DIR_ needs to point to _\<YOUR CONFIG PATH\>/mosquitto_

__The paths within the _.env_ file need to be relative to the _docker-compose_ folder or absolute paths__

## Setup

- If you are behind a corporate proxy, update DOCKER_HTTP_PROXY and DOCKER_HTTPS_PROXY in the _.env_ file.
  - __For further information how to configure and setup the Proxy, please see [here](../docker/README.md)__

## Build & Run (from within the _./docker-compose_ directory)

Platform containers can be build with this command: \
```docker-compose -f docker-compose.platform.yml build```

Platform containers can be run with this command: \
```docker-compose -f docker-compose.platform.yml up```

Mosquitto MQTT broker containers (local and local-remote) can be run and built with this command: \
```docker-compose -f docker-compose.mosquitto.yml up --build```

## Build & Run (from within the _./docker-compose_ directory with custom environment configuration)

```docker-compose -f docker-compose.mosquitto.yml --env-file <YOUR CONFIG PATH>/.env up --build```

## Debug

- Install the Docker Extension for VSCode
  - As an alternative without using VSCode, use the Chrome debugger and open the URL [chrome://inspect/#devices](chrome://inspect/#devices) in your Browser and select the matching Target
- Go to the debugging view and click on the `gears` icon. This brings up the launch.json file
  - Click on `Add Configuration...` and select `Docker: Attach to node` from the list
  - Create the following configurations<br>

  ```json
  {
      "type": "node",
      "request": "attach",
      "protocol": "inspector",
      "name": "Attach to IoT Event Analytics Pipeline Docker Container",
      "port": 9229,
      "address": "localhost",
      "localRoot": "${workspaceFolder}",
      "remoteRoot": "/app"
  },
  {
      "type": "node",
      "request": "attach",
      "protocol": "inspector",
      "name": "Attach to IoT Event Analytics ConfigManager Docker Container",
      "port": 9230,
      "address": "localhost",
      "localRoot": "${workspaceFolder}",
      "remoteRoot": "/app"
  }
  ```

- Start the containers using `docker-compose -f docker-compose.platform.yml -f docker-compose.platform.debug.yml --env-file <YOUR CONFIG PATH>/.env up --build`<br>
  This will start NodeJS with inspect-brk option using default Port _9229_ for the pipeline and _9230_ for the ConfigManager.
- Go to the debug view again and select one of the above configurations from the list to attach the debugger to the container. Now you are able to use e.g. breakpoints directly in the source code

## Hints

If you run the platform containers then please ensure that there is no other MQTT broker (e.g. mosquitto) is running on the same machine on localhost with the same port. Otherwise, configure another port by addint these lines to the env file

```text
MQTT_PORT=1883
MQTT_REMOTE_PORT=1884
```

Append the following line, if you would like to use another port for the IoT Event Analytics API

```text
API_PORT=8080
```
