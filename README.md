<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics

[![Gitter](https://badges.gitter.im/iot-event-analytics/community.svg)](https://gitter.im/iot-event-analytics/community)
[![License](https://img.shields.io/badge/License-MPL%202.0-blue.svg)](https://opensource.org/licenses/MPL-2.0)
[![Unit tests](https://github.com/GENIVI/iot-event-analytics/actions/workflows/test_js_sdk.yml/badge.svg)](https://github.com/GENIVI/iot-event-analytics/actions/workflows/test_js_sdk.yml)
[![Integration test](https://github.com/GENIVI/iot-event-analytics/actions/workflows/run_integration_test.yml/badge.svg)](https://github.com/GENIVI/iot-event-analytics/actions/workflows/run_integration_test.yml)
[![C++ SDK](https://github.com/GENIVI/iot-event-analytics/actions/workflows/build_and_test_cpp_sdk.yml/badge.svg)](https://github.com/GENIVI/iot-event-analytics/actions/workflows/build_and_test_cpp_sdk.yml)

(c) Bosch.IO GmbH
under [MPL-2.0 licence](https://choosealicense.com/licenses/mpl-2.0/)

## Introduction

IoT Event Analytics, is a complex and freely scalable event processing and agent network platform. The platform is basically build around so called __Talents__ which form a distributed network. Each talent is a unit of work and can interact with other Talents via events. The platform itself orchestrates the flow of events and offers additional services, like an in-memory digital twin, calculation of statistical data based on given event histories and complex rule evaluation of when to inform a Talent, that something happened, that the Talent "is interested in".
Thus, a talent is always an event consumer and may also be an event producer. The event-driven business logic can be implemented using one of our SDKs for JavaScript, Python or C++.
There are more features which can be explored using various examples for the different SDKs.

## Prerequisites

### NodeJS

- Install Node.js >=12.13.0
- Install all dependencies using yarn package manager
  - If yarn is not available on your system, install it using `npm install -g yarn`
    - Please make sure, you use the latest version of yarn
  - Run `yarn` in the project root

### Python

- If using Anaconda to manage your Python environments, create a new one
  - `conda create --name <name-of-choice> python=3.7`
    - Python needs to be at at version >=3.6.8
    - Pick a name of choice
  - `conda activate <name-of-choice>`
- Install all necessary packages using `pip install -r requirements.dev.txt` in the project root

## tldr - get it running - asap

- Install the latest IoT Event Analytics VSCode extension from _src/sdk/vscode/lib/*.vsix_
- Use `Ctrl + Shift + P` to bring up the command palette and select _**Bosch IoT Event Analytics: Create new JavaScript Talent project**_ in an empty folder.
  - Follow the instructions to create a "ready to use" Talent project
- Bring the command palette up again and run _**Bosch IoT Event Analytics: Start platform using docker-compose**_.<br>Select the generated _.env_ file in the folder, which contains your newly created talent project.
- Run your talent using NodeJS by simple run `node index.js` in your newly created talent project.
- Send events to the platform by using the command _**Bosch IoT Event Analytics: Publish an MQTT Message**_
- __Done__

## Project structure

```code
src
| |- core      The platform itself
| |- sdk       The platform software development kits
| |- adapter   Integrations of other systems
|- resources   JSON schemas for object validations
L- docs        Documentation
```

## Build and Run

The recommended way of quickstarting the platform is to use docker-compose where available. If not installed, all components can be built and run manually using the given build instructions under _./docker/README.md_ and _./docker/*/README.md_
For further information how to start the platform using docker-compose see [here](./docker-compose/README.md)

## Run an example/your own Talent on your machine (within the project)

- Simply create your first python or NodeJS talent following the examples given in _src/sdk/(javascript|python|cpp)/examples_<br>
- Start your talent by connecting it to `mqtt://localhost:1883`<br>
- There are examples, which start a platform instance by themselves. They only need an MQTT Broker running. To achieve this, simply run from within the _./docker-compose_ directory<br>
`docker-compose -f docker-compose.mosquitto.yml up`

## Run an example/your own Talent on your machine (Using the JavaScript SDK)

- Create a new directory for your Talent
- Copy `src/sdk/javascript/lib/boschio.iotea-<version>.tgz` into this directory
- Run `npm init` to initialize your NodeJS project
- Install the SDK by typing `yarn add boschio.iotea-<version>.tgz`
  - If you have problems with the installation behind a proxy try the following:

    ```text
    npm config set proxy ${HTTP_PROXY}
    npm config set https-proxy ${HTTPS_PROXY}
    npm config set strict-ssl false -g
    npm config set maxsockets 5 -g
    ```

- Create an _index.js_ file with the following contents

  ```javascript
  const iotea = require('boschio.iotea');

  const Logger = iotea.util.Logger;
  process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.INFO;

  const {
      MqttProtocolAdapter
  } = iotea.util;

  const {
      Talent,
      OpConstraint,
      Rule,
      AndRules,
      ProtocolGateway,
      TalentInput
  } = iotea;

  const {
      VALUE_TYPE_RAW
  } = iotea.constants;

  class MyTalent extends Talent {
      constructor(protocolGatewayConfig) {
          super('some-unique-talent-id', protocolGatewayConfig);
      }

      getRules() {
          return new AndRules([
              new Rule(new OpConstraint('anyfeature', OpConstraint.OPS.ISSET,  null, 'anytype', VALUE_TYPE_RAW))
          ]);
      }

      async onEvent(ev, evtctx) {
          this.logger.info(`${TalentInput.getRawValue(ev)}`, evtctx);
      }
  }

  // Update mqttAdapterConfig.config.brokerUrl, if you specified a different one in your configuration !!!
  const mqttAdapterConfig = MqttProtocolAdapter.createDefaultConfiguration();

  new MyTalent(
      ProtocolGateway.createDefaultConfiguration([ mqttAdapterConfig ])
  ).start();
  ```

- Run `node index.js` to start the test talent.

## Dependencies

- List all NodeJS dependencies on stdout `yarn licenses list`
- List all Python dependencies on stdout `pip-licenses --from=mixed`
  - To have a correct list of packages containing all subpackages needed by this project, make sure that you used a "fresh" environment of conda, where you install the dependencies using `pip install -r requirements.dev.txt`. Otherwise pip will list all packages you have installed in your environment.

## Yarn Scripts

- The jsdoc of iot-event-analytics APIs will be generated in `docs/jsdoc` folder after running `yarn` or `yarn docs.generate`
- To run the unit tests, use `yarn test`
