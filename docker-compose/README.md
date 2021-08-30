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

For installation instructions on Linux systems, see [Hints](#Hints)

## Use the example configuration

Just continue with the _Build & Run (from within the ./docker-compose directory)_ section

## Setup custom configuration

- If you want to create a custom configuration, create a config folder at _\<YOUR CONFIG PATH\>_, and copy the following files/folders into it
  - _./mosquitto_
  - _./platform_ (Only needs to be copied, if you want to use _docker-compose.platform.yml_)
  - _./.env_
- Your configuration directories should now look like this

  ```code
  .env                           Copy from docker-compose/.env

  <mosquitto configuration dir>  Copy from docker-compose/mosquitto
  L- config.json

  <platform configuration dir>   Copy from docker-compose/platform
  |- channels
  |  |- talent.channel.json
  |  L- talent.schema.json
  |- config.json
  |- types.json
  L- uom.json
  ```

- You can now adapt the _types.json_ document to fit your needs. Do not make any changes but the `loglevel` to the _config.json_ when you are using `docker_compose`
- The _.env_ file should contain the following and the `PLATFORM_CONFIG_DIR` and the `MOSQUITTO_CONFIG_DIR` should be the absolute paths to _\<YOUR CONFIG PATH\>/platform_ and _\<YOUR CONFIG PATH\>/mosquitto_

  ```code
  DOCKER_HTTP_PROXY=http://host.docker.internal:3128   (optional Proxy configuration)
  DOCKER_HTTPS_PROXY=http://host.docker.internal:3128  (optional Proxy configuration)
  MOSQUITTO_CONFIG_DIR=                                (Path to you mosquitto configuration folder - relative from docker-compose directory or absolute path)
  MQTT_PORT=1883                                       (MQTT port for the local broker)
  PLATFORM_CONFIG_DIR=                                 (Path to you platform configuration folder - relative from docker-compose directory or absolute path. Not needed if you only want to configure Mosquitto)
  API_PORT=8080                                        (Port, which is used to expose the platform REST APIs. Defaults to 8080. Not needed if you only want to configure Mosquitto)
  ```

__The paths within the _.env_ file need to be relative to the _docker-compose_ folder or absolute paths__

Continue with the _Build & Run (from within the ./docker-compose directory with custom configuration)_

## Setup

- If you are behind a corporate proxy, update DOCKER_HTTP_PROXY and DOCKER_HTTPS_PROXY in the _.env_ file.
  - __For further information how to configure and setup the Proxy, please see [here](../docker/README.md)__

## Build & Run (from within the _./docker-compose_ directory)

Platform containers can be build with this command: \
```docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml build```

Platform containers can be run with this command: \
```docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml up```

Mosquitto MQTT broker container can be run and built with this command: \
```docker-compose -f docker-compose.mosquitto.yml up --build```

## Build & Run (from within the _./docker-compose_ directory with custom configuration)

```docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml --env-file <YOUR CONFIG PATH>/.env up --build```

## Build & Run Integration Tests

Build and run sdk test suite containers with the command below. Depending on which test suites are needed (javascript, python, cpp), only the corresponding yml files can be selected: \
```docker-compose -f docker-compose.integration_tests_js.yml -f docker-compose.integration_tests_py.yml -f docker-compose.integration_tests_cpp.yml --env-file .env-test up --build``` 

Build and run the test runner to start integration tests execution: \
```docker-compose -f docker-compose.integration_tests_runner.yml --env-file .env-test up --build```

There are utility scripts in .devcontainer/integration-tests:
* startPlatform.sh: builds and starts the mqtt broker and the platform
* startIntegrationTests.sh: builds and run the test suites and the test runner. When the test runner is done executing
  the test cases, cleanup of the test containers is done.

### Integration Tests .env-test and .env-test-proxy

The integration tests environments file .env-test differs from .env by:
- PLATFORM_CONFIG_DIR=../test/integration-tests/config/platform - contains modified configuration of the platform  
- INTEGRATION_TEST_DIR=/test/integration-tests - contains the sources of the test suites and the runner
- OUTPUT_TEST_DIR=./ - output directory for junit xml report

### Configuration
The configuration files of the integration tests are located in
[../test/integration-tests/config/](../test/integration-tests/config/):
* platform - contains the configuration for the IoT Event Analytics Platform used during the tests
* javascript/runner/config.json and python/runner/config.json contain the configurations of the corresponding
  TestRunners. The description of the properties can be found in [Integration Test Framework doc](../docs/topics/iotea-integration-test-framework.md#Configuration).

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

- Start the containers using `docker-compose -f docker-compose.mosquitto.yml -f docker-compose.platform.yml -f docker-compose.platform.debug.yml --env-file <YOUR CONFIG PATH>/.env up --build`<br>
  This will start NodeJS with inspect-brk option using default Port _9229_ for the pipeline and _9230_ for the ConfigManager.
- Go to the debug view again and select one of the above configurations from the list to attach the debugger to the container. Now you are able to use e.g. breakpoints directly in the source code

## Hints

- If you run the platform containers then please ensure that there is no other MQTT broker (e.g. mosquitto) is running on the same machine on localhost with the same port. Otherwise, configure another port by modifying `MQTT_PORT` in the _.env_ file
- Make sure docker-compose is installed on your system `docker-compose --version`
  - __>> Linux only <<__ To install missing docker-compose<br>
    `sudo apt-get -y -q install docker-compose`<br>
    If docker-compose packgage is missing or too old, directly download the binary<br>

    ```text
    sudo curl -q -L "https://github.com/docker/compose/releases/download/1.27.4/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    sudo chmod +x /usr/local/bin/docker-compose
